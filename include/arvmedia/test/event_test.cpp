
//
// Created by jerett on 2017/6/16.
//

#include <gtest/gtest.h>
#include <eventloop/event.h>
#include <memory>

TEST(event, GetData) {
  using namespace ins;
  auto event = std::make_shared<Event>(2);
  event->SetData("fuck", 1);
  int *data = nullptr;
  data = event->GetDataNoCopy<int>("fuck");
  ASSERT_EQ(*data, 1);

  *data = 2;
  int val;
  event->GetDataCopy("fuck", val);
  ASSERT_EQ(val, 2);
//  A a(10);
//  any int_any(a);
//  any any2;
//  printf("...................\n");
//  any2 = std::move(int_any);
//  EXPECT_EQ(any_cast<A&>(any2), A(10));
//  EXPECT_EQ(true, int_any.empty());
//  printf("...................\n");
//  any2 = A(20);
//  EXPECT_EQ(any_cast<A&>(any2), A(20));
}
