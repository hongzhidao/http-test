/*
 * Copyright (C) Zhidao HONG
 */
#ifndef EPOLL_H
#define EPOLL_H

#define EVENT_NONE 0
#define EVENT_READ 1
#define EVENT_WRITE 2

typedef void (*event_handler)(void *obj, void *data);

typedef struct {
    int fd;
    int mask;
    event_handler read_handler;
    event_handler write_handler;
    void *data;
    uint8_t read_ready;
} file_event;

struct epoll {
    int epfd;
    int mevents;
    struct epoll_event *events;
};

int epoll_engine_create(event_engine *, int);
void epoll_engine_free(event_engine *);
int epoll_create_event(event_engine *, file_event *, int);
void epoll_delete_event(event_engine *, file_event *, int);
void epoll_poll(event_engine *, int);

#endif
