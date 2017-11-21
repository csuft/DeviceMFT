//
// Created by jerett on 16/5/23.
//
#include <gtest/gtest.h>
#include <av_toolbox/any.h>
#include <iostream>

class A {
public:
  friend bool operator==(const A &lhs, const A &rhs);

  explicit A(int val) : val_(val) {
  }

  A(const A &a) {
    val_ = a.val_;
    std::cout << "A copy ..." << std::endl;
  }

  A(A&& a) {
    val_ = a.val_;
    std::cout << "A move ..." << std::endl;
  }

public:
  int val_;
};

bool operator==(const A &lhs, const A &rhs) {
  return lhs.val_ == rhs.val_;
}

TEST(any, cast_int) {
  using namespace ins;
  any any(3);
  int value = any_cast<int>(any);
  EXPECT_EQ(value, 3);
}

TEST(any, copyConstruct) {
  using namespace ins;
  A a(10);
  any any1(a);
  A value = any_cast<A>(any1);
  EXPECT_EQ(value, a);

  printf("..............\n");
  any any2(any1);
  EXPECT_EQ(any_cast<A&>(any2).val_, 10);
}
 
TEST(any, rvalue_reference) {
  using namespace ins;
  //const any a1(1);
  //std::printf("fuck...\n");
  any a1(1);
  any a2(std::move(a1));
  EXPECT_EQ(any_cast<int>(a2), 1);
}

TEST(any, const_rvalue_reference) {
  using namespace ins;
  const any a1(1);
  std::printf("fuck...\n");
  //any a1(1);
  any a2(std::move(a1));
  //EXPECT_EQ(any_cast<int>(a2), 1);
}

TEST(any, cast_moveConstruct) {
  using namespace ins;
  any any(A(10));
  A &value = any_cast<A&>(any);
  EXPECT_EQ(value.val_, 10);
}

TEST(any, empty) {
  using namespace ins;
  any empty_any;
  EXPECT_EQ(true, empty_any.empty());
  EXPECT_EQ(typeid(void), empty_any.type());

  any any1(1);
  EXPECT_EQ(false, any1.empty());
}

TEST(any, badcast) {
  using namespace ins;
  any int_any(1);
  EXPECT_THROW(any_cast<double>(int_any), std::bad_cast);
}

TEST(any, assign) {
  using namespace ins;
  A a(10);
  any int_any(a);
  any any2;
  printf("...................\n");
  any2 = std::move(int_any);
  EXPECT_EQ(any_cast<A&>(any2), A(10));
  EXPECT_EQ(true, int_any.empty());
  printf("...................\n");
  any2 = A(20);
  EXPECT_EQ(any_cast<A&>(any2), A(20));
}
