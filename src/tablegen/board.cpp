#include <cmath>
#include <iostream>
#include <fstream>
#include "board.h"

void Board::load_lut (const std::string& file) {
  std::ifstream lut_file(file);
  if (!lut_file.is_open()) {
    std::cerr << "Could not open LUT file: " << file << std::endl;
    exit(1);
  }
  int right;
  int left;
  int empty;

  int i = 0;
  while (lut_file >> right >> left >> empty) {
    _move_lut[i] = {right, left};
    _empty_lut[i] = empty;
    i++;
  }
}

uint64_t Board::load_board (const std::array<std::array<int, 4>, 4>& board) {
  uint64_t tiles = 0;
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      tiles = set_tile(tiles, x, y, board[y][x] == 0 ? 0 : std::log2(board[y][x]));
    }
  }
  return tiles;
}

void Board::print (std::ostream& out, uint64_t tiles) {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      int tile = get_tile(tiles, x, y);
      out << (tile == 0 ? 0 : (1 << tile)) << " ";
    }
    out << "\n";
  }
}


uint64_t Board::move (uint64_t tiles, Direction dir) {
  if (dir == Direction::left || dir == Direction::right) {
    const uint64_t mask16 = 0xFFFF;
    for (int i = 0; i < 4; i++) {
      uint16_t row = (tiles & (mask16 << (16 * i))) >> (16 * i);
      tiles &= (~(mask16 << (16 * i)));

      uint64_t new_row = dir == Direction::right ? _move_lut[row].first : _move_lut[row].second;
      tiles |= new_row << (16 * i);
    }
  } else {
    uint64_t mask_alternating = 0xF000F000F000F000;
    for (int i = 0; i < 4; i++) {
      uint64_t mask = mask_alternating >> (4 * i);
      uint64_t masked = (tiles & mask);
      // grabbing each of these 2 bytes one by one and combining them
      uint16_t row = ((masked >> 4 * (3 - i)) & 0xF) + ((masked >> 4 * (6 - i)) & 0xF0) + ((masked >> 4 * (9 - i)) & 0xF00) + ((masked >> 4 * (12 - i)) & 0xF000);
      tiles &= ~mask;

      uint64_t new_row = dir == Direction::down ? _move_lut[row].first : _move_lut[row].second;

      // reverse the process
      uint64_t placed_row = ((new_row & 0xF) << 4 * (3 - i)) + ((new_row & 0xF0) << 4 * (6 - i)) + ((new_row & 0xF00) << 4 * (9 - i)) + ((new_row & 0xF000) << 4 * (12 - i));
      tiles |= placed_row;
    }
  }

  return tiles;
}

bool Board::game_over (uint64_t tiles) {
  if (get_empty_squares(tiles)) {
    return false;
  }
  return (tiles == move(tiles, Direction::right) && tiles == move(tiles, Direction::up));
}

uint16_t Board::get_empty_squares (uint64_t tiles) {
  const uint64_t mask16 = 0xFFFF;

  uint16_t empty_squares = 0;
  for (int i = 0; i < 4; i++) {
    uint16_t row = (tiles & (mask16 << (16 * i))) >> (16 * i);
    empty_squares += _empty_lut[row] << (4 * i);
  }

  return empty_squares;
}

int Board::num_tiles (uint64_t tiles, uint8_t tile) { // can also be done by xor with tile repeated 16 times, then get_empty_squares != 0, but idk if faster
  int num = 0;
  for (int i = 0; i < 16; i++) {
    if (((tiles >> (i * 4)) & 0xF) == tile) {
      num++;
    }
  }

  return num;
}

int Board::sum_of_tiles (uint64_t tiles) { // this is not time-sensitive
  int sum = 0;
  for (int i = 0; i < 16; i++) {
    int tile = ((tiles >> (i * 4)) & 0xF);
    sum += tile == 0 ? 0 : 1 << tile;
  }

  return sum;
}

uint64_t Board::make_static_tiles_mask (uint64_t static_tiles) {
  uint64_t static_tiles_mask = 0;

  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      if (get_tile(static_tiles, x, y)) {
        static_tiles_mask = set_tile(static_tiles_mask, x, y, 15); 
      }
    }
  }

  return static_tiles_mask;
}

uint64_t Board::make_moving_tiles_map (uint64_t static_tiles) {
  uint64_t moving_tiles_map = 0;

  int i = 0;
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      if (!get_tile(static_tiles, x, y)) {
        moving_tiles_map |= static_cast<uint64_t>(y * 4 + x) << (i * 4);
        i++;
      }
    }
  }

  return moving_tiles_map;
}

uint64_t Board::pack_tiles (uint64_t tiles, uint64_t moving_tiles_map) {
  /**
   * moving_tiles_map has all the locations of the tiles that are moving
   * for example if positions 1, 2, and 3 are moving then moving_tiles_map
   * will be 0x0000000000000123
   */

  uint64_t res = 0;

  int i = 0;
  while (moving_tiles_map) {
    int pos = moving_tiles_map & 0xF;
    res |= (static_cast<uint64_t>(get_tile(tiles, pos)) << (i * 4));

    moving_tiles_map >>= 4;
    i++;
  }

  return res;
}
