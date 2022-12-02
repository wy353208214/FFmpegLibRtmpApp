//
//  oc_test.h
//  MacVideoPlayer
//
//  Created by steven on 2022/9/7.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ORecordType) {
    OC_VIDEO,
    OC_AUDIO,
    OC_ALL
};

@interface OcMediaManager : NSObject

- (void) startRecord: (ORecordType) type;
- (void) stopRecord;
- (void) convertPcm2AAC;
- (void) openFile: (NSString*) url;
- (void) pushStream;
- (void) play: (NSString*) url;

@end

NS_ASSUME_NONNULL_END
