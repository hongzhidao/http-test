/*
 * Copyright (C) Zhidao HONG
 */
#include "headers.h"

event_engine *
event_engine_create(int mevents)
{
    event_engine *engine;

    engine = zcalloc(sizeof(event_engine));
    if (engine == NULL) {
        return NULL;
    }

    if (epoll_engine_create(engine, mevents)) {
        return NULL;
    }

    timers_init(&engine->timers);

    engine->status = status_create();
    if (engine->status == NULL) {
        return NULL;
    }

    return engine;
}


void
event_engine_start(event_engine *engine)
{
    struct thread *thr = cur_thread();
    int timeout;
    uint32_t now;

    while (1) {
        timeout = timer_find(engine);
        epoll_poll(engine, timeout);
        now = thr->time / 1000000;
        timer_expire(engine, now);
    }
}
