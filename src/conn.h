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
    event_handler read_handler;
    event_handler close_handler;
    event_handler error_handler;
};

void conn_read(void *, void *);
void conn_write(void *, void *);

#endif /* CONN_H */
