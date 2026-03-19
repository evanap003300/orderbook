#pragma once
#include <stdint.h>
#include <stdio.h>
#include <fstream>
#include <arpa/inet.h>

class ItchParser {
public:
    void readItch(uint32_t order);
    void generateItch();
};