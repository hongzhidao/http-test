/*
 * Copyright (C) Zhidao HONG
 */
#ifndef CONN_H
#define CONN_H

struct conn {
    file_event socket;
    http_parser parser;
    http_chunk_parser chunk_parser;
    uint64_t start;
    struct buf *read;
    struct buf *write;
    struct timer timer;
    off_t remainder;
    void (*read_handler)(struct conn *);
};

int conn_read(struct conn *);
int conn_write(struct conn *);

#endif /* CONN_H */
