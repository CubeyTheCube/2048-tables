#pragma once

#include <iostream>
#include <string>
#include <memory>

#include "table_generator.h"

class Interface {
private:
  Board board_lut;
  std::unique_ptr<TableGenerator> table_generator;
public:
  Interface () {}

  static std::string board_to_hash (uint64_t board, Board& board_lut);
  static uint64_t hash_to_board (const std::string& hash, Board& board_lut);

  void read_table ();
  void create_table ();
  void trainer_mode ();
  void run_interface ();
};