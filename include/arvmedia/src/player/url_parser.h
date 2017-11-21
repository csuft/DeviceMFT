//
//  url.hpp
//  INSVideoPlayApp
//
//  Created by jerett on 16/2/24.
//  Copyright © 2016 insta360. All rights reserved.
//

#ifndef url_h
#define url_h

#include <string>

namespace ins {
  class URLParser {
  public:
    URLParser() = default;
    URLParser(const URLParser&) = default;
    URLParser& operator=(const URLParser&) = default;
    
    bool Parse(const std::string &url_s) noexcept;
    
    const std::string& protocol() const noexcept {
      return protocol_;
    }
    
    const std::string& host() const noexcept {
      return host_;
    }
    
    const std::string& path() const noexcept {
      return path_;
    }
    
    const std::string& query() const noexcept {
      return query_;
    }
    
  private:
    std::string protocol_;
    std::string host_;
    std::string path_;
    std::string query_;
  };
}


#endif /* url_h */
