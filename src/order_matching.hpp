#pragma once
#include "itch.hpp"

struct ItchOrderExecuted {
    char message_type;       
    uint16_t stock_locate;   
    uint16_t tracking_number; 
    uint16_t timestamp_high;  
    uint32_t timestamp_low;
    uint64_t order_reference_number; 
    uint32_t executed_shares;   
    uint64_t match_number;      
};

class OrderMatching {
public:
    void matchOrders(const std::vector<Order>& orders);

private:
    std::vector<ItchOrderExecuted> matchTicker(const std::vector<Order>& orders);
};