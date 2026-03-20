#pragma once
#include "itch.hpp"

class OrderMatching {
public:
    std::vector<Order> processOrders();
    void matchOrders(const Order& order);
};