/* Translate a simple sentence from English to French */
#import "Translate/Translate.h"
int main(int argc, char** argv, char** envp) {
    @autoreleasepool {
        char* str[999];
        printf("I am going to translate this to french: ");
        scanf("%s", str);

        Translate* translate = [[Translate alloc] initWithFile:[NSURL URLWithString:@"file:///tmp/test"] From:[NSLocale localeWithLocaleIdentifier:@"en_US"] To:[NSLocale localeWithLocaleIdentifier:@"fr"]];
        /* the file is the cache file from storing translations, so we don't hamme google all the time */
        NSLog([translate translate:@"The spectacle before us was indeed sublime."]);
    }
}