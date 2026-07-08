#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mt5cpp/Mt5Client.hpp"

struct Stats {
  std::atomic<int> ok{0};
  std::atomic<int> fail{0};
  std::mutex mu;
  std::vector<double> ms;

  void add(bool success, double latency_ms) {
    if (success) ok++; else fail++;
    std::lock_guard<std::mutex> lock(mu);
    ms.push_back(latency_ms);
  }
};

int main(int argc, char** argv) {
  std::string symbol = argc > 1 ? argv[1] : "EURUSDc";
  int loops = argc > 2 ? std::atoi(argv[2]) : 1000;
  int threads = argc > 3 ? std::atoi(argv[3]) : 1;
  if (loops <= 0) loops = 1000;
  if (threads <= 0) threads = 1;

  mt5cpp::Mt5Client mt5;
  auto init = mt5.initialize();
  if (!init) {
    std::cerr << "initialize failed: " << init.error.message << "\n";
    return 1;
  }
  std::cout << "Connected build=" << mt5.build() << " pipe=" << mt5.pipe_name() << "\n";
  std::cout << "Stress: symbol=" << symbol << " loops=" << loops << " threads=" << threads << "\n";

  Stats tick_stats, rates_stats, account_stats;
  auto t0 = std::chrono::steady_clock::now();

  auto worker = [&](int tid) {
    for (int i = tid; i < loops; i += threads) {
      auto one = [&](auto&& fn, Stats& st) {
        auto a = std::chrono::steady_clock::now();
        bool success = fn();
        auto b = std::chrono::steady_clock::now();
        st.add(success, std::chrono::duration<double, std::milli>(b - a).count());
      };
      one([&] { return (bool)mt5.symbol_info_tick(symbol); }, tick_stats);
      if (i % 10 == 0) one([&] { return (bool)mt5.copy_rates_from_pos(symbol, mt5cpp::Timeframe::M1, 0, 10); }, rates_stats);
      if (i % 25 == 0) one([&] { return (bool)mt5.account_info(); }, account_stats);
    }
  };

  std::vector<std::thread> pool;
  for (int t = 0; t < threads; ++t) pool.emplace_back(worker, t);
  for (auto& t : pool) t.join();

  auto total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

  auto print = [](const char* name, Stats& st) {
    std::vector<double> v;
    {
      std::lock_guard<std::mutex> lock(st.mu);
      v = st.ms;
    }
    std::sort(v.begin(), v.end());
    auto pct = [&](double p) { return v.empty() ? 0.0 : v[std::min(v.size() - 1, static_cast<size_t>(p * (v.size() - 1)))]; };
    double avg = 0;
    for (double x : v) avg += x;
    if (!v.empty()) avg /= v.size();
    std::cout << name << ": ok=" << st.ok << " fail=" << st.fail
              << " avg_ms=" << avg << " p50=" << pct(0.50)
              << " p95=" << pct(0.95) << " p99=" << pct(0.99) << "\n";
  };

  print("tick", tick_stats);
  print("rates", rates_stats);
  print("account", account_stats);
  std::cout << "total_ms=" << total_ms << " ops="
            << (tick_stats.ok + tick_stats.fail + rates_stats.ok + rates_stats.fail + account_stats.ok + account_stats.fail)
            << "\n";
  return (tick_stats.fail || rates_stats.fail || account_stats.fail) ? 2 : 0;
}
