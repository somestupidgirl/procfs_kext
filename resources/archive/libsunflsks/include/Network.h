#pragma once

@interface SunflsksNetwork : NSObject
+(BOOL)checkIfConnected;
+(BOOL)checkIfUp:(NSURL*)url;
+(NSString*)connectivityAsString;
@end