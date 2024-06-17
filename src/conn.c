/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

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


int
conn_read(struct conn *c)
{
    size_t size;
    ssize_t n;

    size = c->read->end - c->read->free;
    n = conn_recv(c, c->read->free, size);

    if (n > 0) {
        c->read->free += n;
        return OK;
    }

    if (n == 0) {
        return OK;
    }

    if (n != RETRY) {
        return ERROR;
    }

    return RETRY;
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


int
conn_write(struct conn *c)
{
    struct thread *thr = cur_thread();
    size_t size;
    ssize_t n;

    while (c->write->pos < c->write->free) {
        size = c->write->free - c->write->pos;
        n = conn_send(c, c->write->pos, size);

        if (n > 0) {
            c->write->pos += n;
            continue;
        }

        if (n != RETRY) {
            return ERROR;
        }

        epoll_add_event(thr->engine, &c->socket, EVENT_WRITE);
        return RETRY;
    }

    epoll_delete_event(thr->engine, &c->socket, EVENT_WRITE);
    return OK;
}
