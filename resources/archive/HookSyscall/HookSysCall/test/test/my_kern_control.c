//
//  my_kern_control.c
//  test
//
//  Created by 五千年木 on 2019/3/25.
//  Copyright © 2019年 osxkernel. All rights reserved.
//

#include "my_kern_control.h"


#define kEPCommand1            1      /* Operation not permitted */
#define kEPCommand2           2      /* No such file or directory */

char g_string_buf[256];
char execve_string_buf[256];
char file_string_buf[256];

errno_t Do_First_Thing()
{
    return (0);
}


errno_t Do_Command2()
{
    return (0);
}


//ep_ctl.ctl_send = EPHandleWrite;
//ep_ctl.ctl_getopt = EPHandleGet;
//ep_ctl.ctl_setopt = EPHandleSet;
//ep_ctl.ctl_connect = EPHandleConnect;
//ep_ctl.ctl_disconnect = EPHandleDisconnect;


/* A simple setsockopt handler */
errno_t EPHandleSet( kern_ctl_ref ctlref, unsigned int unit, void *userdata, int opt, void *data, size_t len )
{
    int    error = 0;
#if DO_LOG
    LOG_INFO(LOG_ERR, "EPHandleSet opt is %d\n", opt);
#endif
    
//    switch ( opt )
//    {
//        case kEPCommand1:               // program defined symbol
//            error = Do_First_Thing();
//            break;
//            
//        case kEPCommand2:               // program defined symbol
//            error = Do_Command2();
//            break;
//    }
    LOG_INFO("EPHandleSet start");
    switch (opt) {
        case Hello_Control_Set_String:
            len = min((u_int)Max_String_Len,(u_int)len);
            strncpy(g_string_buf,(char *)data, len);
            LOG_INFO("New String:%s",g_string_buf);
            break;
        default:
            error = ENOTSUP;
            break;
    }
    
    return error;
}

/* A simple A simple getsockopt handler */
errno_t EPHandleGet(kern_ctl_ref ctlref, unsigned int unit, void *userdata, int opt, void *data, size_t *len)
{
    int    error = 0;
#if DO_LOG
    LOG_INFO(LOG_ERR, "EPHandleGet opt is %d *****************\n", opt);
#endif
    
    LOG_INFO("EPHandleGet start");
    switch (opt) {
        case Hello_Control_Get_String:
            *len = min((u_int)Max_String_Len,(u_int)*len);
            strncpy(data, g_string_buf, *len);
            break;
        case Hello_Control_Get_Execve:
            *len = min((u_int)Max_String_Len,(u_int)*len);
            strncpy(data, execve_string_buf, *len);
            break;
        case Hello_Control_Get_File:
            *len = min((u_int)Max_String_Len,(u_int)*len);
            strncpy(data, file_string_buf, *len);
            break;
        default:
            error = ENOTSUP;
            break;
    }
    
    
    return error;
}

/* A minimalist connect handler */
errno_t
EPHandleConnect(kern_ctl_ref ctlref, struct sockaddr_ctl *sac, void **unitinfo)
{
#if DO_LOG
    LOG_INFO(LOG_ERR, "EPHandleConnect called\n");
#endif
    LOG_INFO("EPHandleConnect start");
    LOG_INFO("process with pid=%d Connected",proc_selfpid());
    return (0);
}

/* A minimalist disconnect handler */
errno_t
EPHandleDisconnect(kern_ctl_ref ctlref, unsigned int unit, void *unitinfo)
{
#if DO_LOG
    LOG_INFO(LOG_ERR, "EPHandleDisconnect called\n");
#endif
    LOG_INFO("EPHandleDisconnect start");
    LOG_INFO("process with pid=%d disConnected",proc_selfpid());
    return (0);
}

/* A minimalist write handler */
errno_t EPHandleWrite(kern_ctl_ref ctlref, unsigned int unit, void *userdata, mbuf_t m, int flags)
{
#if DO_LOG
    LOG_INFO(LOG_ERR, "EPHandleWrite called\n");
#endif
    return (0);
}

