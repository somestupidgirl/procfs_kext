#import "../include/ProcessInfo.h"
#include <sys/sysctl.h>

@implementation SunflsksProcessInfo {
	NSString* name;
	pid_t pid;
	char niceness;
	int uptime;
}

-(SunflsksProcessInfo*)initWithProc:(struct extern_proc)proc {
	self = [super init];

	if (!self) {
		return nil;
	}

	pid = proc.p_pid;
	name = [NSString stringWithFormat:@"%s", proc.p_comm];
	niceness = proc.p_nice;
	uptime = proc.p_cpticks / sysconf(_SC_CLK_TCK);

	return self;
}

-(pid_t)pid {
	return pid;
}

-(NSString*)name {
	return name;
}

-(NSString*)description {
	return [NSString
			stringWithFormat:@"Name: %@, PID: %d, Niceness:%d, Uptime:%d",
			name,
			pid,
			niceness,
			uptime
		];
}

@end