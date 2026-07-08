#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "mt5cpp/Mt5Client.hpp"

int main(int argc, char** argv) {
  std::string symbol = argc > 1 ? argv[1] : "EURUSDc";
  int count = argc > 2 ? std::atoi(argv[2]) : 10000;
  std::int64_t from = argc > 3 ? std::atoll(argv[3]) : 0; // 0 = terminal chooses earliest available for request
  if (count <= 0) count = 10000;

  mt5cpp::Mt5Client mt5;
  auto init = mt5.initialize();
  if (!init) { std::cerr << "init failed: " << init.error.message << "\n"; return 1; }

  auto t0 = std::chrono::steady_clock::now();
  auto ticks = mt5.copy_ticks_from(symbol, from, count, mt5cpp::CopyTicksFlag::All);
  auto t1 = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

  if (!ticks) {
    std::cerr << "copy_ticks_from failed: " << ticks.error.message << "\n";
    return 2;
  }

  std::cout << "symbol=" << symbol << " requested=" << count << " got=" << ticks.value.size()
            << " ms=" << ms << " ticks_per_sec=" << (ticks.value.empty() ? 0.0 : ticks.value.size() * 1000.0 / ms) << "\n";
  if (!ticks.value.empty()) {
    const auto& first = ticks.value.front();
    const auto& last = ticks.value.back();
    std::cout << "first time=" << first.time << " bid=" << first.bid << " ask=" << first.ask << "\n";
    std::cout << "last  time=" << last.time << " bid=" << last.bid << " ask=" << last.ask << "\n";
  }
  return 0;
}
