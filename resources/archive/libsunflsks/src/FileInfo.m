#import "../include/FileInfo.h"

static NSString* get_socket_info_from_fd(struct socket_fdinfo info);

@implementation SunflsksFileInfo {
    pid_t _pid;
    FDType _type;
    NSString* _path;
    int _fd;
}

-(SunflsksFileInfo*)initWithProcFdInfo:(struct proc_fdinfo)info pid:(pid_t) pid {
    self = [super init];
    if (!self) {
        return nil;
    }

    _fd = info.proc_fd;
    _pid = pid;

    switch (info.proc_fdtype) {

        // Normal filesystem vnode
        case PROX_FDTYPE_VNODE: {
            _type = File;
            struct vnode_fdinfowithpath vnode_info;
            if (proc_pidfdinfo(pid, info.proc_fd, PROC_PIDFDVNODEPATHINFO,
                &vnode_info, PROC_PIDFDVNODEPATHINFO_SIZE) == -1) {
                    return nil;
            }

            _path = [NSString stringWithFormat:@"%s", vnode_info.pvip.vip_path];
        }
        break;

        // Socket fd
        case PROX_FDTYPE_SOCKET: {
            _type = Socket;
            struct socket_fdinfo socket_info;
            if (proc_pidfdinfo(pid, info.proc_fd, PROC_PIDFDSOCKETINFO,
                &socket_info, PROC_PIDFDSOCKETINFO_SIZE) == -1) {
                    return nil;
            }

            _path = get_socket_info_from_fd(socket_info);
        }
        break;

        case PROX_FDTYPE_PIPE: {
            _type = Pipe;
            struct pipe_fdinfo pipe_info;
            if (proc_pidfdinfo(pid, info.proc_fd, PROC_PIDFDPIPEINFO,
                &pipe_info, PROC_PIDFDPIPEINFO_SIZE) == -1) {
                    return nil;
            }

            _path = [NSString stringWithFormat:@"UNIX Pipe: %llu", pipe_info.pipeinfo.pipe_handle];
        }
        break;

        default:
            _type = Unknown;
            _path = @"Unknown type";
    }

    return self;
}

-(NSString*)path {
    return _path;
}

-(int)fd {
    return _fd;
}

-(FDType)type {
    return _type;
}

-(NSString*)description {
    return [_path isEqualToString:@"Unknown type"]
    ? _path
    : [NSString
        stringWithFormat:@"%@ with fd %d with pid %d and type %d", _path, _fd, _pid, _type
    ];
}

@end

static NSString* get_socket_info_from_fd(struct socket_fdinfo info) {
    NSString* ret = nil;

    switch(info.psi.soi_kind) {
        // TCP/IP socket
        case SOCKINFO_TCP: {
            struct in_sockinfo in_sock = info.psi.soi_proto.pri_tcp.tcpsi_ini;
            ret = [NSString
                    stringWithFormat:@"TCP socket: [%d -> %d]",
                    in_sock.insi_lport,
                    in_sock.insi_fport
                ];
        }
        break;

        // UNIX domain socket
        case SOCKINFO_UN: {
            struct un_sockinfo un_sock = info.psi.soi_proto.pri_un;
            char* path = un_sock.unsi_addr.ua_sun.sun_path;
            ret = [NSString stringWithFormat:@"UNIX Domain socket: %s", path];
        }
    }

    return ret;
}