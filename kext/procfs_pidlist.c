#include <kern/assert.h>
#include <kern/debug.h>
#include <kern/kalloc.h>
#include <sys/queue.h>

#include "procfs_pidlist.h"

#pragma mark -
#pragma mark Function Prototypes

pidlist_t *procfs_pidlist_init(pidlist_t *pl);
u_int procfs_pidlist_alloc(pidlist_t *pl, u_int needed);
u_int procfs_pidlist_nalloc(const pidlist_t *pl);
void procfs_pidlist_set_active(pidlist_t *pl);
void procfs_pidlist_free(pidlist_t *pl);

#pragma mark -
#pragma mark Pidlist Functions

pidlist_t *
procfs_pidlist_init(pidlist_t *pl)
{
	SLIST_INIT(&pl->pl_head);
	pl->pl_active = NULL;
	pl->pl_nalloc = 0;
	return pl;
}

u_int
procfs_pidlist_alloc(pidlist_t *pl, u_int needed)
{
	while (pl->pl_nalloc < needed) {
		pidlist_entry_t *pe = kalloc(sizeof(*pe));
		if (NULL == pe) {
			panic("no space for pidlist entry");
		}
		SLIST_INSERT_HEAD(&pl->pl_head, pe, pe_link);
		pl->pl_nalloc += (sizeof(pe->pe_pid) / sizeof(pe->pe_pid[0]));
	}
	return pl->pl_nalloc;
}

u_int
procfs_pidlist_nalloc(const pidlist_t *pl)
{
	return pl->pl_nalloc;
}

void
procfs_pidlist_set_active(pidlist_t *pl)
{
	pl->pl_active = SLIST_FIRST(&pl->pl_head);
	assert(pl->pl_active);
}

void
procfs_pidlist_free(pidlist_t *pl)
{
    pidlist_entry_t *pe;
    while (NULL != (pe = SLIST_FIRST(&pl->pl_head))) {
        SLIST_FIRST(&pl->pl_head) = SLIST_NEXT(pe, pe_link);
        kalloc(sizeof(*pe));
    }
    pl->pl_nalloc = 0;
}
