# mt5cpp

C++17 package for talking directly to a running MetaTrader 5 terminal through Windows Named Pipe IPC. No Python runtime and no EA/MQL5 bridge are required.

> MetaQuotes does not officially document this terminal IPC protocol. Validate against your MT5 build/broker before production use.

## Implemented API subset

- Auto-discover running `terminal64.exe` pipe
- `initialize()` / `shutdown()`
- `symbols_total()`, `symbol_names(group)`, `symbol_select(symbol)`
- `account_info()`
- `symbol_info_tick(symbol)`
- `copy_ticks_from(symbol, from_epoch, count, flags)`
- `copy_rates_from_pos(symbol, timeframe, start_pos, count)`
- `order_send(OrderRequest)`
- `send_raw(command_id, params)` for experiments

## Hardening included

- Serialized access inside one `Mt5Client` for thread-safe request/response matching.
- Multi-client concurrent connections tested.
- Auto-reconnect/re-initialize retry after pipe IO failure.
- MT5 build guard via `set_min_supported_build()`.
- CMake install/export files for `find_package(mt5cpp CONFIG REQUIRED)`.
- Better command/pipe error messages.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DMT5CPP_BUILD_EXAMPLES=ON
cmake --build build --config Release
```

## Run tests

Start MetaTrader 5 (`terminal64.exe`) and log in, then:

```powershell
.\build\Release\mt5cpp_smoke.exe EURUSDc
.\build\Release\mt5cpp_stress.exe EURUSDc 100000 4
.\build\Release\mt5cpp_concurrent.exe EURUSDc 4 1000
.\build\Release\mt5cpp_ticks.exe EURUSDc 1000000
```

## Use from another C++ program

As subdirectory:

```cmake
add_subdirectory(path/to/mt5cpp)
target_link_libraries(my_app PRIVATE mt5cpp::mt5cpp)
```

Installed package:

```powershell
cmake --install build --config Release --prefix C:\mt5cpp
```

```cmake
find_package(mt5cpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE mt5cpp::mt5cpp)
```

```cpp
#include "mt5cpp/Mt5Client.hpp"

mt5cpp::Mt5Client mt5;
mt5.set_timeout(std::chrono::seconds(10));
mt5.set_auto_reconnect(true);

if (!mt5.initialize()) throw std::runtime_error("MT5 connect failed");
mt5.symbol_select("EURUSDc", true);
auto tick = mt5.symbol_info_tick("EURUSDc");
auto ticks = mt5.copy_ticks_from("EURUSDc", 0, 100000);
```

## Protocol notes

Pipe name:

```text
\\.\pipe\MT5.Terminal.<SHA256(UTF16LE("\\\\?\\" + lower_terminal_path))>
```

Request:

```text
[payload_len:LE32][cmd_id:LE32][params...]
```

Response:

```text
[payload_len:LE32][cmd_echo:LE32][success:LE32][data...]
```
