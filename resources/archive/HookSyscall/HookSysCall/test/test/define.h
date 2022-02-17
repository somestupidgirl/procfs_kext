//
//  define.h
//  test
//
//  Created by 五千年木 on 2019/3/19.
//  Copyright © 2019年 osxkernel. All rights reserved.
//

#ifndef define_h
#define define_h

#include <libkern/libkern.h>
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ## __VA_ARGS__)
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ## __VA_ARGS__)

#endif /* define_h */


