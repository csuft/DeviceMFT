//
// Created by jerett on 16/6/15.
// Copyright (c) 2016 Insta360. All rights reserved.
//
#pragma once

//#define ON_WIN64(STATEMENT)
//#define ON_APPLE(STATEMENT) 
//#define ON_ANDORID(STATEMENT)
//#define EXECUTE_STATEMENT(STATEMENT) STATEMENT

#ifdef _WIN32
//define something for Windows (32-bit and 64-bit, this part is common)
  #ifdef _WIN64
    //#undef ON_WIN64
    //#define ON_WIN64(STATEMENT) EXECUTE_STATEMENT(STATEMENT)
    #define ON_WIN64 1
  #endif
#elif __APPLE__
  //#undef _APPLE
  //#define ON_APPLE(STATEMENT) EXECUTE_STATEMENT(STATEMENT)
  #define ON_APPLE 1
  #include "TargetConditionals.h"
  #if TARGET_IPHONE_SIMULATOR
    // iOS Simulator
  #elif TARGET_OS_IPHONE
    // iOS device
  #elif TARGET_OS_MAC
    // Other kinds of Mac OS
  #else
    #   error "Unknown Apple platform"
  #endif
#elif __linux__
// linux
#elif __unix__ // all unices not caught above
// Unix
#elif defined(_POSIX_VERSION)
// POSIX
#else
#   error "Unknown compiler"
#endif
