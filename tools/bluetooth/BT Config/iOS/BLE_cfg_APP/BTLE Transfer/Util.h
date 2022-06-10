//
//  Util.h
//  BTLE Transfer
//
//  Created by CN3_SD9 on 2016/8/10.
//  Copyright © 2016年 Apple. All rights reserved.
//

#ifndef Util_h
#define Util_h

@interface Util : NSObject

+ (BOOL) IsEnableWIFI;
+ (id)fetchSSIDInfo;
+(void)mac2str:(char *)mac :(char *)str;
@end

#endif /* Util_h */
