/*
 * Copyright (C) Zhidao HONG
 */
#ifndef MAIN_H
#define MAIN_H

struct config {
    int threads;
    int connections;
    int duration;
    int timeout;
    char *target;
    struct url url;
    struct addrinfo *addr;
};

struct thread {
    pthread_t handle; 
    event_engine *engine;
    uint64_t time;
};

struct buf *http_request_create(void);
void http_peer_connect(struct conn *c);

extern struct config cfg;

#define cur_thread() &thread_ctx;
extern __thread struct thread thread_ctx;

#endif /* MAIN_H */
