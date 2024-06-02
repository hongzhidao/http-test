/*
 * Copyright (C) Igor Sysoev
 */
#ifndef TIMER_H
#define TIMER_H

typedef void (*timer_handler)(void *obj, void *data);

struct timer {
    /* The rbtree node must be the first field. */
    rbtree_node node;
    timer_handler handler;
    uint32_t time;
    uint8_t bias;
};

struct timers {
    struct rbtree tree;
    /* An overflown milliseconds counter. */
    uint32_t now;
    uint32_t minimum;
};

void timers_init(struct timers *);
void timer_add(event_engine *, struct timer *, uint32_t);
void timer_remove(event_engine *, struct timer *);
uint32_t timer_find(event_engine *);
void timer_expire(event_engine *, uint32_t);

/* Valid values are between 0ms to 255ms. */
#define TIMER_DEFAULT_BIAS 50

#define timer_is_in_tree(timer)                                               \
    ((timer)->node.parent != NULL)

#endif /* TIMER_H */
