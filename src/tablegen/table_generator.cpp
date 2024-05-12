#include <fstream>
#include <cstdio>

#include "table_generator.h"
#include "interface.h"

void TableGenerator::thread_loop (int thread_id) {
  if (!positions_generated) {
    generate_all_positions(thread_id);
  }
  evaluate_all_positions(thread_id);
}

void TableGenerator::generate_all_positions (int thread_id) {
  int saved_sum;

  while (!positions_empty()) {
    get_positions(thread_id);

    std::unique_lock<std::mutex> lock(mutex);
    completed_threads++;

    saved_sum = tile_sum;
    if (completed_threads == num_threads) {
      completed_threads = 0;
      cache->clear();

      std::vector<std::unique_ptr<std::ofstream>> files;
      for (int i = 0; i < num_threads; i++) {
        files.emplace_back(
          std::make_unique<std::ofstream>(
            "positions/"s + std::to_string(tile_sum) + "_"s + std::to_string(i) + ".txt"s, std::ios::binary
          )
        );
      }

      for (int i = 0; i < num_threads; i++) {
        for (const auto board : *current_sum_positions[i]) {
          int hash = bad_hash(board, num_threads);
          files[hash]->write(reinterpret_cast<const char *>(&board), sizeof board);
        }
      }

      tile_sum += 2;

      sum_plus_two_positions.swap(current_sum_positions);
      sum_plus_two_positions.swap(sum_plus_four_positions);
      for (auto &vec : sum_plus_four_positions) {
        vec->resize(0); // why clear() it when we can save a reallocation?
      }

      cv.notify_all();
    } else {
      cv.wait(lock, [this, saved_sum] { 
        return saved_sum != tile_sum || positions_empty();
      }); // prevents "spurious wakeups"
    }
  }

  std::unique_lock<std::mutex> lock(mutex);

  if (!flag_done.test_and_set()) {
    // cleanup, atomic flag means only one thread can be here

    cache->destroy();
    tile_sum -= 2;
    completed_threads = 0;

    // clean up
    for (int i = 0; i < num_threads; i++) {
      current_sum_positions[i] = std::make_shared<std::vector<uint64_t>>();
      sum_plus_two_positions[i] = current_sum_positions[i];
      sum_plus_four_positions[i] = current_sum_positions[i];
    }
  }
  lock.unlock();
}

void TableGenerator::evaluate_all_positions (int thread_id) {
  int saved_sum;

  while (tile_sum >= original_sum) {
    evaluate_positions(thread_id);
    std::remove(("positions/"s + std::to_string(tile_sum) + "_"s + std::to_string(thread_id) + ".txt"s).c_str());

    std::unique_lock<std::mutex> lock(mutex);
    completed_threads++;

    saved_sum = tile_sum;

    if (completed_threads == num_threads) {
      completed_threads = 0;
      write_table();

      tile_sum -= 2;
      for (int i = 0; i < num_threads; i++) {
        std::swap(*sum_plus_two_probs[i], *sum_plus_four_probs[i]);
        std::swap(*current_sum_probs[i], *sum_plus_two_probs[i]);
        current_sum_probs[i] = std::make_shared<ankerl::unordered_dense::map<uint64_t, MoveProbs>>();
      }

      cv.notify_all();
    } else {
      cv.wait(lock, [this, saved_sum] { return saved_sum != tile_sum; });
    }
  }
}

void TableGenerator::generate_table (bool positions_done) {
  positions_generated = positions_done;

  for (int i = 0; i < num_threads; i++) {
    current_sum_positions.emplace_back(std::make_shared<std::vector<uint64_t>>());
    sum_plus_two_positions.emplace_back(std::make_shared<std::vector<uint64_t>>());
    sum_plus_four_positions.emplace_back(std::make_shared<std::vector<uint64_t>>());

    current_sum_probs.emplace_back(
      std::make_shared<ankerl::unordered_dense::map<uint64_t, MoveProbs>>()
    );
    sum_plus_two_probs.emplace_back(
      std::make_shared<ankerl::unordered_dense::map<uint64_t, MoveProbs>>()
    );
    sum_plus_four_probs.emplace_back(
      std::make_shared<ankerl::unordered_dense::map<uint64_t, MoveProbs>>()
    );
  }
  current_sum_positions[0]->emplace_back(root);

  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(std::thread(&TableGenerator::thread_loop, this, i));
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

void TableGenerator::get_positions (int thread_id) {
  std::size_t total_size = 0;
  for (const auto& vec : current_sum_positions) {
    total_size += vec->size();
  }

  std::size_t partition_size;
  std::size_t before_size;
  if (thread_id < (total_size % num_threads)) {
    partition_size = total_size / num_threads + 1;
    before_size = (total_size / num_threads + 1) * thread_id;
  } else {
    partition_size = total_size / num_threads;
    before_size = (total_size / num_threads + 1) * (total_size % num_threads) + (total_size / num_threads) * (thread_id - (total_size % num_threads));
  }

  std::shared_ptr<std::vector<uint64_t>> start_vec = nullptr;
  std::size_t start_vec_idx = 0;

  std::size_t sum = 0;
  std::vector<uint64_t>::iterator start_it;

  for (const auto& vec : current_sum_positions) {
    sum += vec->size();
    if (sum > before_size) {
      start_vec = vec;
      start_it = vec->begin() + (before_size - (sum - vec->size()));

      break;
    }
    start_vec_idx++;
  }

  sum = 0;
  std::shared_ptr<std::vector<uint64_t>> vec = start_vec;
  int vec_idx = start_vec_idx;

  while (true) {
    if (start_vec == nullptr) {
      break;
    }

    if (sum > 0) {
      start_it = vec->begin();
    }

    std::vector<uint64_t>::iterator end_it;

    if (sum + std::distance(start_it, vec->end()) > partition_size) {
      end_it = start_it + (partition_size - sum);
    } else {
      end_it = vec->end();
    }

    for (auto it = start_it; it != end_it; it++) {
      uint64_t board = *it;

      if (board_lut.game_over(board)) {
        continue;
      }

      if (board_lut.num_tiles(board, goal_tile) > 1) {
        continue;
      }

      test_direction(thread_id, board, Direction::up);
      test_direction(thread_id, board, Direction::right);
      test_direction(thread_id, board, Direction::down);
      test_direction(thread_id, board, Direction::left);
    }

    sum += std::distance(start_it, end_it);
    if (sum < partition_size) {
      vec = current_sum_positions[vec_idx + 1];
      vec_idx++;
    } else {
      break;
    }
  }
}

void TableGenerator::test_direction (int thread_id, uint64_t board, Direction dir) {
  uint64_t moved_board = board_lut.move(board, dir);
  if (moved_board == board) {
    return;
  }

  if ((moved_board & static_tiles_mask) != static_tiles) {
    return;
  }

  /**
   * this is probably really unnecessary premature optimization, but it
   * gets a 16 bit integer with the empty squares as 1s and the others
   * as 0s, then loops through all the 1 bits, counting trailing 0s to
   * find their index, it's probably better to just use a normal loop
   */
  uint16_t empty_squares = board_lut.get_empty_squares(moved_board);

  int i = 15;
  while (empty_squares) {
    int trailing = __builtin_ctz(empty_squares); // intrinsic, trailing zeroes

    empty_squares >>= trailing;
    i -= trailing;
    uint64_t new_board = board_lut.set_tile(moved_board, 3 - i % 4, i / 4, 1);
    if (!cache->test(new_board)) {
      sum_plus_two_positions[thread_id]->emplace_back(new_board);
    }
    new_board = board_lut.set_tile(moved_board, 3 - i % 4, i / 4, 2);
    if (!cache->test(new_board)) {
      sum_plus_four_positions[thread_id]->emplace_back(new_board);
    }

    empty_squares >>= 1;
    i--;
  }
}

void TableGenerator::evaluate_positions (int thread_id) {
  std::ifstream positions_file("positions/"s + std::to_string(tile_sum) + "_"s + std::to_string(thread_id) + ".txt"s, std::ios::binary);

  uint64_t board;
  while (positions_file.good()) {
    positions_file.read(reinterpret_cast<char *>(&board), sizeof board);

    MoveProbs move_probs;
    if (board_lut.game_over(board)) {
      move_probs.probs = {0, 0, 0, 0};
    } else if (board_lut.num_tiles(board, goal_tile) > 1) {
      move_probs.probs = {1, 1, 1, 1};
    } else {
      move_probs.probs[0] = evaluate_direction(board, Direction::up, thread_id);
      move_probs.probs[1] = evaluate_direction(board, Direction::right, thread_id);
      move_probs.probs[2] = evaluate_direction(board, Direction::down, thread_id);
      move_probs.probs[3] = evaluate_direction(board, Direction::left, thread_id);
    }

    move_probs.find_best_move();
    (*current_sum_probs[thread_id])[board] = move_probs;
  }
}

MoveProbs TableGenerator::lookup_probs (
  std::vector<std::shared_ptr<ankerl::unordered_dense::map<uint64_t, MoveProbs>>> probs,
  uint64_t board
) {
  return (*probs[bad_hash(board, num_threads)])[board];
}

float TableGenerator::evaluate_direction (uint64_t board, Direction dir, int thread_id) {
  uint64_t moved_board = board_lut.move(board, dir);
  if (moved_board == board) {
    return 0;
  }

  if ((moved_board & static_tiles_mask) != static_tiles) {
    return 0;
  }

  uint16_t empty_squares = board_lut.get_empty_squares(moved_board);
  int num_empty = __builtin_popcount(empty_squares);

  float prob = 0;

  int i = 15;
  while (empty_squares) {
    int trailing = __builtin_ctz(empty_squares); // intrinsic, trailing zeroes

    empty_squares >>= trailing;
    i -= trailing;
    uint64_t new_board = board_lut.set_tile(moved_board, 3 - i % 4, i / 4, 1);

    MoveProbs lookup = lookup_probs(sum_plus_two_probs, new_board);
    prob += lookup.probs[lookup.best_move] * 0.9 / num_empty;

    new_board = board_lut.set_tile(moved_board, 3 - i % 4, i / 4, 2);
    lookup = lookup_probs(sum_plus_four_probs, new_board);
    prob += lookup.probs[lookup.best_move] * 0.1 / num_empty;

    empty_squares >>= 1;
    i--;
  }

  return prob;
}

void TableGenerator::write_table () {
  std::ofstream table_file(table_dir + "/" + std::to_string(tile_sum) + ".txt", std::ios::binary);

  for (int i = 0; i < num_threads; i++) {
    for (const auto it : *current_sum_probs[i]) {
      uint64_t packed_board = board_lut.pack_tiles(it.first, moving_tiles_map);
      table_file.write(reinterpret_cast<char *>(&packed_board), (num_moving_tiles / 2) + (num_moving_tiles % 2 != 0));
      uint64_t packed_probs = pack_probs(it.second.probs);
      table_file.write(reinterpret_cast<char *>(&packed_probs), 7);
    }
  }
}

MoveProbs TableGenerator::read_table (uint64_t board) {
  int sum = board_lut.sum_of_tiles(board);
  std::ifstream table_file(table_dir + "/" + std::to_string(sum) + ".txt", std::ios::binary);
  if (!table_file.good()) {
    throw table_lookup_error("Table file doesn't exist for sum "s + std::to_string(sum));
  }

  while (table_file.good()) {
    uint64_t packed_board = 0;
    table_file.read(reinterpret_cast<char *>(&packed_board), (num_moving_tiles / 2) + (num_moving_tiles % 2 != 0));
    if (packed_board != board_lut.pack_tiles(board, moving_tiles_map)) {
      table_file.ignore(7);
      continue;
    }

    uint64_t packed_probs = 0;
    table_file.read(reinterpret_cast<char *>(&packed_probs), 7);

    MoveProbs move_probs;
    move_probs.probs = unpack_probs(packed_probs);
    move_probs.find_best_move();
    return move_probs;
  }

  throw table_lookup_error("Could not find probabilities for board "s + Interface::board_to_hash(board, board_lut));
}
