# MetaTrader5 Python -> direct C++ IPC notes

MetaQuotes documents `MetaTrader5` for Python as communicating with the MetaTrader 5 terminal through interprocess communication, but the protocol itself is not officially documented.

## Direct IPC findings

Public reverse-engineering projects (notably `github.com/Mukbeast4/go-mt5`, MIT) show that current MT5 terminals expose Windows named pipes derived from the running `terminal64.exe` path:

```text
\\.\pipe\MT5.Terminal.<SHA256(UTF16LE("\\\\?\\" + lower_terminal_path))>
```

Frame format:

```text
Request:  [payload_len:LE32][cmd_id:LE32][params...]
Response: [payload_len:LE32][cmd_echo:LE32][success:LE32][data...]
```

Primitive encoding:

- integers: little-endian 32/64 bit
- doubles: IEEE754 little-endian
- dynamic strings: `[char_count:LE32][UTF-16LE chars]`
- fixed strings: fixed UTF-16LE slots padded by NULs

Useful command ids for the initial C++ port:

| Command | ID |
| --- | ---: |
| initialize | 4 |
| copy rates from pos | 108 |
| symbol info tick | 172 |
| account info | 190 |
| order send | 161 |

## Current package direction

This repo now implements a C++17 direct named-pipe client. No EA/MQL5 bridge is used.

Risks:

- MetaQuotes can change the protocol or pipe naming in future builds.
- Error payloads and many structs still need broader decoding.
- Trading calls need validation on a demo account first.
- This is not an official MetaQuotes C++ SDK.
