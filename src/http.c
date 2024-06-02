#include "headers.h"

static void http_peer_reconnect(struct conn *);
static void http_peer_connected(void *, void *);
static void http_peer_send(void *, void *);
static void http_peer_read(void *, void *);
static void http_peer_init(struct conn *);
static void http_peer_header_parse(struct conn *);
static void http_peer_process(struct conn *);
static void http_peer_body_read(struct conn *);
static void http_peer_done(struct conn *);
static void http_peer_timeout(void *obj, void *data);


struct buf *
http_request_create()
{
    char *p;
    size_t size;
    struct buf *b;

    size = strlen("GET ") + strlen(cfg.target) + strlen(" HTTP/1.1\r\n");
    size += strlen("HOST: localhost\r\n") + 2;

    b = buf_alloc(size);
    if (b == NULL) {
        return NULL;
    }

    p = cpymem(b->free, "GET ", strlen("GET "));
    p = cpymem(p, cfg.target, strlen(cfg.target));
    p = cpymem(p, " HTTP/1.1\r\n", strlen(" HTTP/1.1\r\n"));
    p = cpymem(p, "HOST: localhost\r\n", strlen("HOST: localhost\r\n"));
    *p++ = '\r'; *p++ = '\n';

    b->free = p;

    return b;
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
    c->socket.read_handler = http_peer_connected;
    c->socket.write_handler = http_peer_connected;
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
http_peer_reconnect(struct conn *c)
{
    struct thread *thr = cur_thread();

    epoll_delete_event(thr->engine, &c->socket, EVENT_WRITE | EVENT_READ);
    close(c->socket.fd);
    http_peer_connect(c);
}


static void
http_peer_connected(void *obj, void *data)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct conn *c = obj;

    c->start = 0;

    c->read->free = c->read->start;
    c->read->pos = c->read->start;
    c->write->pos = c->write->start;

    c->socket.write_handler = http_peer_send;
    c->socket.read_handler = http_peer_read;
    c->read_handler = http_peer_init;

    epoll_create_event(engine, &c->socket, EVENT_READ);
    epoll_create_event(engine, &c->socket, EVENT_WRITE);
}


static void
http_peer_send(void *obj, void *data)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct conn *c = obj;

    if (c->start == 0) {
        c->start = thr->time;
        c->timer.handler = http_peer_timeout;
        timer_add(engine, &c->timer, cfg.timeout / 1000);
    }

    switch (conn_write(c)) {
    case OK: break;
    case ERROR: goto error;
    case RETRY: return;
    }

    if (c->write->pos == c->write->free) {
        c->write->pos = c->write->start;
        epoll_delete_event(engine, &c->socket, EVENT_WRITE);
    }

    return;

error:
    engine->status->write_errors++;
    http_peer_reconnect(c);
}


static void
http_conn_read(struct conn *c)
{
    struct thread *thr = cur_thread();
    struct event_engine *engine = thr->engine;
    size_t n;

    if (!c->socket.read_ready) {
        return;
    }

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

    if (c->read_handler == http_peer_init) {
        http_peer_reconnect(c);
        return;
    }

error:
    engine->status->read_errors++;
    http_peer_reconnect(c);
}


static void
http_peer_read(void *obj, void *data)
{
    struct conn *c = obj;

    c->socket.read_ready = 1;
    http_conn_read(c);
}


static void
http_peer_init(struct conn *c)
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

        http_conn_read(c);
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

        http_conn_read(c);
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

    c->start = 0;
    timer_remove(engine, &c->timer);

    if (parser->keepalive) {
        c->socket.write_handler = http_peer_send;
        epoll_create_event(engine, &c->socket, EVENT_WRITE);

        if (c->read->free > c->read->pos) {
            http_peer_init(c);

        } else {
            c->read_handler = http_peer_init;
            c->read->free = c->read->start;
            c->read->pos = c->read->start;

            http_conn_read(c);
        }

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
