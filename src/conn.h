/*
 * Copyright (C) Zhidao HONG
 */
#ifndef CONN_H
#define CONN_H

struct conn;

typedef struct {
    int (*connected)(struct conn *, char *host);
    ssize_t (*recv)(struct conn *, void *, size_t);
    ssize_t (*send)(struct conn *, void *, size_t);
    void (*close)(struct conn *);
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
    void *ssl;
    const conn_io *io;
    event_handler read_handler;
    event_handler close_handler;
    event_handler error_handler;
};

void conn_connect(struct conn *, struct addrinfo *);
void conn_connected(struct conn *, char *);
void conn_read(void *, void *);
void conn_write(void *, void *);
void conn_close(struct conn *);

extern conn_io unix_conn_io;

#endif /* CONN_H */
