
#include <gtest/gtest.h>
#include <string>
#include <av_toolbox/dynamic_circular_buffer.h>
#include <llog/llog.h>
#include <algorithm>
#include <numeric>

TEST(dynamic_circular_buffer, empty) {
  ins::dynamic_circular_buffer<int> circular_buffer;
  ASSERT_TRUE(circular_buffer.empty());

  circular_buffer.set_capacity(1000);
  ASSERT_TRUE(circular_buffer.empty());
  ASSERT_EQ(circular_buffer.capacity(), 1000);
  ASSERT_EQ(circular_buffer.size(), 0);

  circular_buffer.push_back(10);
  ASSERT_FALSE(circular_buffer.empty());
  ASSERT_EQ(circular_buffer.size(), 1);
}

TEST(dynamic_circular_buffer, push_back) {
  ins::dynamic_circular_buffer<int> circular_buffer(2);

  for (int i = 0; i < 10; i++) {
    circular_buffer.push_back(i);
  }
  ASSERT_EQ(circular_buffer[0], 8);
  ASSERT_EQ(circular_buffer[1], 9);
  ASSERT_EQ(circular_buffer.size(), 2);
  ASSERT_FALSE(circular_buffer.empty());
}

TEST(dynamic_circular_buffer, push_front) {
  ins::dynamic_circular_buffer<int> circular_buffer(2);

  for (int i = 0; i < 10; i++) {
    circular_buffer.push_front(i);
    //std::cout << " sum:" << std::accumulate(circular_buffer.begin(), circular_buffer.end(), 0) << std::endl;
  }
  ASSERT_EQ(circular_buffer[0], 9);
  ASSERT_EQ(circular_buffer[1], 8);
  ASSERT_EQ(circular_buffer.size(), 2);
  ASSERT_FALSE(circular_buffer.empty());
}

TEST(dynamic_circular_buffer, pop) {
  ins::dynamic_circular_buffer<int> circular_buffer(10);
  circular_buffer.push_back(1);
  circular_buffer.push_back(2);
  circular_buffer.push_back(3);

  circular_buffer.pop_front();
  ASSERT_EQ(circular_buffer.size(), 2);
  ASSERT_EQ(circular_buffer[0], 2);
  ASSERT_EQ(circular_buffer[1], 3);

  circular_buffer.pop_back();
  ASSERT_EQ(circular_buffer.size(), 1);
  ASSERT_EQ(circular_buffer[0], 2);
}


TEST(dynamic_circular_buffer, copy_construct) {
  ins::dynamic_circular_buffer<std::string> tmp(2);
  tmp.push_back("fuck");
  tmp.push_back("fuck2");
  tmp.push_back("fuck3");
  ASSERT_EQ(tmp.capacity(), 2);
  ASSERT_EQ(tmp.size(), 2);
  ASSERT_FALSE(tmp.empty());
  ASSERT_EQ(tmp[0], "fuck2");
  ASSERT_EQ(tmp[1], "fuck3");

  ins::dynamic_circular_buffer<std::string> circular_buffer(tmp);
  ASSERT_EQ(circular_buffer.capacity(), 2);
  ASSERT_EQ(circular_buffer.size(), 2);
  ASSERT_FALSE(circular_buffer.empty());
  ASSERT_EQ(circular_buffer[0], "fuck2");
  ASSERT_EQ(circular_buffer[1], "fuck3");

  ASSERT_EQ(tmp.capacity(), 2);
  ASSERT_EQ(tmp.size(), 2);
  ASSERT_FALSE(tmp.empty());
  ASSERT_EQ(tmp[0], "fuck2");
  ASSERT_EQ(tmp[1], "fuck3");
}

//struct MoveTestData {
//  MoveTestData() = default;
//  MoveTestData(int val) : val_(val) {}
//  MoveTestData(const MoveTestData&) = default;
//  MoveTestData(MoveTestData &&oth) {
//    val_ = oth.val_;
//    LOG(INFO) << "use move.";
//  }
//  int val_;
//};
TEST(dynamic_circular_buffer, move_construct) {
  ins::dynamic_circular_buffer<std::string> tmp(2);
  tmp.push_back("fuck");
  tmp.push_back("fuck2");
  tmp.push_back("fuck3");
  ASSERT_EQ(tmp.capacity(), 2);
  ASSERT_EQ(tmp.size(), 2);
  ASSERT_FALSE(tmp.empty());
  ASSERT_EQ(tmp[0], "fuck2");
  ASSERT_EQ(tmp[1], "fuck3");

  ins::dynamic_circular_buffer<std::string> circular_buffer(std::move(tmp));
  ASSERT_EQ(circular_buffer.capacity(), 2);
  ASSERT_EQ(circular_buffer.size(), 2);
  ASSERT_FALSE(circular_buffer.empty());
  ASSERT_EQ(circular_buffer[0], "fuck2");
  ASSERT_EQ(circular_buffer[1], "fuck3");

  ASSERT_EQ(tmp.size(), 0);
}

TEST(dynamic_circular_buffer, itr) {
  ins::dynamic_circular_buffer<int> buffer(10);
  buffer.push_back(1);
  buffer.push_back(2);
  buffer.push_back(3);
  ASSERT_EQ(*buffer.begin(), 1);
  ASSERT_EQ(*(buffer.end()-1), 3);

  for (auto &val : buffer) {
    std::cout << val << " ";
  }
  std::cout << std::endl;
}

TEST(dynamic_circular_buffer, op_assign) {
  ins::dynamic_circular_buffer<int> cb(4);
  cb.push_back(1);
  cb.push_back(2);
  cb.push_back(3);
  
  //test operator=(const&)
  ins::dynamic_circular_buffer<int> cb1;
  cb1 = cb;
  ASSERT_EQ(cb1.size(), 3);
  ASSERT_EQ(cb1[0], 1);
  ASSERT_EQ(cb1[1], 2);
  ASSERT_EQ(cb1[2], 3);

  //test opeartor=(&&)
  ins::dynamic_circular_buffer<int> cb2;
  cb2 = std::move(cb1);
  ASSERT_EQ(cb2.size(), 3);
  ASSERT_EQ(cb2[0], 1);
  ASSERT_EQ(cb2[1], 2);
  ASSERT_EQ(cb2[2], 3);
}

TEST(dynamic_circular_buffer, swap) {
  ins::dynamic_circular_buffer<int> cb(4);
  cb.push_back(1);
  cb.push_back(2);
  cb.push_back(3);

  ins::dynamic_circular_buffer<int> cb1(2);
  cb1.push_back(10);

  cb.swap(cb1);
  ASSERT_EQ(cb.size(), 1);
  ASSERT_EQ(cb.capacity(), 2);
  ASSERT_EQ(cb[0], 10);

  ASSERT_EQ(cb1.size(), 3);
  ASSERT_EQ(cb1.capacity(), 4);
  ASSERT_EQ(cb1[0], 1);
  ASSERT_EQ(cb1[1], 2);
  ASSERT_EQ(cb1[2], 3);

  using std::swap;
  swap(cb, cb1);
  ASSERT_EQ(cb1.size(), 1);
  ASSERT_EQ(cb1.capacity(), 2);
  ASSERT_EQ(cb1[0], 10);

  ASSERT_EQ(cb.size(), 3);
  ASSERT_EQ(cb.capacity(), 4);
  ASSERT_EQ(cb[0], 1);
  ASSERT_EQ(cb[1], 2);
  ASSERT_EQ(cb[2], 3);
}
