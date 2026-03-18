#pragma once
#include <stdint.h>
#include <stdio.h>
#include <fstream>

struct Itch {
  int a;
};

class ItchParser {
public:
    void readItch(uint32_t order);
    void generateItch();
};