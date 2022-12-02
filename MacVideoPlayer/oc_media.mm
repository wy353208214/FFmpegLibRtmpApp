//
//  oc_test.m
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#import "oc_media.h"
#import "media_manager.hpp"

@implementation OcMediaManager

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
    const char *url_ = [url UTF8String];
    myTest->openFile(url_);
}

- (void) pushStream {
    myTest->pushStream();
}

- (void) play: (NSString*) url {
    const char *url_ = [url UTF8String];
    myTest->play(url_);
}

@end
