//
//  url.cpp
//  INSVideoPlayApp
//
//  Created by jerett on 16/2/24.
//  Copyright © 2016 insta360. All rights reserved.
//

#include "url_parser.h"

#include <string>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <functional>

namespace ins {

bool URLParser::Parse(const std::string &url_s) noexcept {
  using namespace std;
  const string prot_end("://");
  string::const_iterator prot_i = search(url_s.begin(), url_s.end(),
                                         prot_end.begin(), prot_end.end());
  protocol_.reserve(distance(url_s.begin(), prot_i));
  transform(url_s.begin(), prot_i,
            back_inserter(protocol_),
            ptr_fun<int, int>(tolower)); // protocol is icase
  if (prot_i == url_s.end()) {
    protocol_ = "file";
    path_ = url_s;
    return true;
  }
  advance(prot_i, prot_end.length());
  string::const_iterator path_i = find(prot_i, url_s.end(), '/');
  host_.reserve(distance(prot_i, path_i));
  transform(prot_i, path_i,
            back_inserter(host_),
            ptr_fun<int, int>(tolower)); // host is icase
  string::const_iterator query_i = find(path_i, url_s.end(), '?');
  path_.assign(path_i, query_i);
  if (query_i != url_s.end())
    ++query_i;
  query_.assign(query_i, url_s.end());
  //    std::cout << " p:" << protocol_ << " h:" << host_ << " path:" << path_
  //        << " query:" << query_ << std::endl;
  return true;
}

}