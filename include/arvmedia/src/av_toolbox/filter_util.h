#pragma once
#include <string>
#include "sp.h"

struct AVBSFContext;
struct AVStream;

namespace ins {

sp<AVBSFContext> NewAVBSFContext(const std::string &filter_name, const AVStream *st);

}
