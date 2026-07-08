#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mt5cpp {

struct Error { int code = 0; std::string message; };

template <class T> struct Result { T value{}; Error error{}; explicit operator bool() const { return error.code == 0; } };

enum class OrderType : std::uint32_t { Buy=0, Sell=1, BuyLimit=2, SellLimit=3, BuyStop=4, SellStop=5, BuyStopLimit=6, SellStopLimit=7 };
enum class OrderFilling : std::uint32_t { FOK=0, IOC=1, Return=2, BOC=3 };
enum class OrderTime : std::uint32_t { GTC=0, Day=1, Specified=2, SpecifiedDay=3 };
enum class TradeAction : std::uint32_t { Deal=1, Pending=5, Modify=6, Remove=8, CloseBy=10 };

enum class Timeframe : std::uint32_t {
  M1=1, M2=2, M3=3, M4=4, M5=5, M6=6, M10=10, M12=12, M15=15, M20=20, M30=30,
  H1=16385, H2=16386, H3=16387, H4=16388, H6=16390, H8=16392, H12=16396,
  D1=16408, W1=32769, MN1=49153,
};

enum class CopyTicksFlag : std::uint32_t { All = 0xFFFF'FFFFu, Info = 1, Trade = 2 };

struct AccountInfo {
  std::int64_t login = 0, leverage = 0;
  bool trade_allowed = false, trade_expert = false;
  double balance = 0, equity = 0, margin = 0, margin_free = 0, profit = 0;
  std::string name, server, currency, company;
};

struct SymbolTick {
  std::int64_t time = 0, time_msc = 0;
  double bid = 0, ask = 0, last = 0, volume_real = 0;
  std::uint64_t volume = 0;
  std::uint32_t flags = 0;
};

struct Rate {
  std::int64_t time = 0;
  double open = 0, high = 0, low = 0, close = 0;
  std::int64_t tick_volume = 0, real_volume = 0;
  std::int32_t spread = 0;
};

struct OrderRequest {
  TradeAction action = TradeAction::Deal;
  std::int64_t magic = 0, order = 0, expiration = 0, position = 0, position_by = 0;
  std::string symbol;
  double volume = 0, price = 0, stop_limit = 0, sl = 0, tp = 0;
  std::uint64_t deviation = 20;
  OrderType type = OrderType::Buy;
  OrderFilling type_filling = OrderFilling::FOK;
  OrderTime type_time = OrderTime::GTC;
  std::string comment;
};

struct OrderResult {
  std::uint32_t retcode = 0;
  std::int64_t deal = 0, order = 0;
  double volume = 0, price = 0, bid = 0, ask = 0;
  std::string comment;
  std::uint32_t request_id = 0;
  std::int32_t retcode_ext = 0;
};

} // namespace mt5cpp
