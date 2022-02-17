#pragma once
#include "FileInfo.h"
typedef enum ProcessOutput {
    NOT_ALLOWED,
    NONEXISTENT_PID,
    INVALID_SIGNAL,
    SUCCESS
} ProcessOutput;

@interface SunflsksProcessManager : NSObject
-(SunflsksProcessManager*)initWithPID:(pid_t)PID;
-(ProcessOutput)kill;
-(ProcessOutput)kill:(int)signal;
-(NSArray<SunflsksFileInfo*>*)openFileDescriptors;
@end