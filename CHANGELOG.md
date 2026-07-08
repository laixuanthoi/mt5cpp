# Changelog

## 0.1.0

Initial package release candidate.

- Direct Windows Named Pipe IPC to a running MetaTrader 5 terminal.
- No Python runtime and no EA/MQL5 bridge required.
- CMake target: `mt5cpp::mt5cpp`.
- CMake install package: `find_package(mt5cpp CONFIG REQUIRED)`.
- APIs: initialize/shutdown, account info, symbols total/list/select, symbol tick, historical ticks, rates from position, order send, raw command.
- Example tools: smoke, stress, concurrent, bulk ticks.
