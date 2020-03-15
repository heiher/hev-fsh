/*
 ============================================================================
 Name        : hev-fsh-session-manager.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2020 everyone.
 Description : Fsh session manager
 ============================================================================
 */

#include <stdio.h>

#include <hev-memory-allocator.h>

#include "hev-fsh-config.h"
#include "hev-fsh-session.h"

#include "hev-fsh-session-manager.h"

struct _HevFshSessionManager
{
    HevTask *task;
    HevFshSession *sessions;

    int quit;
    int timeout;
    int autostop;
};

static void hev_fsh_session_manager_task_entry (void *data);

HevFshSessionManager *
hev_fsh_session_manager_new (int timeout, int autostop)
{
    HevFshSessionManager *self;

    self = hev_malloc0 (sizeof (HevFshSessionManager));
    if (!self) {
        fprintf (stderr, "Allocate session manager failed!\n");
        goto exit;
    }

    self->task = hev_task_new (HEV_FSH_CONFIG_TASK_STACK_SIZE);
    if (!self->task) {
        fprintf (stderr, "Create session manager's task failed!\n");
        goto exit_free;
    }

    if (timeout <= HEV_FSH_SESSION_HP)
        self->timeout = 1;
    else
        self->timeout = timeout / HEV_FSH_SESSION_HP;

    self->autostop = autostop;

    return self;

exit_free:
    hev_free (self);
exit:
    return NULL;
}

void
hev_fsh_session_manager_destroy (HevFshSessionManager *self)
{
    hev_task_unref (self->task);
    hev_free (self);
}

void
hev_fsh_session_manager_start (HevFshSessionManager *self)
{
    hev_task_ref (self->task);
    hev_task_run (self->task, hev_fsh_session_manager_task_entry, self);
}

void
hev_fsh_session_manager_stop (HevFshSessionManager *self)
{
    self->quit = 1;
    hev_task_wakeup (self->task);
}

void
hev_fsh_session_manager_insert (HevFshSessionManager *self, HevFshSession *s)
{
#ifdef _DEBUG
    printf ("Insert session: %p\n", s);
#endif
    s->prev = NULL;
    s->next = self->sessions;
    if (self->sessions)
        self->sessions->prev = s;
    self->sessions = s;
}

void
hev_fsh_session_manager_remove (HevFshSessionManager *self, HevFshSession *s)
{
#ifdef _DEBUG
    printf ("Remove session: %p\n", s);
#endif
    if (s->prev) {
        s->prev->next = s->next;
    } else {
        self->sessions = s->next;
    }
    if (s->next) {
        s->next->prev = s->prev;
    }

    if (self->autostop && !self->sessions)
        hev_fsh_session_manager_stop (self);
}

static void
hev_fsh_session_manager_task_entry (void *data)
{
    HevFshSessionManager *self = data;

    for (; !self->quit;) {
        HevFshSession *s;

        hev_task_sleep (self->timeout * 1000);

#ifdef _DEBUG
        printf ("Enumerating sessions ...\n");
#endif
        for (s = self->sessions; s; s = s->next) {
#ifdef _DEBUG
            printf ("Session %p's hp %d\n", s, s->hp);
#endif
            if (self->quit) {
                s->hp = 0;
            } else {
                s->hp--;
                if (s->hp > 0)
                    continue;
            }

            hev_task_wakeup (s->task);
#ifdef _DEBUG
            printf ("Wakeup session %p's task %p\n", s, s->task);
#endif
        }
    }
}
