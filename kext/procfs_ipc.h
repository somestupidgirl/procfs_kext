#ifndef procfs_ipc_h
#define procfs_ipc_h

#include <kern/debug.h>
#include <kern/locks.h>
#include <libkern/OSAtomic.h>
#include <mach/port.h>
#include <os/refcnt.h>

#include "procfs_thread.h"

typedef natural_t ipc_object_refs_t;    /* for ipc/ipc_object.h     */
typedef natural_t ipc_object_bits_t;
typedef natural_t ipc_object_type_t;
typedef natural_t ipc_kobject_type_t;
typedef uint64_t  ipc_label_t;
typedef void *    ipc_kobject_t;
typedef struct    ipc_object *ipc_object_t;
typedef struct    ipc_kobject_label *ipc_kobject_label_t;
typedef struct    ipc_importance_task *ipc_importance_task_t;
typedef struct    ipc_port *ipc_port_t;

#define IKOT_NONE                       0
#define IKOT_THREAD_CONTROL             1
#define IKOT_TASK_CONTROL               2
#define IKOT_THREAD_INSPECT             46
#define IKOT_THREAD_READ                47

struct ipc_kobject_label {
    ipc_label_t   ikol_label;       /* [private] mandatory access label */
    ipc_kobject_t ikol_kobject;     /* actual kobject address */
};

struct ipc_object {
	ipc_object_bits_t   io_bits;
	ipc_object_refs_t   io_references;
	lck_spin_t         *io_lock_data;
} __attribute__((aligned(8)));

#define ip_references           ip_object.io_references

struct ipc_port {
	struct ipc_object ip_object;
    /* update host_request_notification if this union is changed */
    union {
        ipc_kobject_t kobject;
        ipc_kobject_label_t kolabel;
        ipc_importance_task_t imp_task;
        ipc_port_t sync_inheritor_port;
        struct knote *sync_inheritor_knote;
        struct turnstile *sync_inheritor_ts;
    } kdata;
    mach_port_mscount_t ip_mscount;
    mach_port_rights_t ip_srights;
};

#define ip_kobject              kdata.kobject
#define ip_kolabel              kdata.kolabel

/*
 *	IPC steals the high-order bits from the kotype to use
 *	for its own purposes.  This allows IPC to record facts
 *	about ports that aren't otherwise obvious from the
 *	existing port fields.  In particular, IPC can optionally
 *	mark a port for no more senders detection.  Any change
 *	to IO_BITS_PORT_INFO must be coordinated with bitfield
 *	definitions in ipc_port.h.
 */
#define IO_BITS_PORT_INFO       0x0000f000      /* stupid port tricks */
#define IO_BITS_KOTYPE          0x000003ff      /* used by the object */
#define IO_BITS_KOBJECT         0x00000800      /* port belongs to a kobject */
#define IO_BITS_KOLABEL         0x00000400      /* The kobject has a label */
#define IO_BITS_OTYPE           0x7fff0000      /* determines a zone */
#define IO_BITS_ACTIVE          0x80000000      /* is object alive? */

#define io_active(io)           (((io)->io_bits & IO_BITS_ACTIVE) != 0)
#define io_otype(io)            (((io)->io_bits & IO_BITS_OTYPE) >> 16)
#define io_kotype(io)           ((io)->io_bits & IO_BITS_KOTYPE)
#define io_is_kobject(io)       (((io)->io_bits & IO_BITS_KOBJECT) != IKOT_NONE)
#define io_is_kolabeled(io)     (((io)->io_bits & IO_BITS_KOLABEL) != 0)
#define io_makebits(active, otype, kotype)      \
	(((active) ? IO_BITS_ACTIVE : 0) | ((otype) << 16) | (kotype))

/*
 * Here we depend on the ipc_object being first within the kernel struct
 * (ipc_port and ipc_pset).
 */
#define io_lock_init(io) \
    lck_spin_init(&(io)->io_lock_data, &ipc_lck_grp, &ipc_lck_attr)
#define io_lock_destroy(io) \
    lck_spin_destroy(&(io)->io_lock_data, &ipc_lck_grp)
#define io_lock_held(io) \
    LCK_SPIN_ASSERT(&(io)->io_lock_data, LCK_ASSERT_OWNED)
#define io_lock_held_kdp(io) \
    kdp_lck_spin_is_acquired(&(io)->io_lock_data)
#define io_unlock(io) \
    lck_spin_unlock(&(io)->io_lock_data)

extern void io_lock(
    ipc_object_t io);
extern boolean_t io_lock_try(
    ipc_object_t io);

#define IP_NULL                 IPC_PORT_NULL
#define IP_DEAD                 IPC_PORT_DEAD
#define IP_VALID(port)          IPC_PORT_VALID(port)

#define ip_object_to_port(io)   __container_of(io, struct ipc_port, ip_object)
#define ip_to_object(port)      (&(port)->ip_object)
#define ip_active(port)         io_active(ip_to_object(port))
#define ip_lock_init(port)      io_lock_init(ip_to_object(port))
#define ip_lock_held(port)      io_lock_held(ip_to_object(port))
#define ip_lock(port)           io_lock(ip_to_object(port))
#define ip_lock_try(port)       io_lock_try(ip_to_object(port))
#define ip_lock_held_kdp(port)  io_lock_held_kdp(ip_to_object(port))
#define ip_unlock(port)         io_unlock(ip_to_object(port))

#define ip_reference(port)      io_reference(ip_to_object(port))
#define ip_release(port)        io_release(ip_to_object(port))

#define ip_kotype(port)         io_kotype(ip_to_object(port))
#define ip_is_kobject(port)     io_is_kobject(ip_to_object(port))
#define ip_is_control(port) \
    (ip_is_kobject(port) && (ip_kotype(port) == IKOT_TASK_CONTROL || ip_kotype(port) == IKOT_THREAD_CONTROL))
#define ip_is_kolabeled(port)   io_is_kolabeled(ip_to_object(port))
#define ip_get_kobject(port)    ipc_kobject_get(port)

/* Get the kobject address associated with a port */
static inline ipc_kobject_t
ipc_kobject_get(ipc_port_t port)
{
    if (ip_is_kobject(port)) {
        if (ip_is_kolabeled(port)) {
            return port->ip_kolabel->ikol_kobject;
        }
        return &port->ip_kobject;
    }
    return 0;
}

#define IO_MAX_REFERENCES                                               \
    (unsigned)(~0U ^ (1U << (sizeof(int)*BYTE_SIZE - 1)))

static inline void
io_reference(ipc_object_t io)
{
    ipc_object_refs_t new_io_references;
    ipc_object_refs_t old_io_references;

    if ((io)->io_references == 0 ||
        (io)->io_references >= IO_MAX_REFERENCES) {
        panic("%s: reference count %u is invalid\n", __func__, (io)->io_references);
    }

    do {
        old_io_references = (io)->io_references;
        new_io_references = old_io_references + 1;
        if (old_io_references == IO_MAX_REFERENCES) {
            break;
        }
    } while (OSCompareAndSwap(old_io_references, new_io_references,
        &((io)->io_references)) == FALSE);
}

extern lck_grp_t        ipc_lck_grp;
extern lck_attr_t       ipc_lck_attr;

// osfmk/kern/ipc_tt.h
__options_decl(port_to_thread_options_t, uint32_t, {
	PORT_TO_THREAD_NONE               = 0x0000,
	PORT_TO_THREAD_IN_CURRENT_TASK    = 0x0001,
	PORT_TO_THREAD_NOT_CURRENT_THREAD = 0x0002,
});

#define thread_reference_internal(thread)       \
                    os_ref_retain(&(thread)->ref_count);

thread_t convert_port_to_thread(ipc_port_t port);
ipc_port_t convert_thread_to_port(thread_t thread);
ipc_port_t convert_thread_read_to_port(thread_read_t thread);
ipc_port_t convert_thread_inspect_to_port(thread_inspect_t thread);

#endif /* procfs_ipc_h */
