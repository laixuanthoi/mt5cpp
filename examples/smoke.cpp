#include <iostream>
#include "mt5cpp/Mt5Client.hpp"

int main(int argc, char** argv) {
  mt5cpp::Mt5Client mt5; // auto-discovers running terminal64.exe pipe
  const char* symbol = argc > 1 ? argv[1] : "EURUSDc";

  auto init = mt5.initialize();
  if (!init) {
    std::cerr << "initialize failed: " << init.error.message << "\n";
    return 1;
  }
  std::cout << "Connected to " << mt5.pipe_name() << " build=" << mt5.build() << "\n";

  auto total = mt5.symbols_total();
  if (total) std::cout << "symbols_total=" << total.value << "\n";
  auto names = mt5.symbol_names();
  if (names) std::cout << "symbol_names=" << names.value.size() << " first=" << (names.value.empty() ? "" : names.value.front()) << "\n";
  mt5.symbol_select(symbol, true);

  auto account = mt5.account_info();
  if (account) {
    std::cout << "login=" << account.value.login << " server=" << account.value.server
              << " balance=" << account.value.balance << " equity=" << account.value.equity << "\n";
  } else {
    std::cerr << account.error.code << ": " << account.error.message << "\n";
  }

  auto tick = mt5.symbol_info_tick(symbol);
  if (tick) {
    std::cout << symbol << " bid=" << tick.value.bid << " ask=" << tick.value.ask << "\n";
  } else {
    std::cerr << "symbol_info_tick failed: " << tick.error.message << "\n";
  }

  auto rates = mt5.copy_rates_from_pos(symbol, mt5cpp::Timeframe::M1, 0, 5);
  if (rates) {
    std::cout << "rates=" << rates.value.size() << "\n";
  } else {
    std::cerr << "copy_rates_from_pos failed: " << rates.error.message << "\n";
  }

  mt5.shutdown();
  return 0;
}
