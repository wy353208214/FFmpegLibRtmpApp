//
//  oc_test.m
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#import "oc_media.h"
#import "media_manager.hpp"

@implementation OcMediaManager

MediaManager *manager;

- (instancetype)init
{
    self = [super init];
    if (self) {
        if (manager == NULL) {
            manager = new MediaManager();
        }
    }
    return self;
}

- (void) startRecord: (ORecordType) type {

    switch (type) {
        case OC_VIDEO:
            manager->startRecord(VIDEO);
            break;
        case OC_AUDIO:
            manager->startRecord(AUDIO);
            break;
        case OC_ALL:
            manager->startRecord(ALL);
            break;
        default:
            NSLog(@"Record None");
            break;
    }
}

- (void) stopRecord{
    manager->stopRecord();
//    delete myTest;
}

- (void) convertPcm2AAC{
    manager->convertPcm2AAC();
}


- (void) openFile: (NSString*) url {
    const char *url_ = [url UTF8String];
    manager->openFile(url_);
}

- (void) pushStream {
    manager->pushStream();
}

- (void) play: (NSString*) url {
    const char *url_ = [url UTF8String];
    manager->play(url_);
}

- (void) stop {
    manager->stop();
}

- (void) exit {
    delete manager;
}

@end
