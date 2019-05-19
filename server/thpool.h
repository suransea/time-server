//
// Created by sea on 3/24/19.
//

#ifndef _THPOOL_
#define _THPOOL_

typedef struct thpool_ thpool;

thpool *thpool_init(int num_threads);

int thpool_add_work(thpool *, void (*function_p)(void *), void *arg_p);

void thpool_wait(thpool *);

void thpool_pause(thpool *);

void thpool_resume(thpool *);

void thpool_destroy(thpool *);

int thpool_num_threads_working(thpool *);

#endif