
#include <gtest/gtest.h>
#include <av_toolbox/dynamic_bitset.h>

using namespace ins;
TEST(dynamic_bitset, set) {
  DynamicBitset db(5);
  db.Set();
  ASSERT_EQ(db.All(), true);
  
  db.Set(0, false);
  ASSERT_EQ(db.Test(0), false);

  db.Set(0, true);
  ASSERT_EQ(db.Test(0), true);
}

TEST(dynamic_bitset, flip) {
  DynamicBitset db(5);
  db.Set(0, false);
  db.Set(1, true);
  db.Set(2, true);
  db.Set(3, false);
  db.Set(4, false);
  db.Flip();
  ASSERT_EQ(db.Test(0), true);
  ASSERT_EQ(db.Test(1), false);
  ASSERT_EQ(db.Test(2), false);
  ASSERT_EQ(db.Test(3), true);
  ASSERT_EQ(db.Test(4), true);
}

TEST(dynamic_bitset, reset) {
  DynamicBitset db(5);
  db.Set(0, false);
  db.Set(1, true);
  db.Set(2, true);
  db.Set(3, false);
  db.Set(4, false);
  db.Reset();
  ASSERT_EQ(db.None(), true);
}

TEST(dynamic_bitset, all) {
  DynamicBitset db(5);
  ASSERT_EQ(db.All(), false);
  
  db.Set(0, true);
  ASSERT_EQ(db.All(), false);
  db.Set(1, true);
  ASSERT_EQ(db.All(), false);
  db.Set(2, true);
  ASSERT_EQ(db.All(), false);
  db.Set(3, true);
  ASSERT_EQ(db.All(), false);
  db.Set(4, true);
  ASSERT_EQ(db.All(), true);
}

TEST(dynamic_bitset, any) {
  DynamicBitset db(5);
  ASSERT_EQ(db.Any(), false);
  db.Set(0, true);
  ASSERT_EQ(db.Any(), true);
}

TEST(dynamic_bitset, None) {
  DynamicBitset db(5);
  ASSERT_EQ(db.None(), true);
  db.Set(0, true);
  ASSERT_EQ(db.None(), false);
}
