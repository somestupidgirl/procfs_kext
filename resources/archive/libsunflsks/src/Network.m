#import "../lib/Reachability.h"
#import "../include/Network.h"
#import <CoreTelephony/CTCarrier.h>
#import <CoreTelephony/CTTelephonyNetworkInfo.h>

@implementation SunflsksNetwork

+(BOOL)checkIfUp:(NSURL*)URL {
	Reachability* reachability = [Reachability reachabilityForInternetConnection];
	NetworkStatus networkStatus = [reachability currentReachabilityStatus];

	if (networkStatus == NotReachable) {
		return FALSE;
	}

	NSMutableURLRequest* request = [NSMutableURLRequest
									requestWithURL:URL
									cachePolicy:NSURLRequestReloadIgnoringCacheData
									timeoutInterval:4
								];

	[request setHTTPMethod: @"HEAD"];
	NSURLResponse* response;
	NSError* error;
	NSData* data = [NSURLConnection
					sendSynchronousRequest: request
					returningResponse: &response
					error: &error
				];

	if (data) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

+(BOOL)checkIfConnected {
    Reachability* reachability = [Reachability reachabilityForInternetConnection];
	NetworkStatus networkStatus = [reachability currentReachabilityStatus];

	if (networkStatus == NotReachable) {
		return FALSE;
	}
    return TRUE;
}

+(NSString*)connectivityAsString {
	NSString* string;
	Reachability* reachability = [Reachability reachabilityForInternetConnection];
	NetworkStatus networkStatus = [reachability currentReachabilityStatus];

	if (networkStatus == ReachableViaWiFi) {
		string = @"Wi-Fi";
	}

	else if (networkStatus == ReachableViaWWAN) {
		string = @"Cellular";
	}

	else {
		string = @"Error";
	}

	return string;
}

+(NSString*)nameOfLockedProvider {
	CTTelephonyNetworkInfo* networkInfo = [[CTTelephonyNetworkInfo alloc] init];
	return [[networkInfo subscriberCellularProvider] carrierName];
}

@end