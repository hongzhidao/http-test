/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

static ssize_t conn_recv(struct conn *, void *, size_t);
static ssize_t conn_send(struct conn *, void *, size_t);

conn_io unix_conn_io = {
    .recv = conn_recv,
    .send = conn_send
};


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


static ssize_t
conn_recv(struct conn *c, void *buf, size_t size)
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
conn_send(struct conn *c, void *buf, size_t size)
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
