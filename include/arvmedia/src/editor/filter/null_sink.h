#pragma once
#include "media_filter.h"

namespace ins {

/**
 * \brief NullSink Dummy sink
 */
template <typename DataType>
class NullSink: public MediaSink<DataType> {
public:
  bool Init(MediaContext *ctx) override { return true; }
  bool Filter(MediaContext *ctx, const DataType &data) override { return true; }
  void Close(MediaContext *ctx) override { ; }
};

}


