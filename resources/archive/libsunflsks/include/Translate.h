#pragma once

@interface SunflsksTranslator : NSObject
-(SunflsksTranslator*)initWithFile:(NSURL*)url From:(NSLocale*)from To:(NSLocale*)to;
-(NSString*)translate:(NSString*)stringToTranslate;
-(BOOL)sync;
@end