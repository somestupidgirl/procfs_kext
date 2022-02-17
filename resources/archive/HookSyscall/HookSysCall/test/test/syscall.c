//
//  syscall.c
//  test
//
//  Created by 五千年木 on 2019/3/20.
//  Copyright © 2019年 osxkernel. All rights reserved.
//
#include "define.h"
#include "syscall.h"

void *syscall_table[521];
//uint64_t file_num = 0;
//time_t start ,end ;
uint64_t file_table[521] = {0};

//kern_sig
int hook_kill(proc_t cp, struct kill_args *uap, __unused int32_t *retval)
{
    // Negative pid is a killpg case
//    pid_t pid = (uap->pid > 0 ? uap->pid : -uap->pid);
//    
//    if (!g_pid || (pid != g_pid)) {
//        return g_orig_kill(cp, uap, retval);
//    }
//    
//    printf("signal %d from pid %d to pid %d, posix %d\n", uap->signum, proc_pid(cp), uap->pid, uap->posix);
//    
//    // TODO: process cannot ignore or handle SIGKILL so we intercept it here.
//    // However there are other signals that will terminate a process if it doesn't handle or ignore these signals (i.e. SIGTERM)
//    // We don't handle those here for now.
//    if (uap->signum == SIGKILL || uap->signum == SIGTERM) {
//        printf("blocking SIGKILL\n");
//        return EPERM;
//    }
    pid_t pid = (uap->pid > 0 ? uap->pid : -uap->pid);
    LOG_INFO("moonAgirl: MY SYS_KILL.");
    LOG_INFO("KILL PID:%d",pid);
    return ((sy_call_t*)syscall_table[SYS_kill])(cp, uap, retval);
}


//  5	AUE_OPEN_RWTC	ALL	{ int open(user_addr_t path, int flags, int mode) NO_SYSCALL_STUB; }
//vfs_syscalls
//int
//open(proc_t p, struct open_args *uap, int32_t *retval)
//{
//    __pthread_testcancel(1);
//    return(open_nocancel(p, (struct open_nocancel_args *)uap, retval));
//}
//typedef int32_t sy_call_t(struct proc *, void *, int *);
int hook_open(proc_t cp, struct open_args *uap, int32_t *retval)
{
    LOG_INFO("moonAgirl: MY SYS_OPEN.");
    size_t pathlen = strlen((char *)uap->path);
    LOG_INFO("Open Path:%s Len:%d",(char *)uap->path,(int)pathlen);
    
    size_t len = min(strlen("/System/Library/LaunchDaemons"),pathlen);
    int ret1 = strncmp("/System/Library/LaunchDaemons",(char *)uap->path,len);
    len = min(strlen("/Library/LaunchDaemons"),pathlen);
    int ret2 = strncmp("/Library/LaunchDaemons",(char *)uap->path,len);
    len = min(strlen("/System/Library/LaunchAgents"),pathlen);
    int ret3 = strncmp("/System/Library/LaunchAgents",(char *)uap->path,len);
    len = min(strlen("/Library/LaunchAgents"),pathlen);
    int ret4 = strncmp("/Library/LaunchAgents",(char *)uap->path,len);
    len = min(strlen("/Users/moon/Library/LaunchAgents"),pathlen);
    int ret5 = strncmp("/Users/moon/Library/LaunchAgents",(char *)uap->path,len);
    LOG_INFO("ret1:%d ret2:%d ret3:%d ret4:%d ret5:%d",ret1,ret2,ret3,ret4,ret5);
    len = min(strlen("~/Library/LaunchAgents"),pathlen);
    int ret6 = strncmp("~/Library/LaunchAgents",(char *)uap->path,len);
    
    
    if(ret1 == 0 || ret2 == 0 || ret3 == 0 || ret4 == 0 || ret5 == 0 || ret6 == 0){
        LOG_ERROR("malware plst:%s",(char *)uap->path);
        memset(g_string_buf, 0, 256);
        strncpy(g_string_buf,(char *)uap->path, pathlen);
        return -1;
    }
    
    int ret7 = 1;
    ret7 &= strncmp("/Volumes/__.txt",(char *)uap->path,min(strlen("/Volumes/__.txt"),pathlen));
    ret7 &= strncmp("/Users/__.txt",(char *)uap->path,min(strlen("/Users/__.txt"),pathlen));
    ret7 &= strncmp("/Users/moon/__.txt",(char *)uap->path,min(strlen("/Users/moon/__.txt"),pathlen));
    ret7 &= strncmp("/Users/moon/Documents/__.txt",(char *)uap->path,min(strlen("/Users/moon/Documents/__.txt"),pathlen));
    ret7 &= strncmp("/Users/moon/Downloads/__.txt",(char *)uap->path,min(strlen("/Users/moon/Downloads/__.txt"),pathlen));
    ret7 &= strncmp("/Users/moon/Desktop/__.txt",(char *)uap->path,min(strlen("/Users/moon/Desktop/__.txt"),pathlen));
    ret7 &= strncmp("/Users/moon/Library/__.txt",(char *)uap->path,min(strlen("/Users/moon/Library/__.txt"),pathlen));
    ret7 &= strncmp("/Users/moon/Movies/__.txt",(char *)uap->path,min(strlen("/Users/moon/Movies/__.txt"),pathlen));
    ret7 &= strncmp("/Users/moon/Music/__.txt",(char *)uap->path,min(strlen("/Users/moon/Music/__.txt"),pathlen));
    ret7 &= strncmp("/Users/moon/Pictures/__.txt",(char *)uap->path,min(strlen("/Users/moon/Pictures/__.txt"),pathlen));
    if(ret7 == 0){
        LOG_ERROR("malware plst:%s",(char *)uap->path);
        memset(g_string_buf, 0, 256);
        strncpy(g_string_buf,(char *)uap->path, pathlen);
        //return -1;
    }
    
    return ((sy_call_t*)syscall_table[SYS_open])(cp, uap, retval);
}

int hook_rename(struct proc *cp, struct rename_args *uap, int *retval){
    LOG_INFO("moonAgirl: MY SYS_RENAME.");
    size_t pathlen = strlen((char *)uap->to);
    LOG_INFO("Rename to:%s Len:%d",(char *)uap->to,(int)pathlen);
    
    size_t len = min(strlen("/System/Library/LaunchDaemons"),pathlen);
    int ret1 = strncmp("/System/Library/LaunchDaemons",(char *)uap->to,len);
    len = min(strlen("/Library/LaunchDaemons"),pathlen);
    int ret2 = strncmp("/Library/LaunchDaemons",(char *)uap->to,len);
    len = min(strlen("/System/Library/LaunchAgents"),pathlen);
    int ret3 = strncmp("/System/Library/LaunchAgents",(char *)uap->to,len);
    len = min(strlen("/Library/LaunchAgents"),pathlen);
    int ret4 = strncmp("/Library/LaunchAgents",(char *)uap->to,len);
    len = min(strlen("/Users/moon/Library/LaunchAgents"),pathlen);
    int ret5 = strncmp("/Users/moon/Library/LaunchAgents",(char *)uap->to,len);
    LOG_INFO("ret1:%d ret2:%d ret3:%d ret4:%d ret5:%d",ret1,ret2,ret3,ret4,ret5);
    
    len = min(strlen("~/Library/LaunchAgents"),pathlen);
    int ret6 = strncmp("~/Library/LaunchAgents",(char *)uap->to,len);
    
    if(ret1 == 0 || ret2 == 0 || ret3 == 0 || ret4 == 0 || ret5 == 0 || ret6 == 0){
        LOG_ERROR("malware plst:%s",(char *)uap->to);
        memset(g_string_buf, 0, 256);
        strncpy(g_string_buf,(char *)uap->to, pathlen);
        return -1;
    }
    
    return ((sy_call_t*)syscall_table[SYS_rename])(cp, uap, retval);
}

int hook_copyfile(struct proc *cp, struct copyfile_args *uap, int *retval){
    LOG_INFO("moonAgirl: MY SYS_COPYFILE.");
    size_t pathlen = strlen((char *)uap->to);
    LOG_INFO("Copyfile to:%s Len:%d",(char *)uap->to,(int)pathlen);
    
    size_t len = min(strlen("/System/Library/LaunchDaemons"),pathlen);
    int ret1 = strncmp("/System/Library/LaunchDaemons",(char *)uap->to,len);
    len = min(strlen("/Library/LaunchDaemons"),pathlen);
    int ret2 = strncmp("/Library/LaunchDaemons",(char *)uap->to,len);
    len = min(strlen("/System/Library/LaunchAgents"),pathlen);
    int ret3 = strncmp("/System/Library/LaunchAgents",(char *)uap->to,len);
    len = min(strlen("/Library/LaunchAgents"),pathlen);
    int ret4 = strncmp("/Library/LaunchAgents",(char *)uap->to,len);
    len = min(strlen("/Users/moon/Library/LaunchAgents"),pathlen);
    int ret5 = strncmp("/Users/moon/Library/LaunchAgents",(char *)uap->to,len);
    LOG_INFO("ret1:%d ret2:%d ret3:%d ret4:%d ret5:%d",ret1,ret2,ret3,ret4,ret5);
    
    len = min(strlen("~/Library/LaunchAgents"),pathlen);
    int ret6 = strncmp("~/Library/LaunchAgents",(char *)uap->to,len);
    
    if(ret1 == 0 || ret2 == 0 || ret3 == 0 || ret4 == 0 || ret5 == 0 || ret6 == 0){
        LOG_ERROR("malware plst:%s",(char *)uap->to);
        memset(g_string_buf, 0, 256);
        strncpy(g_string_buf,(char *)uap->to, pathlen);
        return -1;
    }
    
    return ((sy_call_t*)syscall_table[SYS_copyfile])(cp, uap, retval);
}

//可能会通过共享内存的方式修改文件
int hook_shm_open(struct proc *cp, struct shm_open_args *uap, int *retval){
    return 0;
}

//59
int hook_execve(struct proc *cp, struct execve_args *uap, int *retval){
//    int i = 0;
//    if(i < 10){
    LOG_INFO("moonAgirl: MY SYS_EXECVE.");
    
    //(char *)uap->argp
    
    int pid = proc_selfpid();
    char name[0x50];
    //proc_name(pid, name, 0x50);
    proc_selfname(name, 0x50);
    
    size_t pathlen = strlen(name);
    size_t len = min(strlen("sh"),pathlen);
    int ret1 = strncmp("sh",name,len);
    len = min(strlen("sudo"),pathlen);
    int ret2 = strncmp("sudo",name,len);
    len = min(strlen("bash"),pathlen);
    int ret3 = strncmp("bash",name,len);
    len = min(strlen("Finder"),pathlen);
    int ret4 = strncmp("Finder",name,len);
    len = min(strlen("ksh"),pathlen);
    int ret5 = strncmp("ksh",name,len);
    len = min(strlen("csh"),pathlen);
    int ret6 = strncmp("csh",name,len);
    
    if(ret1 != 0 && ret2 != 0 && ret3!= 0 && ret4!= 0 && ret5 !=0 && ret6!= 0){
        LOG_INFO("pid:%d name:%s",pid,name);
        memset(execve_string_buf, 0, 256);
        strncpy(execve_string_buf,name, pathlen);
    }

    return ((sy_call_t*)syscall_table[SYS_execve])(cp, uap, retval);
}

//244
int hook_posix_spawn(struct proc *cp, struct posix_spawn_args *uap, int *retval){
    LOG_INFO("moonAgirl: MY SYS_POSIX_SPAWN.");
    
    int pid = proc_selfpid();
    char name[0x50];
    //proc_name(pid, name, 0x50);
    proc_selfname(name, 0x50);
    
    size_t pathlen = strlen(name);
    size_t len = min(strlen("sh"),pathlen);
    int ret1 = strncmp("sh",name,len);
    len = min(strlen("sudo"),pathlen);
    int ret2 = strncmp("sudo",name,len);
    len = min(strlen("bash"),pathlen);
    int ret3 = strncmp("bash",name,len);
    len = min(strlen("Finder"),pathlen);
    int ret4 = strncmp("Finder",name,len);
    len = min(strlen("ksh"),pathlen);
    int ret5 = strncmp("ksh",name,len);
    len = min(strlen("csh"),pathlen);
    int ret6 = strncmp("csh",name,len);
    
    if(ret1 != 0 && ret2 != 0 && ret3!= 0 && ret4!= 0 && ret5 !=0 && ret6!= 0){
        LOG_INFO("pid:%d name:%s",pid,name);
        memset(execve_string_buf, 0, 256);
        strncpy(execve_string_buf,name, pathlen);
    }
    
    return ((sy_call_t*)syscall_table[SYS_posix_spawn])(cp, uap, retval);
}

//380
int hook__mac_execve(struct proc *cp, struct __mac_execve_args *uap, int *retval){
    LOG_INFO("moonAgirl: MY SYS_MAC_EXECVE.");
    
    int pid = proc_selfpid();
    char name[0x50];
    //proc_name(pid, name, 0x50);
    proc_selfname(name, 0x50);
    
    size_t pathlen = strlen(name);
    size_t len = min(strlen("sh"),pathlen);
    int ret1 = strncmp("sh",name,len);
    len = min(strlen("sudo"),pathlen);
    int ret2 = strncmp("sudo",name,len);
    len = min(strlen("bash"),pathlen);
    int ret3 = strncmp("bash",name,len);
    len = min(strlen("Finder"),pathlen);
    int ret4 = strncmp("Finder",name,len);
    len = min(strlen("ksh"),pathlen);
    int ret5 = strncmp("ksh",name,len);
    len = min(strlen("csh"),pathlen);
    int ret6 = strncmp("csh",name,len);
    
    if(ret1 != 0 && ret2 != 0 && ret3!= 0 && ret4!= 0 && ret5 !=0 && ret6!= 0){
        LOG_INFO("pid:%d name:%s",pid,name);
        memset(execve_string_buf, 0, 256);
        strncpy(execve_string_buf,name, pathlen);
    }
    
    return ((sy_call_t*)syscall_table[SYS___mac_execve])(cp, uap, retval);
}

//3
int hook_read(struct proc *cp, struct read_args *uap, user_ssize_t *retval){
    LOG_INFO("moonAgirl: MY SYS_READ.");
    
    //file_num++;
    int pid = proc_selfpid();
    if(file_table[pid%256] != 30){
        file_table[pid%256]++;
    }else{
        char name[0x50];
        //proc_name(pid, name, 0x50);
        proc_selfname(name, 0x50);
        
        size_t pathlen = strlen(name);
        size_t len = min(strlen("sh"),pathlen);
        int ret1 = strncmp("sh",name,len);
        len = min(strlen("moonagirl"),pathlen);
        int ret2 = strncmp("moonagirl",name,len);
        len = min(strlen("bash"),pathlen);
        int ret3 = strncmp("bash",name,len);
        len = min(strlen("Terminal"),pathlen);
        int ret4 = strncmp("Terminal",name,len);
        len = min(strlen("ksh"),pathlen);
        int ret5 = strncmp("ksh",name,len);
        len = min(strlen("csh"),pathlen);
        int ret6 = strncmp("csh",name,len);
        
        if(ret1 != 0 && ret2 != 0 && ret3!= 0 && ret4!= 0 && ret5 !=0 && ret6!= 0){
            LOG_INFO("pid:%d name:%s",pid,name);
            memset(file_string_buf, 0, 256);
            strncpy(file_string_buf,name, strlen(name));
            file_table[pid%256] = 0;
        }
        
    }
    
    //LOG_INFO("Copyfile From:%s To:%s",(char *)uap->from,(char *)uap->to);
    return ((sy_call64_t*)syscall_table[SYS_read])(cp, uap, retval);
}

//4
int hook_write(struct proc *cp, struct writev_args *uap, user_ssize_t *retval){
    LOG_INFO("moonAgirl: MY SYS_WRITE.");
    
    
    int pid = proc_selfpid();
    if(file_table[pid%256] != 30){
        file_table[pid%256]++;
    }else{
        char name[0x50];
        //proc_name(pid, name, 0x50);
        proc_selfname(name, 0x50);
        
        size_t pathlen = strlen(name);
        size_t len = min(strlen("sh"),pathlen);
        int ret1 = strncmp("sh",name,len);
        len = min(strlen("moonagirl"),pathlen);
        int ret2 = strncmp("moonagirl",name,len);
        len = min(strlen("bash"),pathlen);
        int ret3 = strncmp("bash",name,len);
        len = min(strlen("Terminal"),pathlen);
        int ret4 = strncmp("Terminal",name,len);
        len = min(strlen("ksh"),pathlen);
        int ret5 = strncmp("ksh",name,len);
        len = min(strlen("csh"),pathlen);
        int ret6 = strncmp("csh",name,len);
        
        if(ret1 != 0 && ret2 != 0 && ret3!= 0 && ret4!= 0 && ret5 !=0 && ret6!= 0){
            LOG_INFO("pid:%d name:%s",pid,name);
            memset(file_string_buf, 0, 256);
            strncpy(file_string_buf,name, strlen(name));
            file_table[pid%256] = 0;
        }
    }
    
    //LOG_INFO("Copyfile From:%s To:%s",(char *)uap->from,(char *)uap->to);
    return ((sy_call64_t*)syscall_table[SYS_write])(cp, uap, retval);
}

//226
int hook_delete(struct proc *cp, struct delete_args *uap, int *retval){
    LOG_INFO("moonAgirl: MY SYS_DELETE.");
    
    int pid = proc_selfpid();
    if(file_table[pid%256] != 30){
        file_table[pid%256]++;
    }else{
        char name[0x50];
        //proc_name(pid, name, 0x50);
        proc_selfname(name, 0x50);
        
        size_t pathlen = strlen(name);
        size_t len = min(strlen("sh"),pathlen);
        int ret1 = strncmp("sh",name,len);
        len = min(strlen("moonagirl"),pathlen);
        int ret2 = strncmp("moonagirl",name,len);
        len = min(strlen("bash"),pathlen);
        int ret3 = strncmp("bash",name,len);
        len = min(strlen("Terminal"),pathlen);
        int ret4 = strncmp("Terminal",name,len);
        len = min(strlen("ksh"),pathlen);
        int ret5 = strncmp("ksh",name,len);
        len = min(strlen("csh"),pathlen);
        int ret6 = strncmp("csh",name,len);
        
        //char* a;// = kalloc(0x10);
        //MALLOC(a, char *, 0x10, 1, M_ZERO);
        
        if(ret1 != 0 && ret2 != 0 && ret3!= 0 && ret4!= 0 && ret5 !=0 && ret6!= 0){
            LOG_INFO("pid:%d name:%s",pid,name);
            memset(file_string_buf, 0, 256);
            strncpy(file_string_buf,name, strlen(name));
            file_table[pid%256] = 0;
        }
    }
    
    
    //LOG_INFO("Copyfile From:%s To:%s",(char *)uap->from,(char *)uap->to);
    return ((sy_call_t*)syscall_table[SYS_delete])(cp, uap, retval);
}







