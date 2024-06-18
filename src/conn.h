/*
 * Copyright (C) Zhidao HONG
 */
#ifndef CONN_H
#define CONN_H

struct conn;

typedef struct {
    ssize_t (*recv)(struct conn *, void *, size_t);
    ssize_t (*send)(struct conn *, void *, size_t);
} conn_io;

struct conn {
    file_event socket;
    http_parser parser;
    http_chunk_parser chunk_parser;
    uint64_t start;
    struct buf *read;
    struct buf *write;
    struct timer timer;
    off_t remainder;
    const conn_io *io;
    event_handler read_handler;
    event_handler close_handler;
    event_handler error_handler;
};

void conn_read(void *, void *);
void conn_write(void *, void *);

extern conn_io unix_conn_io;

#endif /* CONN_H */
