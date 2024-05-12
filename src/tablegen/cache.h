#pragma once

#include <vector>
#include <queue>
#include <algorithm>
#include <cstdint>

class Cache {
private:

std::vector<uint64_t> data;

public:
int hits = 0;
int misses = 0;
  Cache (std::size_t size) {
    data.resize(size);
    clear();
  }

  // why would you want to copy this
  Cache (const Cache& other) = delete;
  Cache& operator=(const Cache& other) = delete;

  bool test (uint64_t value) {
    std::size_t index = value % data.size();

    bool res = data[index] == value;
    if (res) {
      hits++;
    } else {
      misses++;
    }
    data[index] = value;
    return res;
  }

  void clear () {
    std::cout << "Clearing cache: Hits are " << hits << " and misses are " << misses << std::endl;
    hits=0;
    misses=0;
    std::fill(data.begin(), data.end(), 0);
  }

  // this is NOT a destructor, this is just to reduce memory usage
  void destroy () {
    data = std::vector<uint64_t>();
  }
};
