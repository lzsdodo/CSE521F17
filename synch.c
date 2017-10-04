/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.
   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.
   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/****************function declaration for priority scheduling *****************************/
bool cmpPriority(struct list_elem *a_, struct list_elem *b_, void *UNUSED);
bool cmpMax(struct list_elem *a_, struct list_elem *b_, void *UNUSED);
/***************************************************************************************/
/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:
   - down or "P": wait for the value to become positive, then
     decrement it.
   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void sema_init (struct semaphore *sema, unsigned value)
{
    ASSERT (sema != NULL);

    sema->value = value;
    list_init (&sema->waiters);
}



/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread (const struct lock *lock)
{
    ASSERT (lock != NULL);

    return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem
{
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init (struct condition *cond)
{
    ASSERT (cond != NULL);

    list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.
   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.
   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.
   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void cond_wait (struct condition *cond, struct lock *lock)
{
    struct semaphore_elem waiter;

    ASSERT (cond != NULL);
    ASSERT (lock != NULL);
    ASSERT (!intr_context ());
    ASSERT (lock_held_by_current_thread (lock));

    sema_init (&waiter.semaphore, 0);
    list_push_back (&cond->waiters, &waiter.elem);
    lock_release (lock);
    sema_down (&waiter.semaphore);
    lock_acquire (lock);
}


static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test (void)
{
    struct semaphore sema[2];
    int i;

    printf ("Testing semaphores...");
    sema_init (&sema[0], 0);
    sema_init (&sema[1], 0);
    thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
    for (i = 0; i < 10; i++)
    {
        sema_up (&sema[0]);
        sema_down (&sema[1]);
    }
    printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void sema_test_helper (void *sema_)
{
    struct semaphore *sema = sema_;
    int i;

    for (i = 0; i < 10; i++)
    {
        sema_down (&sema[0]);
        sema_up (&sema[1]);
    }
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.
   This function may be called from an interrupt handler. */
bool sema_try_down (struct semaphore *sema)
{
    enum intr_level old_level;
    bool success;

    ASSERT (sema != NULL);

    old_level = intr_disable ();
    if (sema->value > 0)
    {
        sema->value--;
        success = true;
    }
    else
        success = false;
    intr_set_level (old_level);

    return success;
}
/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.
   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire (struct lock *lock)
{
    bool success;

    ASSERT (lock != NULL);
    ASSERT (!lock_held_by_current_thread (lock));

    success = sema_try_down (&lock->semaphore);
    if (success)
        lock->holder = thread_current ();
    return success;
}

/* Releases LOCK, which must be owned by the current thread.
   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.
   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast (struct condition *cond, struct lock *lock)
{
    ASSERT (cond != NULL);
    ASSERT (lock != NULL);

    while (!list_empty (&cond->waiters))
        cond_signal (cond, lock);
}

/**************************************************************************************************************************************
insert curr threads into sema waiters at the proper position
*************************************************************************************************************************************/

void sema_down (struct semaphore *sema)
{
    enum intr_level old_level;
    struct thread*curr = thread_current();
    ASSERT (sema != NULL);
    ASSERT (!intr_context ());
    old_level = intr_disable ();
    while (sema->value == 0)
    {

        list_insert_ordered(&sema->waiters, &curr->elem, cmpPriority, NULL);
        thread_block ();
    }
    sema->value--;
    intr_set_level (old_level);
}
/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.
   This function may be called from an interrupt handler. */

 /************ unblock the thread by choosing max priority one ********************/
void sema_up (struct semaphore *sema)
{
    enum intr_level old_level;

    ASSERT (sema != NULL);

    old_level = intr_disable ();
    sema->value++;
    if (!list_empty (&sema->waiters))
    {

        struct list_elem *e=list_max (&sema->waiters,cmpMax, NULL);
        list_remove(e);
        thread_unblock (list_entry (e, struct thread, elem));
    }
    intr_set_level (old_level);
}


/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.
   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init (struct lock *lock) {
    ASSERT (lock != NULL);
    lock->holder = NULL;
    sema_init (&lock->semaphore, 1);
    list_init(&lock->waiters);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.
   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

/******************************************
important stuff
******************************************/
void lock_acquire (struct lock *lock) {
    enum intr_level old_level;
    ASSERT (lock != NULL);
    ASSERT (!intr_context ());
    ASSERT (!lock_held_by_current_thread (lock));

    struct thread* curr = thread_current();

/*
tries to get sema down, and if unsuccessful
goes to waiting state after adding itself to lock->waiters list.
*/
    old_level = intr_disable();
    /** while this lock is not accessible **/
    while(!sema_try_down(&lock->semaphore))
    {
       curr->targetLock = lock;

        /** after an attempt to acquire this thread fails, put curr thread in lock-waiters at its proper position **/
        list_insert_ordered(&lock->waiters, &curr->elem, cmpPriority, NULL);

        /************************ nested donations *******************************/
        struct thread *targetThread;
        struct lock *lock_held = lock;
        for(targetThread = lock->holder; ; targetThread = targetThread->targetLock->holder)
        {
            if(targetThread->priority < thread_current()->priority) {
                thread_change_priority(targetThread, curr->priority, lock_held);
            }
            if(targetThread->targetLock == NULL) break;
            else lock_held = targetThread->targetLock;
        }
        thread_block();
    }
    intr_set_level(old_level);
    lock->holder = curr;
}



/******************************************
important stuff
******************************************/
void lock_release (struct lock *lock) {
    struct thread *cur=thread_current();
    enum intr_level old_level;
    old_level=intr_disable();
    ASSERT (lock != NULL);
    ASSERT (lock_held_by_current_thread (lock));

    // make this lock's semaphore become avaliable
    lock->holder = NULL;
    sema_up (&lock->semaphore);

    /**
     *  unblock the max priority thread that is waiting for this lock
     */
    if(!list_empty(&lock->waiters))
    {
        struct list_elem *e = list_max(&lock->waiters, cmpMax, NULL);
        struct thread* toRun = list_entry(e, struct thread, elem);
        list_remove(e);
        toRun->targetLock = NULL;
        thread_unblock(toRun);
    }

    /**
     * if current thread has been donated priority due to this lock(donatedlaok)
     *  the thread's priority is recovered to  previous priority
     */
    if(cur->donatedLock == lock)
    {
        struct list_elem* p= list_pop_front(&cur->listOfInfo);
        struct info *prevInfo= list_entry(p, struct info, elem);
        struct lock *prevLock = prevInfo->lock;
        int prevPriority = prevInfo->priority;
      //  free(e);
        thread_change_priority(cur, prevPriority, prevLock);
    }
       /***** If the thread was once donated priority due to this lock, remove that priority from the priority list */
    else
    {
        removeTargetLockFromPList(lock, cur);
    }
    intr_set_level(old_level);

}



// helper function to remove the lock from listOfInfo, without modifying any priority
void removeTargetLockFromPList(struct lock* targetLock, struct thread* t){
    struct list_elem *e;
    for(e=list_begin(&t->listOfInfo);e!=list_end(&t->listOfInfo);e=list_next(e))
    {
        struct info *currInfo = list_entry(e, struct info, elem);
        if(currInfo->lock == targetLock)
        {
            list_remove(e);
            free(currInfo);
            break;
        }
    }
}


/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.
   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/******************************************
important stuff
******************************************/
void cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
    ASSERT (cond != NULL);
    ASSERT (lock != NULL);
    ASSERT (!intr_context ());
    ASSERT (lock_held_by_current_thread (lock));
    /******************************************************************
    release the thread with highest priority in the condi waiting
    ******************************************************************/
    if (!list_empty (&cond->waiters))
    {
        struct list_elem *e;
        struct list_elem *max_elem;
        int max_pri = -1;

        /***Iterate over cond-waiter list to obtain the max element*/
        for(e=list_begin(&cond->waiters);e!=list_end(&cond->waiters);e=list_next(e))
        {
            struct semaphore_elem *s_elem = list_entry(e, struct semaphore_elem, elem);
            struct thread* tempThread = list_entry(list_front(&s_elem->semaphore.waiters), struct thread, elem);

            if(tempThread->priority > max_pri)
            {
                max_pri =tempThread->priority;
                max_elem = e;
            }
        }
        /*** remove this max shit and releaes this semaphore */
        list_remove(max_elem);
        struct semaphore_elem* maxSem = list_entry (max_elem, struct semaphore_elem, elem);
        sema_up (&maxSem->semaphore);
    }

}


/******************************************
comparing functions for priority scheduling
******************************************/
bool cmpPriority(struct list_elem *a_, struct list_elem *b_, void * aux )
{
    struct thread *a, *b;
    a =list_entry(a_,struct thread, elem);
    b =list_entry(b_,struct thread, elem);
    return a->priority>b->priority ? true : false ;
}
bool cmpMax (struct list_elem *a_,struct list_elem *b_, void * aux)
{
    struct thread *a, *b;
    a=list_entry(a_, struct thread, elem);
    b=list_entry(b_, struct thread, elem);
    return b->priority >= a->priority ? true: false;
}


