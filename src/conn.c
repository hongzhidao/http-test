/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

static int unix_connect(struct conn *, char *);
static ssize_t unix_recv(struct conn *, void *, size_t);
static ssize_t unix_send(struct conn *, void *, size_t);
static void unix_close(struct conn *);

conn_io unix_conn_io = {
    .connect = unix_connect,
    .recv = unix_recv,
    .send = unix_send,
    .close = unix_close,
};


int
conn_connect(struct conn *c, char *host)
{
    return c->io->connect(c, host);
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
    c->io->close(c);
}


static int
unix_connect(struct conn *c, char *host)
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
