#include "mt5cpp/NamedPipeTransport.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef MT5CPP_WINDOWS
#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace mt5cpp {
namespace {
#ifdef MT5CPP_WINDOWS
std::runtime_error winerr(const char* what) {
  return std::runtime_error(std::string(what) + " failed, GetLastError=" + std::to_string(GetLastError()));
}

std::wstring widen_utf8(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
  return w;
}

std::string narrow_utf8(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}

std::uint32_t u32le(const std::uint8_t* p) {
  return (std::uint32_t)p[0] | ((std::uint32_t)p[1] << 8) | ((std::uint32_t)p[2] << 16) | ((std::uint32_t)p[3] << 24);
}
void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
  b.push_back((std::uint8_t)v); b.push_back((std::uint8_t)(v >> 8)); b.push_back((std::uint8_t)(v >> 16)); b.push_back((std::uint8_t)(v >> 24));
}

std::wstring process_path(DWORD pid) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return {};
  std::wstring buf(MAX_PATH, L'\0'); DWORD size = (DWORD)buf.size();
  if (!QueryFullProcessImageNameW(h, 0, buf.data(), &size)) { CloseHandle(h); return {}; }
  CloseHandle(h); buf.resize(size); return buf;
}

std::vector<std::wstring> terminal_paths() {
  std::vector<std::wstring> out;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return out;
  PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"terminal64.exe") == 0) {
        auto p = process_path(pe.th32ProcessID);
        if (!p.empty() && std::find(out.begin(), out.end(), p) == out.end()) out.push_back(p);
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return out;
}

std::array<std::uint8_t, 32> sha256(const std::vector<std::uint8_t>& bytes) {
  BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
  std::array<std::uint8_t, 32> digest{};
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) throw std::runtime_error("BCryptOpenAlgorithmProvider(SHA256) failed");
  if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) { BCryptCloseAlgorithmProvider(alg, 0); throw std::runtime_error("BCryptCreateHash failed"); }
  BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()), (ULONG)bytes.size(), 0);
  BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0);
  BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg, 0); return digest;
}

std::string pipe_name_for_terminal_path(std::wstring path) {
  std::transform(path.begin(), path.end(), path.begin(), towlower);
  std::wstring input = L"\\\\?\\" + path;
  std::vector<std::uint8_t> bytes(input.size() * 2);
  std::memcpy(bytes.data(), input.data(), bytes.size());
  auto h = sha256(bytes);
  static const char* hex = "0123456789ABCDEF";
  std::string s = R"(\\.\pipe\MT5.Terminal.)";
  for (auto v : h) { s.push_back(hex[v >> 4]); s.push_back(hex[v & 15]); }
  return s;
}
#endif
} // namespace

NamedPipeTransport::NamedPipeTransport() = default;
NamedPipeTransport::NamedPipeTransport(std::string pipe_name) : pipe_name_(std::move(pipe_name)) {}
NamedPipeTransport::~NamedPipeTransport() { close(); }

std::string NamedPipeTransport::discover_pipe_name(std::chrono::milliseconds probe_timeout) {
#ifdef MT5CPP_WINDOWS
  for (const auto& p : terminal_paths()) {
    auto name = pipe_name_for_terminal_path(p);
    NamedPipeTransport t(name);
    try { t.open(probe_timeout); t.close(); return name; } catch (...) {}
  }
  throw std::runtime_error("no responding MT5 terminal pipe found; start terminal64.exe and log in");
#else
  throw std::runtime_error("Windows only");
#endif
}

void NamedPipeTransport::open(std::chrono::milliseconds timeout) {
#ifdef MT5CPP_WINDOWS
  std::lock_guard<std::mutex> lock(mutex_);
  if (pipe_) { CloseHandle(static_cast<HANDLE>(pipe_)); pipe_ = nullptr; }
  if (pipe_name_.empty()) pipe_name_ = discover_pipe_name();
  std::wstring wname = widen_utf8(pipe_name_);
  auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    HANDLE h = CreateFileW(wname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) { pipe_ = h; return; }
    DWORD err = GetLastError();
    if (err != ERROR_PIPE_BUSY && err != ERROR_FILE_NOT_FOUND) throw winerr("CreateFileW(open MT5 pipe)");
    if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("timeout opening MT5 pipe: " + pipe_name_);
    WaitNamedPipeW(wname.c_str(), 250);
  }
#endif
}

std::vector<std::uint8_t> NamedPipeTransport::transact(std::uint32_t command_id, const std::vector<std::uint8_t>& params, std::chrono::milliseconds) {
#ifdef MT5CPP_WINDOWS
  std::lock_guard<std::mutex> lock(mutex_);
  if (!pipe_) throw std::runtime_error("MT5 pipe is not open");
  HANDLE h = static_cast<HANDLE>(pipe_);
  std::vector<std::uint8_t> req; req.reserve(8 + params.size());
  put_u32(req, (std::uint32_t)(4 + params.size())); put_u32(req, command_id); req.insert(req.end(), params.begin(), params.end());
  DWORD written = 0;
  if (!WriteFile(h, req.data(), (DWORD)req.size(), &written, nullptr)) {
    auto e = winerr("WriteFile"); CloseHandle(h); pipe_ = nullptr; throw e;
  }

  std::uint8_t lenbuf[4]; DWORD got = 0;
  if (!ReadFile(h, lenbuf, 4, &got, nullptr) || got != 4) {
    auto e = winerr("ReadFile(length)"); CloseHandle(h); pipe_ = nullptr; throw e;
  }
  std::uint32_t payload_len = u32le(lenbuf);
  if (payload_len < 8 || payload_len > 64u * 1024u * 1024u) throw std::runtime_error("invalid MT5 payload length");
  std::vector<std::uint8_t> payload(payload_len); std::uint32_t off = 0;
  while (off < payload_len) {
    DWORD n = 0;
    if (!ReadFile(h, payload.data() + off, payload_len - off, &n, nullptr)) {
      auto e = winerr("ReadFile(payload)"); CloseHandle(h); pipe_ = nullptr; throw e;
    }
    if (n == 0) { CloseHandle(h); pipe_ = nullptr; throw std::runtime_error("ReadFile(payload) returned 0 bytes"); }
    off += n;
  }
  std::uint32_t echo = u32le(payload.data()); std::uint32_t ok = u32le(payload.data() + 4);
  if (echo != command_id) throw std::runtime_error("MT5 response command id mismatch: sent " + std::to_string(command_id) + ", got " + std::to_string(echo));
  if (!ok) throw std::runtime_error("MT5 command failed: cmd=" + std::to_string(command_id) + ", data_len=" + std::to_string(payload.size() - 8));
  return {payload.begin() + 8, payload.end()};
#else
  (void)command_id; (void)params; return {};
#endif
}

void NamedPipeTransport::close() {
#ifdef MT5CPP_WINDOWS
  std::lock_guard<std::mutex> lock(mutex_);
  if (pipe_) { CloseHandle(static_cast<HANDLE>(pipe_)); pipe_ = nullptr; }
#endif
}
bool NamedPipeTransport::connected() const { std::lock_guard<std::mutex> lock(mutex_); return pipe_ != nullptr; }

} // namespace mt5cpp
