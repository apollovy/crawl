//
//  main.mm
//  dcss
//
//  Created by Аполлов Юрий Андреевич on 25.06.2022.
//
#import <Foundation/Foundation.h>
#import "SDL_main.h"

extern int DCSS_main(int argc, char* argv[]);

// https://stackoverflow.com/a/15318065/674557
char** cArrayFromNSArray(NSArray* array)
{
    int i, count = array.count;
    char** cargs = (char**) malloc(sizeof(char*) * (count + 1));
    for(i = 0; i < count; i++) {
        NSString *s = array[i];
        const char *cstr = s.UTF8String;
        int len = strlen(cstr);
        char* cstr_copy = (char*) malloc(sizeof(char) * (len + 1));
        strcpy(cstr_copy, cstr);
        cargs[i] = cstr_copy;
    }
    cargs[i] = NULL;
    return cargs;
}

int main(int argc, char* argv[])
{
    NSURL *documentUrl = [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] lastObject];
    NSArray<NSString*>* arguments = NSProcessInfo.processInfo.arguments;
    NSString* datadir = [[[NSBundle mainBundle] bundlePath] stringByAppendingString:@"/data/"];
    NSArray<NSString*>* newArguments = [arguments arrayByAddingObjectsFromArray:@[
//        @"--datadir", datadir,
        @"-dir", [documentUrl path],
//        Unknown option: -extra-opt-first save_dir=/var/mobile/Containers/Data/Application/CF2D2F40-D0BD-47FA-90EE-96503AD21A8D/Documents
        //(std::string *) dir = "/private/var/mobile/Containers/Data/Application/C1603E7D-C54E-46D3-B111-E566278C9833/Library/Application Support/Dungeon Crawl Stone Soup/saves/"

    ]];
    int newArgumentsCount = newArguments.count;
    char** stringArgs = cArrayFromNSArray(newArguments);

    return DCSS_main(newArgumentsCount, stringArgs);
//    return DCSS_main(argc, argv);
}
