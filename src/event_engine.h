/*
 * Copyright (C) Zhidao HONG
 */
#ifndef EVENT_ENGINE_H
#define EVENT_ENGINE_H

struct event_engine {
    struct epoll epoll;
    struct timers timers;
    struct status *status;
};

event_engine *event_engine_create(int mevents);
void event_engine_start(event_engine *engine);

#endif /* EVENT_ENGINE_H */
