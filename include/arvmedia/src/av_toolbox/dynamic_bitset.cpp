#include "dynamic_bitset.h"
#include <algorithm>

namespace ins {

DynamicBitset::DynamicBitset(size_t size) noexcept :data_(size, false) {
}

void DynamicBitset::Set() {
  std::fill(data_.begin(), data_.end(), true);
}

void DynamicBitset::Set(size_t pos, bool value) {
  data_[pos] = value;
}

void DynamicBitset::Reset() {
  std::fill(data_.begin(), data_.end(), false);
}

void DynamicBitset::Reset(size_t pos) {
  data_[pos] = false;
}

void DynamicBitset::Flip() {
  data_.flip();
}

bool DynamicBitset::Test(size_t pos) const {
  return data_[pos];
}

bool DynamicBitset::All() const {
  return std::all_of(data_.begin(), data_.end(), [](bool i) {
    return i == true;
  });
}

bool DynamicBitset::Any() const {
  return std::any_of(data_.begin(), data_.end(), [](bool i) {
    return i == true;
  });
}

bool DynamicBitset::None() const {
  return std::none_of(data_.begin(), data_.end(), [](bool i) {
    return i == true;
  });
}


}
