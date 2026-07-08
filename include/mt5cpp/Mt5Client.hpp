#pragma once

#include "mt5cpp/NamedPipeTransport.hpp"
#include "mt5cpp/Types.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace mt5cpp {

class Mt5Client {
public:
  explicit Mt5Client(std::string pipe_name = {});
  ~Mt5Client();

  Result<bool> initialize();
  Result<bool> shutdown();
  bool connected() const;
  void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
  void set_auto_reconnect(bool enabled) { auto_reconnect_ = enabled; }
  void set_min_supported_build(int build) { min_supported_build_ = build; }
  int build() const { return build_; }
  const std::string& pipe_name() const { return transport_.pipe_name(); }

  Result<AccountInfo> account_info();
  Result<int> symbols_total();
  Result<std::vector<std::string>> symbol_names(const std::string& group = {});
  Result<bool> symbol_select(const std::string& symbol, bool enable = true);
  Result<SymbolTick> symbol_info_tick(const std::string& symbol);
  Result<std::vector<SymbolTick>> copy_ticks_from(const std::string& symbol, std::int64_t date_from, int count,
                                                  CopyTicksFlag flags = CopyTicksFlag::All);
  Result<std::vector<Rate>> copy_rates_from_pos(const std::string& symbol, Timeframe timeframe, int start_pos, int count);
  Result<OrderResult> order_send(const OrderRequest& request);

  Result<std::vector<std::uint8_t>> send_raw(std::uint32_t command_id, const std::vector<std::uint8_t>& params = {});

private:
  NamedPipeTransport transport_;
  std::chrono::milliseconds timeout_ = std::chrono::seconds(10);
  bool auto_reconnect_ = true;
  int build_ = 0;
  int min_supported_build_ = 5000;

  std::vector<std::uint8_t> rpc(std::uint32_t command_id, const std::vector<std::uint8_t>& params = {}, bool allow_reconnect = true);
  void open_and_initialize();
};

} // namespace mt5cpp
