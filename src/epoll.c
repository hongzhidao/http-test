/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

int
epoll_engine_create(event_engine *engine, int mevents)
{
    struct epoll *epoll = &engine->epoll;

    epoll->mevents = mevents;

    epoll->events = zmalloc(sizeof(struct epoll_event) * mevents);
    if (epoll->events == NULL) {
        return -1;
    }

    epoll->epfd = epoll_create(1024);
    if (epoll->epfd == -1) {
        zfree(epoll->events);
        return -1;
    }

    return 0;
}


void
epoll_engine_free(event_engine *engine)
{
    struct epoll *epoll = &engine->epoll;

    zfree(epoll->events);
    close(epoll->epfd);
}


int
epoll_create_event(event_engine *engine, file_event *ev, int mask)
{
    struct epoll *epoll = &engine->epoll;
    struct epoll_event ee = {0};
    int op;

    ee.events = 0;
    ee.data.ptr = ev;

    mask |= ev->mask;

    if (mask & EVENT_READ) {
        ee.events |= EPOLLIN;
    }

    if (mask & EVENT_WRITE) {
        ee.events |= EPOLLOUT;
    }

    op = (ev->mask == EVENT_NONE) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    if (epoll_ctl(epoll->epfd, op, ev->fd, &ee) == -1) {
        return -1;
    }

    ev->mask = mask;

    return 0;
}


void
epoll_delete_event(event_engine *engine, file_event *ev, int mask)
{
    struct epoll *epoll = &engine->epoll;
    struct epoll_event ee = {0};
    int op;

    if (ev->mask == EVENT_NONE) {
        return;
    }

    ee.events = 0;
    ee.data.ptr = ev;

    mask = ev->mask & (~mask);

    if (mask & EVENT_READ) {
        ee.events |= EPOLLIN;
    }

    if (mask & EVENT_WRITE) {
        ee.events |= EPOLLOUT;
    }

    op = (mask == EVENT_NONE) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
    epoll_ctl(epoll->epfd, op, ev->fd, &ee);

    ev->mask = mask;
}


void
epoll_poll(event_engine *engine, int timeout)
{
    struct thread *thr = cur_thread();
    struct epoll *epoll = &engine->epoll;
    int i, nevents;
    struct epoll_event *event;

    nevents = epoll_wait(epoll->epfd, epoll->events, epoll->mevents, timeout);

    thr->time = monotonic_time();

    if (nevents < 0) {
        return;
    }

    for (i = 0; i < nevents; i++) {
        event = &epoll->events[i];
        file_event *ev = event->data.ptr;

        if (event->events & EPOLLIN) {
            ev->read_handler(ev, ev->data);
        }

        if (event->events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
            ev->write_handler(ev, ev->data);
        }
    }
}
