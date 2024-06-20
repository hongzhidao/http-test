/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

static int unix_connected(struct conn *, char *);
static ssize_t unix_recv(struct conn *, void *, size_t);
static ssize_t unix_send(struct conn *, void *, size_t);
static void unix_close(struct conn *);

conn_io unix_conn_io = {
    .connected = unix_connected,
    .recv = unix_recv,
    .send = unix_send,
    .close = unix_close,
};


void
conn_connect(struct conn *c, struct addrinfo *addr)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    int fd, flags;

    fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (fd == -1) {
        return;
    }

    c->socket.fd = fd;

    flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    flags = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));

    if (connect(fd, addr->ai_addr, addr->ai_addrlen) == -1) {
        if (errno != EINPROGRESS) {
            goto error;
        }
    }

    flags = EVENT_READ | EVENT_WRITE;
    if (epoll_add_event(engine, &c->socket, flags)) {
        goto error;
    }

    return;

error:
    engine->status->connect_errors++;
    close(fd);
}


void
conn_connected(struct conn *c, char *host)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    int ret;

    ret = c->io->connected(c, host);

    if (ret == OK) {
        c->socket.write_handler = conn_write;
        c->socket.read_handler = conn_read;
        c->read_handler(c, NULL);
        return;
    }

    if (ret != RETRY) {
        engine->status->connect_errors++;
        return;
    }
}


void
conn_read(void *obj, void *data)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct conn *c = obj;
    size_t size;
    ssize_t n;

    size = c->read->end - c->read->free;
    n = c->io->recv(c, c->read->free, size);

    if (n > 0) {
        c->read->free += n;
        engine->status->bytes += n;
        c->read_handler(c, NULL);
        return;
    }

    if (n == 0) {
        c->close_handler(c, NULL);
        return;
    }

    if (n != RETRY) {
        engine->status->read_errors++;
        c->error_handler(c, NULL);
    }
}


void
conn_write(void *obj, void *data)
{
    struct thread *thr = cur_thread();
    event_engine *engine = thr->engine;
    struct conn *c = obj;
    size_t size;
    ssize_t n;

    while (c->write->pos < c->write->free) {
        size = c->write->free - c->write->pos;
        n = c->io->send(c, c->write->pos, size);

        if (n > 0) {
            c->write->pos += n;
            continue;
        }

        if (n != RETRY) {
            engine->status->write_errors++;
            c->error_handler(c, NULL);
            return;
        }

        epoll_add_event(thr->engine, &c->socket, EVENT_WRITE);
        return;
    }

    epoll_delete_event(thr->engine, &c->socket, EVENT_WRITE);
}


void
conn_close(struct conn *c)
{
    struct thread *thr = cur_thread();

    epoll_delete_event(thr->engine, &c->socket, EVENT_WRITE | EVENT_READ);
    close(c->socket.fd);
    c->io->close(c);
}


static int
unix_connected(struct conn *c, char *host)
{
    int ret, err;
    socklen_t len;

    err = 0;
    len = sizeof(int);

    ret = getsockopt(c->socket.fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len);
    if (ret || err) {
        return ERROR;
    }

    return OK;
}


static ssize_t
unix_recv(struct conn *c, void *buf, size_t size)
{
    ssize_t n;

    for (;;) {
        n = read(c->socket.fd, c->read->free, size);

        if (n > 0) {
            if (n < size) {
                c->socket.read_ready = 0;
            }
            return n;
        }

        if (n == 0) {
            return 0;
        }

        switch (errno) {
        case EAGAIN:
            c->socket.read_ready = 0;
            return RETRY;
        case EINTR:
            continue;
        default:
            return ERROR;
        }
    }
}


static ssize_t
unix_send(struct conn *c, void *buf, size_t size)
{
    ssize_t n;

    for (;;) {
        n = write(c->socket.fd, buf, size);
        if (n > 0) {
            return n;
        }

        switch (errno) {
        case EAGAIN: return RETRY;
        case EINTR: continue;
        default: return ERROR;
        }
    }
}


static void
unix_close(struct conn *c)
{

}
