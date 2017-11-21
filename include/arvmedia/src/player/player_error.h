//
//  player_error.h
//  INSVideoPlayApp
//
//  Created by jerett on 16/2/18.
//  Copyright © 2016 insta360. All rights reserved.
//

#ifndef player_error_h
#define player_error_h


namespace ins {
  const static int kErrorNone = 0;
  const static int kErrorOpenFormat = -1;
  const static int kErrorAgain = -2;
  const static int kErrorEnd = -3;
  const static int kErrorSeek = -4;
  const static int kErrorParseUrl = -4;
  const static int kErrorDemux = -5;
}

#endif /* player_error_h */
