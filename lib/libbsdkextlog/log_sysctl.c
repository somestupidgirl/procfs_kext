/*
 * Created 190423 lynnl
 */

#include <sys/sysctl.h>

#include <libkext/libkext.h>
#include <libbsdkextlog/log_sysctl.h>

static SYSCTL_NODE(
    /* No parent */,
    OID_AUTO,
    kextlog,
    CTLFLAG_RD,
    NULL,
    "" /* root sysctl node: kextlog */
)

static SYSCTL_NODE(
    _kextlog,
    OID_AUTO,
    statistics,
    CTLFLAG_RD,
    NULL,
    "" /* sysctl node: kextlog.statistics */
)

struct kextlog_statistics log_stat = {};

static SYSCTL_QUAD(
    _kextlog_statistics,
    OID_AUTO,
    syslog,
    CTLFLAG_RD,
    (uint64_t *) &log_stat.syslog,
    "" /* sysctl nub: kextlog.statistics.syslog */
);

static SYSCTL_QUAD(
    _kextlog_statistics,
    OID_AUTO,
    heapmsg,
    CTLFLAG_RD,
    (uint64_t *) &log_stat.heapmsg,
    "" /* sysctl nub: kextlog.statistics.heapmsg */
);

static SYSCTL_QUAD(
    _kextlog_statistics,
    OID_AUTO,
    stackmsg,
    CTLFLAG_RD,
    (uint64_t *) &log_stat.stackmsg,
    "" /* sysctl nub: kextlog.statistics.stackmsg */
);

static SYSCTL_QUAD(
    _kextlog_statistics,
    OID_AUTO,
    oom,
    CTLFLAG_RD,
    (uint64_t *) &log_stat.oom,
    "" /* sysctl nub: kextlog.statistics.oom */
);

static SYSCTL_QUAD(
    _kextlog_statistics,
    OID_AUTO,
    enqueue_failure,
    CTLFLAG_RD,
    (uint64_t *) &log_stat.enqueue_failure,
    "" /* sysctl nub: kextlog.statistics.enqueue_failure */
);

static struct
sysctl_oid *sysctl_entries[] = {
    /* sysctl nodes */
    &sysctl__kextlog,
    &sysctl__kextlog_statistics,

    /* sysctl nubs */
    &sysctl__kextlog_statistics_syslog,
    &sysctl__kextlog_statistics_heapmsg,
    &sysctl__kextlog_statistics_stackmsg,
    &sysctl__kextlog_statistics_oom,
    &sysctl__kextlog_statistics_enqueue_failure,
};

void
log_sysctl_register(void)
{
    size_t i;
    for (i = 0; i < ARRAY_SIZE(sysctl_entries); i++) {
        sysctl_register_oid(sysctl_entries[i]);
    }

    LOG_DBG("%zu sysctl entries registered", ARRAY_SIZE(sysctl_entries));
}

void
log_sysctl_deregister(void)
{
    size_t i;
    for (i = 0; i < ARRAY_SIZE(sysctl_entries); i++) {
        if (sysctl_entries[i] != NULL) {
            /* Double sysctl_unregister_oid() call causes panic */
            sysctl_unregister_oid(sysctl_entries[i]);
            sysctl_entries[i] = NULL;
        }
    }
    LOG_DBG("%zu sysctl entries deregistered", ARRAY_SIZE(sysctl_entries));
}

