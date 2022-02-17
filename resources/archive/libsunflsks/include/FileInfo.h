#pragma once
#import "../lib/libproc.h"
#import "../lib/proc_info.h"

typedef enum {
    File,
    Socket,
    Pipe,
    Unknown,
} FDType;

@interface SunflsksFileInfo : NSObject
-(SunflsksFileInfo*)initWithProcFdInfo:(struct proc_fdinfo)info pid:(pid_t) pid;
-(NSString*)path;
-(FDType)type;
-(int)fd;
@end