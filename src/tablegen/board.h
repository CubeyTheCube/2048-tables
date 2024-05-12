#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <utility>


enum class Direction {
  up,
  right,
  down,
  left
};

class Board {
private:
  static const uint64_t MASK = 0b1111;

  // this can't be here, need to fix
  std::array<std::pair<uint16_t, uint16_t>, 65536> _move_lut;
  std::array<uint16_t, 65536> _empty_lut;

  void load_lut (const std::string& file);
public:
  Board () {
    load_lut("src/lut/lut.txt");
  }

  // value is 1-16
  static uint8_t get_tile (uint64_t tiles, int x, int y) {
    // 4 bits per tile, because 65k will never happen
    int pos = y * 4 + x;
    int shift = 60 - pos * 4;

    return (tiles >> shift) & MASK;
  }

  static uint8_t get_tile (uint64_t tiles, int pos) {
    int shift = 60 - pos * 4;

    return (tiles >> shift) & MASK;
  }

  static uint64_t set_tile (uint64_t tiles, int x, int y, uint8_t value) {
    int pos = y * 4 + x;
    int shift = 60 - pos * 4;
    
    tiles &= (~(MASK << shift));
    tiles |= (static_cast<uint64_t>(value) << shift);

    return tiles;
  }

  static uint64_t load_board (const std::array<std::array<int, 4>, 4>& board);
  static void print (std::ostream& out, uint64_t tiles);

  uint64_t move(uint64_t tiles, Direction dir);
  bool game_over (uint64_t tiles);
  uint16_t get_empty_squares (uint64_t tiles);

  int num_tiles (uint64_t tiles, uint8_t tile);
  int sum_of_tiles (uint64_t tiles);

  uint64_t make_static_tiles_mask (uint64_t static_tiles);
  uint64_t make_moving_tiles_map (uint64_t static_tiles);
  uint64_t pack_tiles (uint64_t tiles, uint64_t moving_tiles_map);
};