#pragma once
#include <sys/sysctl.h>
#include "FileInfo.h"
@interface SunflsksProcessInfo : NSObject
-(SunflsksProcessInfo*)initWithProc:(struct extern_proc)proc;
-(pid_t)pid;
-(NSString*)name;
-(NSString*)description;
@end
