#pragma once
#include <vector>

namespace ins {
class DynamicBitset {
public:
  /**
   * \brief Construct bitset, default value is false
   * \param size Bitset size
   */
  DynamicBitset(size_t size) noexcept;
  ~DynamicBitset() = default;

  /**
   * \brief Set all bit true
   */
  void Set();
  /**
   * \brief Set pos bit state
   * \param pos Position
   * \param value State to set
   */
  void Set(size_t pos, bool value = true);
  /**
   * \brief Reset all bit is false
   */
  void Reset();
  /**
   * \brief Reset target position false
   * \param pos Position bit to set
   */
  void Reset(size_t pos);
  /**
   * \brief Flips bits, i.e. changes true values to false and false values to true
   */
  void Flip();
  /**
   * \brief Returns the value of the bit at the position pos.
   * \param pos The bit position
   * \return The value of the bit at the position pos.
   */
  bool Test(size_t pos) const;
  /**
   * \brief Checks if all of the bits are set to true.
   * \return 
   */
  bool All() const;
  /**
   * \brief Checks if any of the bits are set to true.
   * \return 
   */
  bool Any() const;
  /**
   * \brief Checks if none of the bits are set to true.
   * \return 
   */
  bool None() const;

private:
  std::vector<bool> data_;
};

}
