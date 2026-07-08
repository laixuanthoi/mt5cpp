#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace mt5cpp {

class NamedPipeTransport {
public:
  NamedPipeTransport();
  explicit NamedPipeTransport(std::string pipe_name);
  ~NamedPipeTransport();

  NamedPipeTransport(const NamedPipeTransport&) = delete;
  NamedPipeTransport& operator=(const NamedPipeTransport&) = delete;

  void open(std::chrono::milliseconds timeout = std::chrono::seconds(60));
  static std::string discover_pipe_name(std::chrono::milliseconds probe_timeout = std::chrono::milliseconds(500));

  std::vector<std::uint8_t> transact(std::uint32_t command_id, const std::vector<std::uint8_t>& params,
                                     std::chrono::milliseconds timeout = std::chrono::seconds(10));
  void close();
  bool connected() const;
  const std::string& pipe_name() const { return pipe_name_; }

private:
  std::string pipe_name_;
  void* pipe_ = nullptr;
  mutable std::mutex mutex_;
};

} // namespace mt5cpp
