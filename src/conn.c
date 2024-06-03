/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

int
conn_read(struct conn *c, size_t *plen)
{
    size_t size;
    ssize_t n;

    size = c->read->end - c->read->free;

    for (;;) {
        n = read(c->socket.fd, c->read->free, size);
        *plen = (size_t) n;

        if (n > 0) {
            if (n < size) {
                c->socket.read_ready = 0;
            }
            c->read->free += n;
            return OK;
        }

        if (n == 0) {
            return OK;
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
conn_write(struct conn *c)
{
    size_t size;
    ssize_t n;

    size = c->write->free - c->write->pos;

    for (;;) {
        n = write(c->socket.fd, c->write->pos, size);
        if (n > 0) {
            c->write->pos += n;
            return OK;
        }

        switch (errno) {
        case EAGAIN:
            return RETRY;
        case EINTR: continue;
        default: return ERROR;
        }
    }
}
