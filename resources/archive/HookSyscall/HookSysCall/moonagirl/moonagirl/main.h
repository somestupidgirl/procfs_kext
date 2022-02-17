//
//  main.h
//  moonagirl
//
//  Created by 五千年木 on 2019/3/25.
//  Copyright © 2019年 osxkernel. All rights reserved.
//

#ifndef main_h
#define main_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/kern_control.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sys_domain.h>
#include "define_ctl.h"
#include <stdbool.h>
#include <pthread.h>



#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ## __VA_ARGS__)
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ## __VA_ARGS__)

#endif /* main_h */
