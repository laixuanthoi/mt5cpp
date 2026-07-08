# mt5cpp

`mt5cpp` is a small C++17 library for talking directly to a running MetaTrader 5 terminal through Windows Named Pipe IPC.

It is intended for C++ trading/data applications that want a native API without embedding Python and without installing an EA/MQL5 bridge.

> [!WARNING]
> MetaQuotes does not officially document this terminal IPC protocol. This project is experimental and should be validated against your MT5 terminal build, broker, account type, and risk controls before any production or live-trading use.

## Features

- Native C++17 static library.
- Windows + MSVC/CMake support.
- Direct IPC to `terminal64.exe` named pipe.
- No Python runtime required.
- No Expert Advisor / MQL5 bridge required.
- Auto-discovery of a running MT5 terminal pipe.
- Thread-safe request serialization inside one `Mt5Client`.
- Auto-reconnect/re-initialize retry after pipe IO failure.
- Multi-client concurrent use tested.
- CMake target and install package: `mt5cpp::mt5cpp`.

## Current API coverage

| Area | API |
| --- | --- |
| Connection | `initialize()`, `shutdown()`, `connected()`, `set_timeout()`, `set_auto_reconnect()` |
| Version/build guard | `build()`, `set_min_supported_build()` |
| Account | `account_info()` |
| Symbols | `symbols_total()`, `symbol_names(group)`, `symbol_select(symbol, enable)` |
| Market data | `symbol_info_tick(symbol)`, `copy_ticks_from(symbol, from_epoch, count, flags)`, `copy_rates_from_pos(symbol, timeframe, start_pos, count)` |
| Trading | `order_send(OrderRequest)` |
| Experimental | `send_raw(command_id, params)` |

## Requirements

- Windows.
- MetaTrader 5 terminal (`terminal64.exe`) running and logged in.
- CMake 3.20+.
- A C++17 compiler; tested with MSVC / Visual Studio 2022 Build Tools.

## Quick build

```powershell
git clone https://github.com/<your-org>/mt5cpp.git
cd mt5cpp
cmake -S . -B build -G "Visual Studio 17 2022" -DMT5CPP_BUILD_EXAMPLES=ON
cmake --build build --config Release
```

## Run examples/tests

Start MetaTrader 5 and log in first. Use your broker's actual symbol name. For Exness cent-style symbols this may be `EURUSDc`, not `EURUSD`.

```powershell
.\build\Release\mt5cpp_smoke.exe EURUSDc
.\build\Release\mt5cpp_ticks.exe EURUSDc 1000000
.\build\Release\mt5cpp_stress.exe EURUSDc 100000 4
.\build\Release\mt5cpp_concurrent.exe EURUSDc 4 1000
```

Example output from local validation on MT5 build 5833:

```text
symbols_total=43
symbol_names=43 first=USDAUCc
EURUSDc bid=... ask=...
rates=5

tick: ok=10000 fail=0
rates: ok=1000 fail=0
account: ok=400 fail=0
```

## Use in another C++ project

### Option 1: add as subdirectory

```cmake
add_subdirectory(path/to/mt5cpp)
target_link_libraries(my_app PRIVATE mt5cpp::mt5cpp)
```

### Option 2: install and use `find_package`

Install:

```powershell
cmake --build build --config Release
cmake --install build --config Release --prefix C:\mt5cpp
```

Consumer project:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app LANGUAGES CXX)

find_package(mt5cpp CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE mt5cpp::mt5cpp)
```

Configure consumer:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\mt5cpp
cmake --build build --config Release
```

### Minimal C++ example

```cpp
#include <chrono>
#include <iostream>
#include <stdexcept>

#include <mt5cpp/Mt5Client.hpp>

int main() {
  mt5cpp::Mt5Client mt5;
  mt5.set_timeout(std::chrono::seconds(10));
  mt5.set_auto_reconnect(true);
  mt5.set_min_supported_build(5000);

  auto init = mt5.initialize();
  if (!init) throw std::runtime_error(init.error.message);

  const std::string symbol = "EURUSDc";
  mt5.symbol_select(symbol, true);

  auto tick = mt5.symbol_info_tick(symbol);
  if (!tick) throw std::runtime_error(tick.error.message);

  std::cout << symbol << " bid=" << tick.value.bid
            << " ask=" << tick.value.ask << "\n";

  auto ticks = mt5.copy_ticks_from(symbol, 0, 100000);
  if (!ticks) throw std::runtime_error(ticks.error.message);
  std::cout << "ticks=" << ticks.value.size() << "\n";
}
```

## Error handling model

Most API calls return `mt5cpp::Result<T>`:

```cpp
auto account = mt5.account_info();
if (!account) {
  std::cerr << account.error.code << ": " << account.error.message << "\n";
  return;
}
```

`Result<T>` is intentionally simple and dependency-free. `error.code == 0` means success.

## Threading model

- A single `Mt5Client` serializes requests internally, so it is safe to call from multiple threads, but calls are processed one at a time per client.
- For read-heavy workloads, you can create multiple `Mt5Client` instances. Concurrent multi-client reads have been tested.
- For order execution, prefer one controlled execution client/queue so your strategy can enforce rate limits and risk checks.

## Performance notes

Local benchmark observations:

- Small calls such as `symbol_info_tick`, `account_info`, and small `copy_rates_from_pos` are roughly comparable to the official Python package.
- Bulk tick reads are fast enough for many data/research workflows; 100k and 1M tick reads have been tested.
- This is not a low-latency HFT gateway. MT5 terminal, broker checks, network routing, and trade server latency dominate actual order execution latency.

## What this is good for

- Native C++ retail algo trading apps.
- Data collectors using an already logged-in MT5 terminal.
- Backtest/research tooling that needs MT5 history.
- Integrating MT5 account/market data into a larger C++ system.

## What this is not good for

- Microsecond-level HFT.
- Exchange colocated trading.
- Broker/exchange gateway replacement.
- Systems requiring officially supported protocol guarantees from MetaQuotes.

## Protocol notes

Pipe name derivation:

```text
\\.\pipe\MT5.Terminal.<SHA256(UTF16LE("\\\\?\\" + lower_terminal_path))>
```

Request framing:

```text
[payload_len:LE32][cmd_id:LE32][params...]
```

Response framing:

```text
[payload_len:LE32][cmd_echo:LE32][success:LE32][data...]
```

Primitive encoding currently used by implemented commands:

- integers: little-endian 32/64 bit
- doubles: IEEE754 little-endian
- dynamic strings: `[char_count:LE32][UTF-16LE chars]`
- fixed strings: fixed UTF-16LE slots padded by NULs

## Roadmap

Likely next APIs:

- `terminal_info()`, `version()`
- `symbol_info()` full struct
- `copy_rates_from()`, `copy_rates_range()`
- `copy_ticks_range()`
- `order_check()`, `order_calc_margin()`, `order_calc_profit()`
- `positions_get()`, `orders_get()`
- `history_orders_get()`, `history_deals_get()`
- optional raw/zero-copy APIs for very large historical data pulls

## License

MIT. See [LICENSE](LICENSE).
