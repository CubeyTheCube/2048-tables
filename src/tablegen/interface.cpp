#include <iostream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <fstream>

#include "interface.h"

using namespace std::literals::string_literals;

std::string Interface::board_to_hash (uint64_t board, Board& board_lut) {
  std::stringstream hash;

  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      hash << std::hex << static_cast<int>(board_lut.get_tile(board, x, y));
    }
  }

  return hash.str();
}

uint64_t Interface::hash_to_board (const std::string& hash, Board& board_lut) {
  if (hash.size() != 16) {
    std::cerr << "Invalid practice hash" << std::endl;
    exit(1);
  }
  
  uint64_t tiles = 0;
  int i = 0;

  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      std::stringstream ss;
      int tile;

      ss << std::hex << hash[i];
      ss >> tile;
      tiles = board_lut.set_tile(tiles, x, y, tile);
      i++;
    }
  }

  return tiles;
}

void Interface::run_interface () {
  std::cout
    << "Welcome to Cubey's 2048 table tool"
    << std::endl;

  while (true) {
    std::cout
      << "Do you want to create a new table (1), read an existing one (2), trainer mode (3), or quit (4)"
      << std::endl;

    int answer;
    std::cin >> answer;

    switch (answer) {
      case 1:
        create_table();
        break;
      case 2:
        read_table();
        break;
      case 3:
        trainer_mode();
        break;
      case 4:
        return;
      default:
        std::cerr
          << "Invalid answer"
          << std::endl;
        return;
    }
  }
}

void Interface::read_table () {
  if (!table_generator) {
    std::cout
      << "What's the name of the table?"
      << std::endl;
    std::string name;
    std::cin >> name;
    name = "table_"s + name;

    std::ifstream meta_file(name + "/meta.txt"s);
    if (!meta_file.good()) {
      std::cerr
        << "Could not find table "s + name
        << std::endl;
      return;
    }

    uint64_t starting_board, static_tiles;
    int goal_tile;

    meta_file >> starting_board;
    meta_file >> static_tiles;
    meta_file >> goal_tile;
    table_generator = std::make_unique<TableGenerator>(board_lut, name, starting_board, static_tiles, goal_tile, 0, 0);
  }
  
  std::string hash;
  std::cout
    << "Enter the practice hash (16 characters) of your board: "
    << std::endl;
  std::cin >> hash;

  uint64_t board = hash_to_board(hash, board_lut);

  MoveProbs p;
  try {
    p = table_generator->read_table(board);
    std::cout << "The probability of this position is" << std::endl
      << "U: " << p.probs[0]*100 << "%" << std::endl
      << "R: " << p.probs[1]*100 << "%" << std::endl
      << "D: " << p.probs[2]*100 << "%" << std::endl
      << "L: " << p.probs[3]*100 << "%" << std::endl
      << std::endl;
  } catch (const std::runtime_error& ex) {
    std::cerr << ex.what() << std::endl;
  }
}

void Interface::trainer_mode () {
  std::cerr << "Coming soon" << std::endl;
  exit(1);

  uint64_t starting_board, static_tiles;
  if (!table_generator) {
    std::cout
      << "What's the name of the table?"
      << std::endl;
    std::string name;
    std::cin >> name;
    name = "table_"s + name;

    std::ifstream meta_file(name + "/meta.txt"s);
    if (!meta_file.good()) {
      std::cerr
        << "Could not find table "s + name
        << std::endl;
      return;
    }

    int goal_tile;

    meta_file >> starting_board;
    meta_file >> static_tiles;
    meta_file >> goal_tile;
    table_generator = std::make_unique<TableGenerator>(board_lut, name, starting_board, static_tiles, goal_tile, 0, 0);
  }

  uint64_t board = starting_board;
  float accuracy = 1.0;

  while (true) {
    std::cout
      << "Current board: "
      << std::endl;
    board_lut.print(std::cout, board);
    std::cout
      << "Enter move (U/D/L/R):"
      << std::endl;
    char move;
    std::cin >> move;

    Direction dir;

    MoveProbs p;
    try {
      p = table_generator->read_table(board);
      p.find_best_move();
      std::cout << "The probability of this position is" << std::endl
        << "U: " << p.probs[0]*100 << "%" << std::endl
        << "R: " << p.probs[1]*100 << "%" << std::endl
        << "D: " << p.probs[2]*100 << "%" << std::endl
        << "L: " << p.probs[3]*100 << "%" << std::endl
        << std::endl;
    } catch (const std::runtime_error& ex) {
      std::cerr << ex.what() << std::endl;
    }
  }
}

void Interface::create_table () {
  std::cout
    << "What's the name of the table?"
    << std::endl;
  std::string name;
  std::cin >> name;
  name = "table_"s + name;

  bool positions_generated = false;

  std::cout
    << "Do you already have positions generated to evaluate (Y), or do you want to do both (N)?"
    << std::endl;

  char answer;
  std::cin >> answer;
  if (answer == 'Y' || answer == 'y') {
    positions_generated = true;
  }

  uint64_t starting_board;
  while (true) {
    std::string hash;
    std::cout
      << "Enter the practice hash (16 characters) of your starting board: "
      << std::endl;
    std::cin >> hash;

    starting_board = hash_to_board(hash, board_lut);

    std::cout
      << "This is your board:"
      << std::endl;
    board_lut.print(std::cout, starting_board);
    std::cout
      << "Is it OK? (Y/N)"
      << std::endl;

    std::cin >> answer;
    if (answer == 'Y' || answer == 'y') {
      break;
    }
    
    std::cout
      << "Womp womp, try again"
      << std::endl;
  }

  uint64_t static_tiles;
  while (true) {
    std::string hash;
    std::cout
      << "Enter the practice hash (16 characters) of all the tiles on the board that you don't want to move: "
      << std::endl;
    std::cin >> hash;

    static_tiles = hash_to_board(hash, board_lut);

    std::cout
      << "These tiles wil stay still:"
      << std::endl;
    board_lut.print(std::cout, static_tiles);
    std::cout
      << "Is it OK? (Y/N)"
      << std::endl;

    std::cin >> answer;
    if (answer == 'Y' || answer == 'y') {
      break;
    }

    std::cout
      << "Womp womp, try again"
      << std::endl;
  }

  int goal_tile;
  std::cout
    << "What's the goal tile?" << std::endl
    << "Getting 2 of this tile is considered a win" << std::endl
    << "Enter goal tile:" << std::endl;
  std::cin >> goal_tile;

  int num_threads;
  std::cout
    << "How many threads do you want to use?"
    << std::endl;

  std::cin >> num_threads;
  if (num_threads < 1) {
    std::cerr << "Invalid # threads" << std::endl;
    exit(0);
  }

  uint64_t cache_size;
  std::cout
    << "What do you want the cache size to be?" << std::endl
    << "Ideally this should be a prime number, and a large one." << std::endl
    << "If you're doing 10 space, a good size is 104,395,301" << std::endl
    << "Enter size:" << std::endl;
  std::cin >> cache_size;

  std::cout << "Starting..." << std::endl;

  std::ofstream meta_file(name + "/meta.txt"s);
  meta_file
    << starting_board << std::endl
    << static_tiles << std::endl
    << std::log2(goal_tile) << std::endl;
  
  table_generator = std::make_unique<TableGenerator>(board_lut, name, starting_board, static_tiles, std::log2(goal_tile), cache_size, num_threads);

  auto start_time = std::chrono::high_resolution_clock::now();
  try {
    table_generator->generate_table(positions_generated);
  } catch (const std::runtime_error& ex) {
    std::cerr << ex.what() << std::endl;
    return;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  std::cout << "Completed in " << (duration / 1e6) << " seconds." << std::endl;

  MoveProbs p;
  try {
    p = table_generator->read_table(starting_board);
    std::cout << "The probability of this position is" << std::endl
      << "U: " << p.probs[0]*100 << "%" << std::endl
      << "R: " << p.probs[1]*100 << "%" << std::endl
      << "D: " << p.probs[2]*100 << "%" << std::endl
      << "L: " << p.probs[3]*100 << "%" << std::endl
      << std::endl;
  } catch (const std::runtime_error& ex) {
    std::cerr << ex.what() << std::endl;
  }
}
