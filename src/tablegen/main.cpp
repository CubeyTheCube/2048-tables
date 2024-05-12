#include <iostream>
#include <array>
#include <chrono>
#include "board.h"
#include "table_generator.h"
#include "interface.h"

int main() {
  Interface interface;
  interface.run_interface();

  return 0;
}