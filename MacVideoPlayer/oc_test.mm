//
//  oc_test.m
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#import "oc_test.h"
#import "test.hpp"

@implementation oc_test

MediaManager *myTest;

- (instancetype)init
{
    self = [super init];
    if (self) {
        if (myTest == NULL) {
            myTest = new MediaManager();
        }
    }
    return self;
}

- (void) startRecord: (ORecordType) type {

    switch (type) {
        case OC_VIDEO:
            myTest->startRecord(VIDEO);
            break;
        case OC_AUDIO:
            myTest->startRecord(AUDIO);
            break;
        case OC_ALL:
            myTest->startRecord(ALL);
            break;
        default:
            NSLog(@"Record None");
            break;
    }
}

- (void) stopRecord{
    myTest->stopRecord();
//    delete myTest;
}

- (void) convertPcm2AAC{
    myTest->convertPcm2AAC();
}


- (void) openFile: (NSString*) url {
    const char *ul = [url UTF8String];
    myTest->openFile(ul);
}

- (void) pushStream {
    myTest->pushStream();
}

@end
