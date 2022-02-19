/*
 * Created 190417 lynnl
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <sys/kern_control.h>
#include <sys/ioctl.h>

#include <libbsdkextlog/kextlog.h>

/*
 * Used to indicate unused function parameters
 * see: <sys/cdefs.h>#__unused
 */
#define UNUSED(e, ...)      (void) ((void) (e), ##__VA_ARGS__)

#define LOG(fmt, ...)       (void) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define LOG_OFF(fmt, ...)   (void) ((void) (fmt), ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)   LOG("[ERR] " fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG("[WARN] " fmt, ##__VA_ARGS__)
#ifdef DEBUG
#define LOG_DBG(fmt, ...)   LOG("[DBG] " fmt, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)   LOG_OFF(fmt, ##__VA_ARGS__)
#endif

#define BUILD_BUG_ON(cond)  UNUSED(sizeof(char[-!!(cond)]))

/**
 * Connect to a kernel control
 * @name        kernel control name
 * @socktype    either SOCK_DGRAM or SOCK_STREAM
 * @return      -1 if failed(errno will be set)  fd otherwise
 */
static int
connect_to_kctl(const char *name, int socktype)
{
    int fd = -1;
    int e;
    struct sockaddr_ctl sc;
    struct ctl_info ci;

    if (strlen(name) >= MAX_KCTL_NAME) {
        LOG_ERR("kernel control socket name too long");
        goto out_exit;
    }

    fd = socket(PF_SYSTEM, socktype, SYSPROTO_CONTROL);
    if (fd == -1) {
        LOG_ERR("socket(2) fail  errno: %d", errno);
        goto out_exit;
    }

    LOG_DBG("socket fd: %d", fd);

    (void) memset(&ci, 0, sizeof(struct ctl_info));
    (void) strncpy(ci.ctl_name, name, MAX_KCTL_NAME-1);
    ci.ctl_name[MAX_KCTL_NAME-1] = '\0';

    /*
     * The control ID is dynamically generated
     * So we must obtain ci.sc_id via ioctl()
     *
     * If ioctl returns ENOENT  it means that socket descriptor is nonexist
     * So before we run the daemon  we must load kext to the kernel
     */
    e = ioctl(fd, CTLIOCGINFO, &ci);
    if (e == -1) {
        LOG_ERR("ioctl(2) CTLIOCGINFO fail  name: %s fd: %d errno: %d",
                    ci.ctl_name, fd, errno);
        goto out_close;
    }

    LOG_DBG("kctl %s id %d", ci.ctl_name, ci.ctl_id);

    (void) memset(&sc, 0, sizeof(sc));
    sc.sc_len = sizeof(struct sockaddr_ctl);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_id = ci.ctl_id;
    sc.sc_unit = 0;

    e = connect(fd, (struct sockaddr *) &sc, sizeof(struct sockaddr_ctl));
    if (e == -1) {
        LOG_ERR("connect(2) fail  fd: %d errno: %d", fd, errno);
        goto out_close;
    }

    LOG("kctl %s connected  fd: %d\n", ci.ctl_name, fd);

out_exit:
    return fd;

out_close:
    (void) close(fd);
    fd = -1;
    goto out_exit;
}

/*
 * User space read buffer should over commit 25% from ctl_recvsize
 * see: xnu/bsd/kern/kern_control.c#ctl_rcvbspace
 *
 * Generally speaking: use more buffer in user space
 */
#define BUFFER_SIZE     24576       /* 8192 * 3 */

static char buffer[BUFFER_SIZE];

static void
read_log_from_kctl(int fd)
{
    struct kextlog_msghdr *m;
    ssize_t n;
    ssize_t i;
    ssize_t concur;

    while (1) {
        n = read(fd, buffer, BUFFER_SIZE);
        if (n < 0) {
            if (errno == EINTR) continue;
            LOG_ERR("read(2) fail  errno: %d", errno);
            break;
        }
        /* If n != BUFFER_SIZE  it means an early socket read wakeup */

        for (i = 0, concur = 0; i + (ssize_t) sizeof(*m) <= n; i += sizeof(*m) + m->size, concur++) {
            m = (struct kextlog_msghdr *) (buffer + i);

            LOG("[%zd:%zd]  pid: %d tid: %#llx ts: %#llx level: %u flags: %#x sz: %u",
                concur, i, m->pid, m->tid, m->timestamp, m->level, m->flags, m->size);

            if (i + (ssize_t) (sizeof(*m) + m->size) > n) {
                LOG_WARN("message body(%u bytes) incomplete  n: %zd", m->size, n);
                break;
            }

            LOG("%.*s\n", (int) m->size, m->buffer);

            if (m->_padding != _KEXTLOG_PADDING_MAGIC) {
                LOG_ERR("bad message magic: %#x", m->_padding);
                assert(m->_padding == _KEXTLOG_PADDING_MAGIC);
            }
        }

        if (i < n) {
            LOG_WARN("%zd bytes left unread in buffer  n: %zd", n - i, n);
        }
    }
}

int
main(int argc, char *argv[])
{
    UNUSED(argc, argv);

    int fd = connect_to_kctl(KEXTLOG_KCTL_NAME, KEXTLOG_KCTL_SOCKTYPE);
    if (fd >= 0) {
        read_log_from_kctl(fd);
        (void) close(fd);
    }
    return fd >= 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

