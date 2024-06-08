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
    char *script;
    char *url;
    char *host;
    char *path;
    http_field *headers;
    struct addrinfo *addr;
};

struct thread {
    pthread_t handle; 
    event_engine *engine;
    lua_State *lua;
    int has_request;
    uint64_t time;
};

extern struct config cfg;

#define cur_thread() &thread_ctx;
extern __thread struct thread thread_ctx;

#endif /* MAIN_H */
