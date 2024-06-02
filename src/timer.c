/*
 * Copyright (C) Igor Sysove
 */
#include "headers.h"

static intptr_t timer_rbtree_compare(rbtree_node *, rbtree_node *);


void
timers_init(struct timers *timers)
{
    rbtree_init(&timers->tree, timer_rbtree_compare);
}


static intptr_t
timer_rbtree_compare(rbtree_node *node1, rbtree_node *node2)
{
    struct timer *timer1, *timer2;

    timer1 = (struct timer *) node1;
    timer2 = (struct timer *) node2;

    /*
     * Timer values are distributed in small range, usually several minutes
     * and overflow every 49 days if uint32_t is stored in 32 bits.
     * This signed comparison takes into account that overflow.
     */
    return msec_diff(timer1->time , timer2->time);
}


void
timer_add(event_engine *engine, struct timer *timer, uint32_t timeout)
{
    int32_t diff;
    uint32_t time;

    time = engine->timers.now + timeout;

    if (timer_is_in_tree(timer)) {
        diff = msec_diff(time, timer->time);

        /*
         * Use the previous timer if difference between it and the
         * new timer is within bias: this decreases number of rbtree
         * operations for fast connections.
         */
        if (abs(diff) <= timer->bias) {
            return;
        }

        timer_remove(engine, timer);
    }

    timer->time = time;

    rbtree_insert(&engine->timers.tree, &timer->node);
}


void
timer_remove(event_engine *engine, struct timer *timer)
{
    rbtree_delete(&engine->timers.tree, &timer->node);
}


uint32_t
timer_find(event_engine *engine)
{
    int32_t delta;
    uint32_t time;
    rbtree_node *node, *next;
    struct timer *timer;
    struct timers *timers;
    struct rbtree *tree;

    timers = &engine->timers;

    tree = &timers->tree;

    for (node = rbtree_min(tree);
         rbtree_is_there_successor(tree, node);
         node = next)
    {
        next = rbtree_node_successor(tree, node);

        timer = (struct timer *) node;

        /*
         * Disabled timers are not deleted here since the minimum active
         * timer may be larger than a disabled timer, but event poll may
         * return much earlier and the disabled timer can be reactivated.
         */

        time = timer->time;
        timers->minimum = time - timer->bias;

        delta = msec_diff(time, timers->now);

        return (uint32_t) max_int(delta, 0);
    }

    /* Set minimum time one day ahead. */
    timers->minimum = timers->now + 24 * 60 * 60 * 1000;

    return (uint32_t) -1;
}


void
timer_expire(event_engine *engine, uint32_t now)
{
    rbtree_node *node, *next;
    struct timer *timer;
    struct timers *timers;
    struct rbtree *tree;

    timers = &engine->timers;
    timers->now = now;

    if (msec_diff(timers->minimum , now) > 0) {
        return;
    }

    tree = &timers->tree;

    for (node = rbtree_min(tree);
         rbtree_is_there_successor(tree, node);
         node = next)
    {
        timer = (struct timer *) node;

        if (msec_diff(timer->time , now) > (int32_t) timer->bias) {
            return;
        }

        next = rbtree_node_successor(tree, node);

        rbtree_delete(tree, &timer->node);

        timer->handler(timer, NULL);
    }
}
