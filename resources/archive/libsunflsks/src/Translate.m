#import "../include/Translate.h"

@implementation SunflsksTranslator {
    NSMutableDictionary* cache;
    NSLocale* fromLocale;
    NSLocale* toLocale;
    NSURL* cachePath;
    NSString* translatePath;
}

-(SunflsksTranslator*)initWithFile:(NSURL*)url From:(NSLocale*)from To:(NSLocale*)to {
    self = [super init];

    if (self == nil || url == nil || from == nil || to == nil) {
        return nil;
    }

    fromLocale = from;
    toLocale = to;
    cachePath = url;
    translatePath = [NSString
                        stringWithFormat:@"/translate_a/single?client=gtx&sl=%@&tl=%@&dt=t&q=",
                        [from languageCode],
                        [to languageCode]
                    ];

    // Should probably use something like core data or sqlite for this but it will work for now
    cache = [[NSMutableDictionary alloc] initWithContentsOfFile:[url path]];

    if (cache && ![[cache objectForKey:@"language"] isEqualToString:[to languageCode]]) {
        [[NSFileManager defaultManager]removeItemAtPath:[url path] error:nil];
        cache = nil;
    }

    if (cache == nil) {
        cache = [[NSMutableDictionary alloc] init];
        [cache setObject:[to languageCode] forKey:@"language"];

        if ([cache writeToURL:url error:nil] == NO) {
            return nil;
        }
    }

    return self;
}

-(NSString*)translate:(NSString*)stringToTranslate {
    if (stringToTranslate == nil) {
        return nil;
    }

    if ([cache objectForKey:stringToTranslate] != nil) {
        return [cache objectForKey:stringToTranslate];
    }

    NSURL* translateURL = [[NSURL alloc]
                            initWithScheme:@"https"
                            host:@"translate.googleapis.com"
                            path:[NSString stringWithFormat:@"%@%@",
                                    translatePath,
                                    stringToTranslate
                            ]
                        ];

    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] init];
    [request setURL:translateURL];
    [request setHTTPMethod:@"GET"];

    NSData *data;
    data = [NSURLConnection sendSynchronousRequest:request returningResponse:nil error:nil];

    if (data == nil) {
        return nil;
    }

    NSString* translatedString = [[[[NSJSONSerialization
                                        JSONObjectWithData:data
                                        options:kNilOptions
                                        error:nil]
                                        objectAtIndex:0]
                                    objectAtIndex:0]
                                objectAtIndex:0
                            ];
    [cache setObject:translatedString forKey:stringToTranslate];
    [self sync];
    return translatedString;
}

-(BOOL)sync {
    return [cache writeToURL:cachePath error:nil];
}

@end
