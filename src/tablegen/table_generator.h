#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <atomic>
#include <functional>
#include <filesystem>

#include "board.h"
#include "cache.h"

#include "ankerl/unordered_dense.h"

using namespace std::literals::string_literals;

struct Node {
  uint64_t tiles;
  float prob;

  Node(uint64_t tiles, float prob) : tiles(tiles), prob(prob) {}
};

struct MoveProbs {
  // up right down left
  std::array<float, 4> probs;
  uint8_t best_move;

  void find_best_move () {
    best_move = std::distance(probs.begin(), std::max_element(probs.begin(), probs.end()));
  }
};

class TableGenerator {
private:
  std::string table_dir;
  bool positions_generated;

  int num_threads;
  std::vector<std::thread> threads;
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic_flag flag_done = ATOMIC_FLAG_INIT;
  int completed_threads = 0;

  Board& board_lut;

  uint64_t root;

  uint64_t static_tiles;
  uint64_t static_tiles_mask = 0;
  uint64_t moving_tiles_map;
  int num_moving_tiles;

  uint8_t goal_tile;

  std::vector<std::shared_ptr<std::vector<uint64_t>>> current_sum_positions;
  int original_sum;
  int tile_sum;

  std::vector<std::shared_ptr<std::vector<uint64_t>>> sum_plus_two_positions;
  std::vector<std::shared_ptr<std::vector<uint64_t>>> sum_plus_four_positions;

  std::unique_ptr<Cache> cache;

  std::vector<std::shared_ptr<ankerl::unordered_dense::map<uint64_t, MoveProbs>>> current_sum_probs;
  std::vector<std::shared_ptr<ankerl::unordered_dense::map<uint64_t, MoveProbs>>> sum_plus_two_probs;
  std::vector<std::shared_ptr<ankerl::unordered_dense::map<uint64_t, MoveProbs>>> sum_plus_four_probs;

  bool positions_empty () {
    return std::all_of(
      current_sum_positions.begin(),
      current_sum_positions.end(),
      [](auto vec) {
        return vec->empty();
      }
    );
  }

  uint64_t pack_probs (const std::array<float, 4>& probs) {
    // 14 digit binary decimals, 7 bytes total
    uint64_t res = 0;

    for (int i = 0; i < 4; i++) {
      float prob = probs[i];
      if (tile_sum == 32768)
      /**
       * simple trick that makes 1 not end up as 0.99999, this shifts 
       * everything left by one digit but you lose one piece of data where
       * the value 1/2^14 is just 0
       */
      prob -= 1.0 / (1 << 14);
      for (int exp = 1; exp <= 14; exp++) {
        if (prob >= 1.0 / (1 << exp)) {
          res |= ((UINT64_C(1) << (14 - exp)) << (14 * i));
          prob -= 1.0 / (1 << exp);
        }
      }
    }

    return res;
  }

  std::array<float, 4> unpack_probs (uint64_t packed) {
    std::array<float, 4> probs;
    
    for (int i = 0; i < 4; i++) {
      uint64_t part = (packed >> (14 * i)) & 0x3FFF; // 0b11 1111 1111 1111
      probs[i] = 0.0;

      for (int exp = 1; exp <= 14; exp++) {
        if (((part >> (14 - exp)) & 1) == 1) {
          probs[i] += 1.0 / (1 << exp);
        }
      }

      if (probs[i] != 0) {
        probs[i] += 1.0 / (1 << 14);
      }
    }

    return probs;
  }


  int bad_hash (uint64_t x, int modulus) {
    x ^= (x << 21);
    x ^= (x >> 35);
    x ^= (x << 4);
    return x % modulus;
  }

  void generate_all_positions (int thread_id);
  void evaluate_all_positions (int thread_id);

  void get_positions (int thread_id);
  void test_direction (int thread_id, uint64_t board, Direction dir);

  void evaluate_positions (int thread_id);
  float evaluate_direction (uint64_t board, Direction dir, int thread_id);
  MoveProbs lookup_probs (
    std::vector<std::shared_ptr<ankerl::unordered_dense::map<uint64_t, MoveProbs>>> probs,
    uint64_t board
  );
  void write_table ();
public:
  TableGenerator (Board& board_lut, const std::string& name, uint64_t start_tiles, uint64_t static_tiles, uint8_t goal_tile, std::size_t cache_size, int num_threads): board_lut(board_lut), table_dir(name), root(start_tiles), static_tiles(static_tiles), goal_tile(goal_tile), num_threads(num_threads) {
    if (!std::filesystem::exists("positions")) {
      std::filesystem::create_directory("positions");
    }
    if (!std::filesystem::exists(table_dir)) {
      std::filesystem::create_directory(table_dir);
    }

    static_tiles_mask = board_lut.make_static_tiles_mask(static_tiles);
    moving_tiles_map = board_lut.make_moving_tiles_map(static_tiles);
    num_moving_tiles = __builtin_popcount(board_lut.get_empty_squares(static_tiles));

    cache = std::make_unique<Cache>(cache_size);

    original_sum = board_lut.sum_of_tiles(root);
    tile_sum = original_sum;
  }

  struct table_generator_error: public std::runtime_error {
    table_generator_error(const std::string& text): std::runtime_error("Table Generator Error: "s + text) {}
  };

  struct table_lookup_error: public std::runtime_error {
    table_lookup_error(const std::string& text): std::runtime_error("Table Lookup Error: "s + text) {}
  };

  void thread_loop (int thread_id);
  void generate_table (bool positions_generated);

  MoveProbs read_table (uint64_t board);
};
