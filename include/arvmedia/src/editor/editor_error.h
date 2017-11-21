#pragma once

namespace ins {

const static int kDemuxError = 1;
const static int kDecodeError = 2;
const static int kEncodeError = 3;
const static int kScaleError = 4;
/**
 * \brief User define error code, eg: const static int kErrorStitch = kUserError+1;
 */
const static int kUserError = 10000;

}
