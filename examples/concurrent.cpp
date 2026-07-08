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
  std::atomic<int> ok{0}, fail{0};
  std::mutex mu;
  std::vector<double> ms;
  void add(bool success, double latency_ms) {
    success ? ok++ : fail++;
    std::lock_guard<std::mutex> lock(mu);
    ms.push_back(latency_ms);
  }
};

static void print_stats(const char* name, Stats& st) {
  std::vector<double> v;
  { std::lock_guard<std::mutex> lock(st.mu); v = st.ms; }
  std::sort(v.begin(), v.end());
  auto pct = [&](double p) { return v.empty() ? 0.0 : v[std::min(v.size() - 1, static_cast<size_t>(p * (v.size() - 1)))]; };
  double avg = 0; for (double x : v) avg += x; if (!v.empty()) avg /= v.size();
  std::cout << name << ": ok=" << st.ok << " fail=" << st.fail
            << " avg_ms=" << avg << " p50=" << pct(0.50)
            << " p95=" << pct(0.95) << " p99=" << pct(0.99) << "\n";
}

int main(int argc, char** argv) {
  std::string symbol = argc > 1 ? argv[1] : "EURUSDc";
  int clients = argc > 2 ? std::atoi(argv[2]) : 4;
  int loops_per_client = argc > 3 ? std::atoi(argv[3]) : 1000;
  if (clients <= 0) clients = 4;
  if (loops_per_client <= 0) loops_per_client = 1000;

  std::string pipe;
  try {
    pipe = mt5cpp::NamedPipeTransport::discover_pipe_name();
  } catch (const std::exception& e) {
    std::cerr << "discover failed: " << e.what() << "\n";
    return 1;
  }

  std::cout << "Concurrent clients: pipe=" << pipe << " symbol=" << symbol
            << " clients=" << clients << " loops/client=" << loops_per_client << "\n";

  Stats init_stats, tick_stats, rates_stats;
  std::atomic<int> ready{0};
  std::atomic<bool> start{false};
  auto total_start = std::chrono::steady_clock::now();

  std::vector<std::thread> pool;
  for (int c = 0; c < clients; ++c) {
    pool.emplace_back([&, c] {
      mt5cpp::Mt5Client mt5(pipe);
      auto t0 = std::chrono::steady_clock::now();
      auto init = mt5.initialize();
      auto t1 = std::chrono::steady_clock::now();
      init_stats.add((bool)init, std::chrono::duration<double, std::milli>(t1 - t0).count());
      if (!init) return;
      ready++;
      while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
      for (int i = 0; i < loops_per_client; ++i) {
        auto a = std::chrono::steady_clock::now();
        auto tick = mt5.symbol_info_tick(symbol);
        auto b = std::chrono::steady_clock::now();
        tick_stats.add((bool)tick, std::chrono::duration<double, std::milli>(b - a).count());
        if (i % 20 == 0) {
          auto r0 = std::chrono::steady_clock::now();
          auto rates = mt5.copy_rates_from_pos(symbol, mt5cpp::Timeframe::M1, 0, 10);
          auto r1 = std::chrono::steady_clock::now();
          rates_stats.add((bool)rates, std::chrono::duration<double, std::milli>(r1 - r0).count());
        }
      }
    });
  }

  while (ready.load() + init_stats.fail.load() < clients) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  start.store(true, std::memory_order_release);
  for (auto& t : pool) t.join();

  auto total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - total_start).count();
  print_stats("init", init_stats);
  print_stats("tick", tick_stats);
  print_stats("rates", rates_stats);
  std::cout << "total_ms=" << total_ms << "\n";
  return (init_stats.fail || tick_stats.fail || rates_stats.fail) ? 2 : 0;
}
