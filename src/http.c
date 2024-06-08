/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

static void http_peer_conn_test(void *, void *);
static void http_peer_reconnect(struct conn *);
static void http_peer_init(struct conn *);
static void http_peer_send(void *, void *);
static void http_peer_read(void *, void *);
static void http_peer_header_read(struct conn *);
static void http_peer_header_parse(struct conn *);
static void http_peer_process(struct conn *);
static void http_peer_body_read(struct conn *);
static void http_peer_done(struct conn *);
static void http_peer_timeout(void *obj, void *data);


int
http_header_parse(http_field *field, char *header)
{
    char *p;

    p = strchr(header, ':');
    if (p == NULL || p == header) {
        return -1;
    }

    field->name = header;
    field->name_length = p - header;

    do {
        p++;
    } while (*p == ' ');

    field->value = p;
    field->value_length = strlen(p);

    return 0;
}


void
http_peer_connect(struct conn *c)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct addrinfo *addr = cfg.addr;
    int fd, flags;

    fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (fd == -1) {
        return;
    }

    c->socket.fd = fd;

    flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    if (connect(fd, addr->ai_addr, addr->ai_addrlen) == -1) {
        if (errno != EINPROGRESS) {
            goto error;
        }
    }

    flags = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));

    flags = EVENT_READ | EVENT_WRITE;
    c->socket.read_handler = http_peer_conn_test;
    c->socket.write_handler = http_peer_conn_test;
    c->socket.data = c;

    if (epoll_create_event(engine, &c->socket, flags)) {
        goto error;
    }

    return;

error:
    engine->status->connect_errors++;
    close(fd);
}


static void
http_peer_conn_test(void *obj, void *data)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct conn *c = obj;
    socklen_t len;
    int ret, err;

    err = 0;
    len = sizeof(int);
    ret = getsockopt(c->socket.fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len);
    if (ret || err) {
        goto fail;
    }

    c->socket.write_handler = http_peer_send;
    c->socket.read_handler = http_peer_read;
    c->timer.handler = http_peer_timeout;

    http_peer_init(c);
    return;

fail:
    engine->status->connect_errors++;
}


static void
http_peer_reconnect(struct conn *c)
{
    struct thread *thr = cur_thread();

    epoll_delete_event(thr->engine, &c->socket, EVENT_WRITE | EVENT_READ);
    close(c->socket.fd);
    http_peer_connect(c);
}


static void
http_peer_init(struct conn *c)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct buf *request;

    c->read->free = c->read->start;
    c->read->pos = c->read->start;
    c->read_handler = http_peer_header_read;

    c->start = thr->time;
    timer_add(engine, &c->timer, cfg.timeout / 1000);

    if (thr->has_request) {
        lua_getglobal(thr->lua, "http");
        lua_getfield(thr->lua, -1, "request");
        lua_call(thr->lua, 0, 1);
        request = script_request(thr->lua);
        lua_pop(thr->lua, 2);

        if (c->write != NULL) {
            zfree(c->write);
        }
        c->write = request;

    } else {
        c->write->pos = c->write->start;
    }

    epoll_create_event(engine, &c->socket, EVENT_WRITE);
}


static void
http_peer_send(void *obj, void *data)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct conn *c = obj;

    switch (conn_write(c)) {
    case OK: break;
    case ERROR: goto error;
    case RETRY: return;
    }

    if (c->write->pos == c->write->free) {
        epoll_delete_event(engine, &c->socket, EVENT_WRITE);
    }

    return;

error:
    engine->status->write_errors++;
    http_peer_reconnect(c);
}


static void
http_peer_read(void *obj, void *data)
{
    struct thread *thr = cur_thread();
    struct event_engine *engine = thr->engine;
    struct conn *c = obj;
    size_t n;

    switch (conn_read(c, &n)) {
    case OK: break;
    case ERROR: goto error;
    case RETRY: return;
    }

    if (n > 0) {
        engine->status->bytes += n;
        c->read_handler(c);
        return;
    }

    if (c->read_handler == http_peer_header_read) {
        http_peer_reconnect(c);
        return;
    }

error:
    engine->status->read_errors++;
    http_peer_reconnect(c);
}


static void
http_peer_header_read(struct conn *c)
{
    memset(&c->parser, 0, sizeof(c->parser));
    memset(&c->chunk_parser, 0, sizeof(c->chunk_parser));
    http_parse_init(&c->parser);
    http_peer_header_parse(c);
}


static void
http_peer_header_parse(struct conn *c)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    int ret;

    ret = http_parse_response(&c->parser, c->read);

    switch (ret) {
    case DONE:
        http_peer_process(c);
        return;

    case RETRY:
        if (c->read->free < c->read->end) {
            c->read_handler = http_peer_header_parse;
            return;
        }
        /* fall through */
    default:
        break;
    }

    engine->status->read_errors++;
    http_peer_reconnect(c);
}


static void
http_peer_process(struct conn *c)
{
    http_parser *parser = &c->parser;

    if (parser->content_length_n <= 0 && !parser->chunked) {
        http_peer_done(c);
        return;
    }

    c->remainder = parser->chunked ? 1 : parser->content_length_n;

    if (c->read->free > c->read->pos) {
        http_peer_body_read(c);

    } else {
        c->read_handler = http_peer_body_read;

        c->read->free = c->read->start;
        c->read->pos = c->read->start;

        if (c->socket.read_ready) {
            http_peer_read(c, NULL);
        }
    }
}


static void
http_peer_body_read(struct conn *c)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    size_t size;
    struct buf *b, *out, *next;
    http_parser *parser;

    parser = &c->parser;

    if (parser->chunked) {
        out = http_parse_chunk(&c->chunk_parser, c->read);
        if (c->chunk_parser.chunk_error || c->chunk_parser.error) {
            goto error;
        }

        for (b = out; b != NULL; b = next) {
            next = b->next;
            zfree(b);
        }

        if (c->chunk_parser.last) {
            c->remainder = 0;
        }

    } else {
        size = c->read->free - c->read->pos;
        size = min_int(size, c->remainder);

        c->remainder -= size;
        c->read->pos += size;
    }

    if (c->remainder > 0) {
        c->read_handler = http_peer_body_read;
        c->read->free = c->read->start;
        c->read->pos = c->read->start;

        if (c->socket.read_ready) {
            http_peer_read(c, NULL);
        }
        return;
    }

    http_peer_done(c);
    return;

error:
    engine->status->read_errors++;
    http_peer_reconnect(c);
}


static void
http_peer_done(struct conn *c)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct status *status = engine->status;;
    uint64_t elapsed_us;
    http_parser *parser;

    parser = &c->parser;

    elapsed_us = (thr->time - c->start) / 1000;
    if (elapsed_us <= cfg.timeout) {
        hdr_record_value(status->latency, elapsed_us);
    }

    timer_remove(engine, &c->timer);

    if (parser->keepalive) {
        http_peer_init(c);
    } else {
        http_peer_reconnect(c);
    }
}


static void
http_peer_timeout(void *obj, void *data)
{
    struct thread *thr = cur_thread();
    struct timer *timer = obj;
    struct conn *c = container_of(timer, struct conn, timer);

    thr->engine->status->timeouts++;
    http_peer_reconnect(c);
}
