#include <stdio.h>
#import <UIKit/UIKit.h>
#include <libsunflsks/Network.h>
#include <libsunflsks/SystemStatus.h>
#include <libsunflsks/TextUtils.h>
#include "../include/FileInfo.h"
#include "../include/ProcessManager.h"

int main(int argc, char *argv[], char *envp[]) {
	@autoreleasepool {

		printf("\nSystem Status:\n"
				"Battery: %lld\n"
				"Battery state: %s\n"
				"Tweak Count: %lld\n"
				"Package Count:%lld\n"
				"Process Count:%lld\n"
				"Uptime:%s\n\n"
				"Network Status:\n"
				"Connected to: %s\n\n", [SunflsksSystemStatus batteryPercent],
										[[SunflsksSystemStatus stringWithChargingStatus] UTF8String],
										[SunflsksSystemStatus tweakCount],
										[SunflsksSystemStatus packageCount],
										[SunflsksSystemStatus processCount],
										[[SunflsksSystemStatus stringWithUptime] UTF8String],
										[[SunflsksNetwork connectivityAsString] UTF8String]);

		printf("Open FD's for this process:\n");
		for (SunflsksFileInfo* file in [[[SunflsksProcessManager alloc] initWithPID:getpid()] openFileDescriptors]) {
			if ([file type] != Unknown) {
				printf("%s\n", [[file path] UTF8String]);
			}
		}

		return 0;
	}
}
