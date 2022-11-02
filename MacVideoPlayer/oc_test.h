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

@interface oc_test : NSObject

- (void) startRecord: (ORecordType) type;
- (void) stopRecord;
- (void) convertPcm2AAC;
- (void) openFile: (NSString*) url;
- (void) pushStream;


@end

NS_ASSUME_NONNULL_END
