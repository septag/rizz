//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// tween helper: useful for small animations and transitions
//
// usage:
//      frame_update() {
//          static rizz_tween t;
//          float val = rizz_tween_update(&t, delta_time, )
//          // val: [0, 1]. you can ease it or do whatever you like
//          if (rizz_tween_done(&t)) {
//              // tweening is finished. trigger something
//          }
//      }
//
typedef struct rizz_tween {
    float tm;
} rizz_tween;

static inline float rizz_tween_update(rizz_tween* tween, float dt, float max_tm)
{
    sx_assert(max_tm > 0.0);
    float t = sx_min(1.0f, tween->tm / max_tm);
    tween->tm += dt;
    return t;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// rizz_event_queue: small _circular_ stack-based event queue.
// useful for pushing and polling gameplay events to components/objects
// NOTE: This is a small on-stack event queue implementation, and it's not thread-safe
//       If you want to use a more general thread-safe queue, use sx/thread.h: sx_queue_spsc
// NOTE: This is a circular queue, so it only holds RIZZ_EVENTQUEUE_MAX_EVENTS events and overwrites
//       previous ones if it's overflowed. So use it with this limitation in mind
//       most gameplay events for a component shouldn't require many events per-frame. If this
//       happens to not be the case, then make your own queue or increase the
//       `RIZZ_EVENTQUEUE_MAX_EVENTS` value
//
#ifndef RIZZ_EVENTQUEUE_MAX_EVENTS
#    define RIZZ_EVENTQUEUE_MAX_EVENTS 4
#endif

typedef struct rizz_event {
    int e;
    void* user;
} rizz_event;

typedef struct rizz_event_queue {
    rizz_event events[RIZZ_EVENTQUEUE_MAX_EVENTS];
    int first;
    int count;
} rizz_event_queue;

static inline void rizz_event_push(rizz_event_queue* eq, int event, void* user)
{
    if (eq->count < RIZZ_EVENTQUEUE_MAX_EVENTS) {
        int index = (eq->first + eq->count) % RIZZ_EVENTQUEUE_MAX_EVENTS;
        eq->events[index].e = event;
        eq->events[index].user = user;
        ++eq->count;
    } else {
        // overwrite previous events !!!
        int first = eq->first;
        first = ((first + 1) < RIZZ_EVENTQUEUE_MAX_EVENTS) ? (first + 1) : 0;
        int index = (first + eq->count) % RIZZ_EVENTQUEUE_MAX_EVENTS;
        eq->events[index].e = event;
        eq->events[index].user = user;
        eq->first = first;
    }
}

static inline bool rizz_event_poll(rizz_event_queue* eq, rizz_event* e)
{
    if (eq->count > 0) {
        int first = eq->first;
        *e = eq->events[first];
        eq->first = ((first + 1) < RIZZ_EVENTQUEUE_MAX_EVENTS) ? (first + 1) : 0;
        --eq->count;
        return true;
    } else {
        return false;
    }
}

static inline rizz_event rizz_event_peek(const rizz_event_queue* eq)
{
    sx_assert(eq->count > 0);
    return eq->events[eq->first];
}

static inline bool rizz_event_empty(const rizz_event_queue* eq)
{
    return eq->count == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// fsm:   Tiny finite state machine
//        state machines can be implemented with co-routines more intuitively
//        But coroutines allocate much more resources than this
// Stolen from: https://github.com/r-lyeh/tinybits/blob/master/tinyfsm.c
// Example:
// enum {
//    IDLE,
//    WALKING,
//    RUNNING,
// };
//
// void update( fsm state ) {
//    with(state) {
//        rizz_fsm_when(IDLE):                  puts("idle");
//        rizz_fsm_transition(IDLE,WALKING):    puts("idle --> walking");
//        rizz_fsm_transition(IDLE,RUNNING):    puts("idle --> running");
//
//        rizz_fsm_when(WALKING):               puts("walking");
//        rizz_fsm_transition(WALKING,IDLE):    puts("walking --> idle");
//        rizz_fsm_transition(WALKING,RUNNING): puts("walking --> running");
//
//        rizz_fsm_when(RUNNING):               puts("running");
//        rizz_fsm_transition(RUNNING,IDLE):    puts("running --> idle");
//        rizz_fsm_transition(RUNNING,WALKING): puts("running --> walking");
//    }
// }
//
// void main() {
//    rizz_fsm state = {0};
//
//    *state = IDLE;
//    update(state);        // Idle
//
//    *state = WALKING;
//    update(state);        // Idle->walking
//
//    *state = WALKING;
//    update(state);        // Walking
//
//    *state = RUNNING;
//    update(state);        // Walking->Running
//
//    *state = IDLE;
//    update(state);        // Running->Idle
// }
//

#define rizz_fsm_with(st)               \
    for (int i = 1; i--; st[1] = st[0]) \
        switch (((st[0]) << 16) | (st[1]))
#define rizz_fsm_when(a) \
    break;               \
    case (((a) << 16) | (a))
#define rizz_fsm_transition(a, b) \
    break;                        \
    case (((b) << 16) | (a))

typedef int rizz_fsm_state[2];
