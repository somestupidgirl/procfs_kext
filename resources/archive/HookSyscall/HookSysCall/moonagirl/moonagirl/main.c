//
//  main.c
//  moonagirl
//
//  Created by 五千年木 on 2019/3/25.
//  Copyright © 2019年 osxkernel. All rights reserved.
//

#include "main.h"

struct ctl_info ctl_info;
struct sockaddr_ctl sc;
char str[Max_String_Len];
char str_pid[Max_String_Len];
char str_file[Max_String_Len];
bool hooked = false;
bool sys_table_bool[521];

long malware_tale[256] = {0};
long black_list[256] = {0};
long white_list[256] = {0};
size_t malware_num = 0;
size_t black_num = 0;
size_t white_num = 0;

void install_kext(){
    if(hooked){
        puts("\033[31mHave Installed.\033[0m");
    }
    else{
        system("sudo cp -rf ./test.kext /System/Library/Extensions/");
        system("sudo chown -R root:wheel /System/Library/Extensions/test.kext");
        system("sudo kextload /System/Library/Extensions/test.kext");
        system("sudo dmesg | tail -n 10");
        hooked = true;
    }
}

void uninstall_kext(){
    if(hooked){
        system("sudo sysctl -w debug.killhook.pid=1");
        system("sudo kextunload /System/Library/Extensions/test.kext");
        system("sudo dmesg | tail -n 10");
        hooked = false;
    }
    else
        puts("\033[31mPlease install kext.\033[0m");
}

void menu(){
    puts("_     _             _        _                ______      _ _  ");
    puts("| |   | |           | |      | |              / _____)    | | |");
    puts("| |__ | | ___   ___ | |  _   \\ \\  _   _  ___| /      ____| | |");
    puts("|  __)| |/ _ \\ / _ \\| | / )   \\ \\| | | |/___) |     / _  | | |");
    puts("| |   | | |_| | |_| | |< ( _____) ) |_| |___ | \\____( ( | | | |");
    puts("|_|   |_|\\___/ \\___/|_| \\_|______/ \\__  (___/ \\______)_||_|_|_|");
    puts("                                  (____/                       ");
    puts("-------------------\033[34mWelcome To HookSysCall Game\033[0m-------------------");
    puts("-----------------------\033[36mCopyRight@moonAgirl\033[0m-----------------------");
    puts("------------------------\033[33mInput your choice\033[0m------------------------");
    puts("1.Install Kext");
    puts("2.Uninstall Kext");
    puts("3.Hook Syscall");
    puts("4.Unhook Syscall");
    puts("5.Start_monitoring");
    puts("6.Stop_monitoring");
    puts("7.Show Log");
    puts("8.Show intercepted malware list");
    puts("9.Exit");
    write(1,">",1);
}

void hook_syscall(){
    if(hooked){
        char str[0x100];
        puts("syscall id:");
        int id;
        scanf("%d",&id);
        if(sys_table_bool[id]){
            puts("\033[31mThis syscall have hooked.\033[0m");
        }else{
            sprintf(str,"sudo sysctl -w debug.killhook.hooked=%d",id);
            system(str);
            system("sudo dmesg | tail -n 10");
            sys_table_bool[id] = true;
        }
    }
    else
        puts("\033[31mPlease install kext.\033[0m");
}

void unhook_syscall(){
    if(hooked){
        char str[0x100];
        puts("syscall id:");
        int id;
        scanf("%d",&id);
        if(sys_table_bool[id]){
            sprintf(str,"sudo sysctl -w debug.killhook.unhook=%d",id);
            system(str);
            system("sudo dmesg | tail -n 10");
            sys_table_bool[id] = false;
        }else{
            puts("\033[31mThis syscall do not hooked.\033[0m");
        }
    }
    else
        puts("\033[31mPlease install kext.\033[0m");
}

int read_mbuf(char *buf, int length)
{
    char tmp_ch;
    int i;
    for(i=0; i<length; i++)
    {
        read(0, &tmp_ch, 1);
        if(tmp_ch == '\n')
            break;
        buf[i] = tmp_ch;
    }
    return i;
}

int get_choice()
{
    char tmp_buf[10];
    int length = read_mbuf(tmp_buf, 6);
    if(length<=0)
        return -1;
    return atoi(tmp_buf);
}

void start(){
    int i;
    for(i = 0;i < 521;i++)
        sys_table_bool[i] = false;
//    malware_tale = malloc(256*sizeof(char *));
    for(size_t i = 0;i < 256;i++){
        malware_tale[i] = 0;
    }
}

int checkexsit(char *name){
    int result = 0;
    for(size_t i = 0;i < malware_num;i++){
        if(malware_tale[i])
        {
            if(strstr(malware_tale[i],name)){
                result = 1;
                return result;
            }
            
        }
    }
    return result;
}

void monitoring(){
    while (1) {
        int sock = socket(PF_SYSTEM,SOCK_DGRAM,SYSPROTO_CONTROL);
        if(sock < 0){
            LOG_INFO("\033[31msocket filed.\033[0m");
            exit(0);
        }
        
        bzero(&ctl_info,sizeof(struct ctl_info));
        strcpy(ctl_info.ctl_name,BUNDLE_ID);
        
        if(ioctl(sock, CTLIOCGINFO,&ctl_info) == -1){
            LOG_INFO("\033[31mioctl filed.\033[0m");
             exit(0);
        }
        
        bzero(&sc, sizeof(struct sockaddr_ctl));
        sc.sc_len = sizeof(struct sockaddr_ctl);
        sc.sc_family = AF_SYSTEM;
        sc.ss_sysaddr = SYSPROTO_CONTROL;
        sc.sc_id = ctl_info.ctl_id;
        sc.sc_unit = 0;
        
        if(connect(sock, (struct sockaddr *)&sc, sizeof(struct sockaddr_ctl))){
            LOG_INFO("\033[31mconnect filed.\033[0m");
             exit(0);
        }
        
        unsigned int size = Max_String_Len;
        unsigned int size_pid = Max_String_Len;
        unsigned int size_file = Max_String_Len;
        
        memset(str,'\x00',256);
        memset(str_pid,'\x00',256);
        memset(str_file, '\x00', 256);
        
        if(getsockopt(sock, SYSPROTO_CONTROL, Hello_Control_Get_String, &str, &size) == -1){
            LOG_INFO("\033[31mgetsockopt filed Hello_Control_Get_String.\033[0m");
             exit(0);
        }
        
        if(getsockopt(sock, SYSPROTO_CONTROL, Hello_Control_Get_Execve, &str_pid, &size_pid) == -1){
            LOG_INFO("\033[31mgetsockopt filed Hello_Control_Get_Execve.\033[0m");
            exit(0);
        }
        
        if(getsockopt(sock, SYSPROTO_CONTROL, Hello_Control_Get_File, &str_file, &size_file) == -1){
            LOG_INFO("\033[31mgetsockopt filed Hello_Control_Get_File.\033[0m");
            exit(0);
        }

//        Malware 0:[File]OSX_Mokes
//        Malware 1:[Lanuch]/Users/moon/Library/LaunchAgents/profile[Lanuch]/System/Libraryrary/LaunchAgents/profile[File]tmds
//        Malware 2:[Lanuch]/System/Libraryrary/LaunchAgents/profile[File]tmds
//        Malware 3:[Lanuch]/Users/moon/Library/LaunchAgents/profiled.plist
//        Malware 4:[File]tmds
//        Malware 5:[Lanuch]/System/Libraryrary/LaunchAgents/profile[Lanuch]/Userem/Libraryrary/LaunchAgents/profile[Lanuch]/System/Libraryrary/LaunchAgents/profiled.plist
//        Malware 6:[Lanuch]/Userem/Libraryrary/LaunchAgents/profile[Lanuch]/System/Libraryrary/LaunchAgents/profiled.plist
//        Malware 7:[Lanuch]/System/Libraryrary/LaunchAgents/profiled.plist
        
        size_t str_len = strlen(str) > Max_String_Len ? Max_String_Len:strlen(str);
        if((strstr(str,"__.txt") || strstr(str,"LaunchAgents") || strstr(str, "LaunchDaemons")) && malware_num != 20){
            if(malware_tale[malware_num] == 0 && str_len != 0 ){
                if(!checkexsit(str)){
                    malware_tale[malware_num] = malloc(str_len + 0x10);
                    memset(malware_tale[malware_num], '\x00', str_len);
                    //strncpy((char *)malware_tale[malware_num], str,str_len);
                    strncpy((char *)malware_tale[malware_num], "[Lanuch]",strlen("[Lanuch]"));
                    strncat((char *)malware_tale[malware_num], str,str_len);
//                    if(!checkexsit(malware_tale[malware_num])) malware_num++;
//                    else {
//                        free(malware_tale[malware_num]);
//                        malware_tale[malware_num] = 0;
//                    }
                    malware_num++;
                }
            }
        }
        
        size_t str_len_pid = strlen(str_pid) > Max_String_Len ? Max_String_Len:strlen(str_pid);
        if(strstr(str_pid,"WoodABoy") == NULL && malware_num != 20){
            if(malware_tale[malware_num] == 0 && str_len_pid != 0){
                if(!checkexsit(str_pid)){
                    malware_tale[malware_num] = malloc(str_len_pid + 0x10);
                    memset(malware_tale[malware_num], '\x00', str_len_pid);
                    //strncpy((char *)malware_tale[malware_num], str_pid,str_len_pid);
                    strncpy((char *)malware_tale[malware_num], "[Execve]",strlen("[Execve]"));
                    strncat((char *)malware_tale[malware_num], str_pid,str_len_pid);
//                    if(!checkexsit(malware_tale[malware_num])) malware_num++;
//                    else {
//                        free(malware_tale[malware_num]);
//                        malware_tale[malware_num] = 0;
//                    }
                    malware_num++;
                }
            }
        }
        
        size_t str_len_file = strlen(str_file) > Max_String_Len ? Max_String_Len:strlen(str_file);
        if(strstr(str_file,"WoodABoy") == NULL && malware_num != 20){
            if(malware_tale[malware_num] == 0 && str_len_file != 0){
                if(!checkexsit(str_file)){
                    malware_tale[malware_num] = malloc(str_len_file + 0x10);
                    memset(malware_tale[malware_num], '\x00', str_len_file);
                    strncpy((char *)malware_tale[malware_num], "[File]",strlen("[File]"));
                    strncat((char *)malware_tale[malware_num], str_file,str_len_file);
//                    if(!checkexsit(malware_tale[malware_num])) malware_num++;
//                    else {
//                        free(malware_tale[malware_num]);
//                        malware_tale[malware_num] = 0;
//                    }
                    malware_num++;
                }
            }
        }

        close(sock);
    }
}

void mkdir_files(){
    system("touch /Volumes/__.txt");
    system("touch /Users/__.txt");
    system("touch /Users/moon/__.txt");
    system("touch /Users/moon/Documents/__.txt");
    system("touch /Users/moon/Downloads/__.txt");
    system("touch /Users/moon/Desktop/__.txt");
    
    system("touch /Users/moon/Library/__.txt");
    system("touch /Users/moon/Movies/__.txt");
    system("touch /Users/moon/Music/__.txt");
    system("touch /Users/moon/Pictures/__.txt");
}

void rm_files(){
    system("rm -f /Volumes/__.txt");
    system("rm -f /Users/__.txt");
    system("rm -f /Users/moon/__.txt");
    system("rm -f /Users/moon/Documents/__.txt");
    system("rm -f /Users/moon/Downloads/__.txt");
    system("rm -f /Users/moon/Desktop/__.txt");
    
    system("rm -f /Users/moon/Library/__.txt");
    system("rm -f /Users/moon/Movies/__.txt");
    system("rm -f /Users/moon/Music/__.txt");
    system("rm -f /Users/moon/Pictures/__.txt");
}

pthread_t t0 = NULL;
void start_monitoring(){
    if(hooked){
        //pthread_t t0;
        // 创建线程A
        mkdir_files();
        if(pthread_create(&t0, NULL, monitoring, NULL) == -1){
            puts("fail to create pthread t0");
            exit(1);
        }
    }else
        puts("\033[31mPlease install kext.\033[0m");
    LOG_INFO("Starting Monitoring...");
}

int sys_ctl(){
    if(hooked){
        int sock = socket(PF_SYSTEM,SOCK_DGRAM,SYSPROTO_CONTROL);
        if(sock < 0){
            LOG_INFO("\033[31msocket filed.\033[0m");
            return -1;
        }
    
        bzero(&ctl_info,sizeof(struct ctl_info));
        strcpy(ctl_info.ctl_name,BUNDLE_ID);
    
        if(ioctl(sock, CTLIOCGINFO,&ctl_info) == -1){
            LOG_INFO("\033[31mioctl filed.\033[0m");
            return -1;
        }
    
        bzero(&sc, sizeof(struct sockaddr_ctl));
        sc.sc_len = sizeof(struct sockaddr_ctl);
        sc.sc_family = AF_SYSTEM;
        sc.ss_sysaddr = SYSPROTO_CONTROL;
        sc.sc_id = ctl_info.ctl_id;
        sc.sc_unit = 0;
    
        if(connect(sock, (struct sockaddr *)&sc, sizeof(struct sockaddr_ctl))){
            LOG_INFO("\033[31mconnect filed.\033[0m");
            return -1;
        }
    
        unsigned int size = Max_String_Len;
        
        memset(str,0,256);
        
        if(getsockopt(sock, SYSPROTO_CONTROL, Hello_Control_Get_String, &str, &size) == -1){
            LOG_INFO("\033[31mgetsockopt filed.\033[0m");
            return -1;
        }
        
        size_t str_len = strlen(str) > Max_String_Len ? Max_String_Len:strlen(str);
        LOG_INFO("Kernel string:%s Len:%d",str,str_len);
        //size_t str_len = strlen(str) > Max_String_Len ? Max_String_Len:strlen(str);
        if(strstr(str,"LaunchAgents") || strstr(str, "LaunchDaemons")){
            if(malware_tale[malware_num] == 0){
                malware_tale[malware_num] = malloc(str_len);
                strncpy((char *)malware_tale[malware_num], str,str_len);
                malware_num++;
            }
        }
//        strcpy(str, "Hello Kernel.");
//        if(setsockopt(sock, SYSPROTO_CONTROL, Hello_Control_Set_String, str, (socklen_t)strlen(str)) == -1){
//            LOG_INFO("\033[31msetsockopt filed.\033[0m");
//            return -1;
//        }
        
        close(sock);
//        system("sudo dmesg | tail -n 10");
    }else
        puts("\033[31mPlease install kext.\033[0m");
    return 0;
}

void show_intercept_malware(){
    LOG_INFO("Intercepted malware list");
    LOG_INFO("There are \033[31m %d \033[0m malware",malware_num);
    for(size_t i = 0;i < malware_num;i++){
        if(malware_tale[i])
        {
            printf("Malware %d:%s\n",(int)i,(char *)malware_tale[i]);
        }
    }
}

void bye(){
    if(malware_num){
        for(size_t i = 0;i < malware_num;i++){
            if(malware_tale[i]){
                free(malware_tale[i]);
                malware_tale[i] = 0;
            }
        }
    }
    if(black_num){
        for(size_t i = 0;i < black_num;i++){
            if(black_list[i]){
                free(black_list[i]);
                black_list[i] = 0;
            }
        }
    }
    if(white_num){
        for(size_t i = 0;i < white_num;i++){
            if(white_list[i]){
                free(white_list[i]);
                white_list[i] = 0;
            }
        }
    }
    exit(0);
}

void stop_monitoring(){
    if(t0 != NULL){
        void * result;
        pthread_cancel(t0);//取消线程
        pthread_join(t0, &result);
        LOG_INFO("Stop monitoring:%d",(int)result);
        rm_files();
    }else{
        LOG_INFO("\033[31mPlease start monitoring\033[0m");
    }
}

int main(){    
    start();
    int choice;
    while (1) {
        menu();
        choice = get_choice();
        switch (choice) {
            case 1:
                install_kext();
                break;
            case 2:
                uninstall_kext();
                break;
            case 3:
                hook_syscall();
                break;
            case 4:
                unhook_syscall();
                break;
            case 5:
                start_monitoring();
                break;
            case 6:
                stop_monitoring();
                break;
            case 7:
                system("sudo dmesg | tail -n 10");
                break;
            case 8:
                show_intercept_malware();
                break;
            case 9:
                if(hooked)
                    puts("\033[31mPlease uninsatll kext.\033[0m");
                else
                    bye();
                break;
            default:
                puts("\033[31mError Choice.\033[0m");
                break;
        }
    }
    return 0;
}
