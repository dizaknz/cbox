#include <gtest/gtest.h>
#include "queue.hpp"
    
TEST(QueueTest, Push)
{
    auto n = 50;
    TQueue<int> tq;
    for (int i = 1; i <= n; i++)
    {
        tq.push(i);
    }
    for (int i = 1; i <= n; i++)
    {
        auto v = tq.pop();
        EXPECT_EQ(i, v);
    }
}

