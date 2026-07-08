#include "mt5cpp/Mt5Client.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#ifdef MT5CPP_WINDOWS
#include <windows.h>
#endif

namespace mt5cpp {
namespace {
constexpr std::uint32_t CmdInitialize = 4;
constexpr std::uint32_t CmdAccountInfo = 190;
constexpr std::uint32_t CmdCopyTicksFrom = 104;
constexpr std::uint32_t CmdCopyRatesFromPos = 108;
constexpr std::uint32_t CmdSymbolSelect = 171;
constexpr std::uint32_t CmdSymbolInfoTick = 172;
constexpr std::uint32_t CmdSymbolsTotal = 173;
constexpr std::uint32_t CmdSymbolsGet = 174;
constexpr std::uint32_t CmdSymbolsGetByGroup = 175;
constexpr size_t SymbolInfoRecordBytes = 2993;
constexpr size_t SymbolNameOffset = 2929;
constexpr std::uint32_t CmdOrderSend = 161;

std::uint32_t u32(const std::vector<std::uint8_t>& b, size_t off) { if (off+4>b.size()) throw std::runtime_error("decode u32 eof"); return (std::uint32_t)b[off]|((std::uint32_t)b[off+1]<<8)|((std::uint32_t)b[off+2]<<16)|((std::uint32_t)b[off+3]<<24); }
std::int32_t i32(const std::vector<std::uint8_t>& b, size_t off) { return (std::int32_t)u32(b, off); }
std::uint64_t u64(const std::vector<std::uint8_t>& b, size_t off) { if (off+8>b.size()) throw std::runtime_error("decode u64 eof"); std::uint64_t v=0; for(int i=7;i>=0;--i) v=(v<<8)|b[off+i]; return v; }
std::int64_t i64(const std::vector<std::uint8_t>& b, size_t off) { return (std::int64_t)u64(b, off); }
double f64(const std::vector<std::uint8_t>& b, size_t off) { auto v=u64(b, off); double d; std::memcpy(&d,&v,8); return d; }

class Writer {
public:
  std::vector<std::uint8_t> b;
  void u8(std::uint8_t v){ b.push_back(v); }
  void u32(std::uint32_t v){ b.push_back((std::uint8_t)v); b.push_back((std::uint8_t)(v>>8)); b.push_back((std::uint8_t)(v>>16)); b.push_back((std::uint8_t)(v>>24)); }
  void i64(std::int64_t v){ auto x=(std::uint64_t)v; for(int i=0;i<8;++i) b.push_back((std::uint8_t)(x>>(8*i))); }
  void u64(std::uint64_t v){ for(int i=0;i<8;++i) b.push_back((std::uint8_t)(v>>(8*i))); }
  void f64(double d){ std::uint64_t v; std::memcpy(&v,&d,8); u64(v); }
  void str(const std::string& s){ auto w=utf16(s); u32((std::uint32_t)w.size()); for(wchar_t c:w){ b.push_back((std::uint8_t)c); b.push_back((std::uint8_t)(c>>8)); } }
  void fixed_str(const std::string& s, size_t slot_bytes){ size_t start=b.size(); b.resize(start+slot_bytes); auto w=utf16(s); size_t max=(slot_bytes/2)-1; if(w.size()>max) w.resize(max); for(size_t i=0;i<w.size();++i){ b[start+i*2]=(std::uint8_t)w[i]; b[start+i*2+1]=(std::uint8_t)(w[i]>>8); } }
private:
  static std::wstring utf16(const std::string& s){
#ifdef MT5CPP_WINDOWS
    if(s.empty()) return {}; int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0); std::wstring w(n,L'\0'); MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),n); return w;
#else
    return std::wstring(s.begin(), s.end());
#endif
  }
};

std::string fixed_utf16(const std::vector<std::uint8_t>& b, size_t off, size_t slot_bytes) {
  if (off + slot_bytes > b.size()) throw std::runtime_error("decode fixed string eof");
  size_t chars = 0; while (chars*2+1 < slot_bytes) { if (b[off+chars*2]==0 && b[off+chars*2+1]==0) break; ++chars; }
#ifdef MT5CPP_WINDOWS
  std::wstring w(chars, L'\0'); for(size_t i=0;i<chars;++i) w[i]=(wchar_t)(b[off+i*2] | (b[off+i*2+1]<<8));
  if(w.empty()) return {}; int n=WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),nullptr,0,nullptr,nullptr); std::string s(n,'\0'); WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),s.data(),n,nullptr,nullptr); return s;
#else
  std::string s; for(size_t i=0;i<chars;++i) s.push_back((char)b[off+i*2]); return s;
#endif
}

template<class T, class F> Result<T> wrap(F&& fn) { try { return {fn(), {}}; } catch(const std::exception& e) { return {{}, {1000, e.what()}}; } }
} // namespace

Mt5Client::Mt5Client(std::string pipe_name) : transport_(std::move(pipe_name)) {}
Mt5Client::~Mt5Client() { transport_.close(); }
bool Mt5Client::connected() const { return transport_.connected(); }

void Mt5Client::open_and_initialize() {
  if (!transport_.connected()) transport_.open(std::chrono::seconds(60));
  Writer w; w.u32(3); w.str("C++");
  auto data = transport_.transact(CmdInitialize, w.b, timeout_);
  if (data.size() >= 4) build_ = (int)u32(data, 0);
  if (min_supported_build_ > 0 && build_ > 0 && build_ < min_supported_build_) {
    throw std::runtime_error("unsupported MT5 build " + std::to_string(build_) + ", expected >= " + std::to_string(min_supported_build_));
  }
}

std::vector<std::uint8_t> Mt5Client::rpc(std::uint32_t command_id, const std::vector<std::uint8_t>& params, bool allow_reconnect) {
  if (!transport_.connected()) open_and_initialize();
  try {
    return transport_.transact(command_id, params, timeout_);
  } catch (...) {
    if (!auto_reconnect_ || !allow_reconnect) throw;
    transport_.close();
    open_and_initialize();
    return transport_.transact(command_id, params, timeout_);
  }
}

Result<std::vector<std::uint8_t>> Mt5Client::send_raw(std::uint32_t command_id, const std::vector<std::uint8_t>& params) {
  return wrap<std::vector<std::uint8_t>>([&]{ return rpc(command_id, params); });
}

Result<bool> Mt5Client::initialize() {
  return wrap<bool>([&]{ open_and_initialize(); return true; });
}
Result<bool> Mt5Client::shutdown() { transport_.close(); return {true,{}}; }

Result<AccountInfo> Mt5Client::account_info() {
  return wrap<AccountInfo>([&]{
    auto d = rpc(CmdAccountInfo);
    if (d.size() < 8 + 704) throw std::runtime_error("account_info response too short");
    const size_t strings = d.size() - 704;
    AccountInfo a; a.login = i64(d,0);
    size_t m = 8;
    a.leverage = i32(d, m+4); a.trade_allowed = d[m+16] != 0; a.trade_expert = d[m+17] != 0;
    a.balance = f64(d,m+27); a.profit = f64(d,m+43); a.equity = f64(d,m+51); a.margin = f64(d,m+59); a.margin_free = f64(d,m+67);
    a.name = fixed_utf16(d, strings, 256); a.server = fixed_utf16(d, strings+256, 128); a.currency = fixed_utf16(d, strings+384, 64); a.company = fixed_utf16(d, strings+448, 256);
    return a;
  });
}

Result<int> Mt5Client::symbols_total() {
  return wrap<int>([&]{
    auto d = rpc(CmdSymbolsTotal);
    if (d.size() < 4) throw std::runtime_error("symbols_total response too short");
    return (int)u32(d, 0);
  });
}

Result<std::vector<std::string>> Mt5Client::symbol_names(const std::string& group) {
  return wrap<std::vector<std::string>>([&]{
    Writer w;
    const auto cmd = group.empty() ? CmdSymbolsGet : CmdSymbolsGetByGroup;
    if (!group.empty()) w.str(group);
    auto d = rpc(cmd, w.b);
    if (d.size() < 4) throw std::runtime_error("symbols_get response too short");
    std::uint32_t n = u32(d, 0);
    if (d.size() < 4 + (size_t)n * SymbolInfoRecordBytes) throw std::runtime_error("symbols_get truncated response");
    std::vector<std::string> names; names.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      names.push_back(fixed_utf16(d, 4 + (size_t)i * SymbolInfoRecordBytes + SymbolNameOffset, 64));
    }
    return names;
  });
}

Result<bool> Mt5Client::symbol_select(const std::string& symbol, bool enable) {
  return wrap<bool>([&]{
    Writer w; w.str(symbol); w.u8(enable ? 1 : 0);
    rpc(CmdSymbolSelect, w.b);
    return true;
  });
}

Result<SymbolTick> Mt5Client::symbol_info_tick(const std::string& symbol) {
  return wrap<SymbolTick>([&]{
    Writer w; w.str(symbol); auto d = rpc(CmdSymbolInfoTick, w.b);
    if (d.size() < 56) throw std::runtime_error("symbol_info_tick response too short");
    SymbolTick t; t.time=i64(d,0); t.bid=f64(d,8); t.ask=f64(d,16); t.last=f64(d,24); t.volume=u64(d,32); t.time_msc=i64(d,40); t.flags=u32(d,48); t.volume_real=f64(d,52); return t;
  });
}

Result<std::vector<SymbolTick>> Mt5Client::copy_ticks_from(const std::string& symbol, std::int64_t date_from, int count, CopyTicksFlag flags) {
  return wrap<std::vector<SymbolTick>>([&]{
    Writer w; w.str(symbol); w.i64(date_from); w.u32((std::uint32_t)count); w.u32((std::uint32_t)flags);
    auto d = rpc(CmdCopyTicksFrom, w.b);
    if (d.size() < 4) throw std::runtime_error("ticks response too short");
    std::uint32_t n = u32(d,0); std::vector<SymbolTick> out; out.reserve(n); size_t off=4;
    for(std::uint32_t i=0;i<n;++i){ SymbolTick t; t.time=i64(d,off); t.bid=f64(d,off+8); t.ask=f64(d,off+16); t.last=f64(d,off+24); t.volume=u64(d,off+32); t.time_msc=i64(d,off+40); t.flags=u32(d,off+48); t.volume_real=f64(d,off+52); out.push_back(t); off+=60; }
    return out;
  });
}

Result<std::vector<Rate>> Mt5Client::copy_rates_from_pos(const std::string& symbol, Timeframe timeframe, int start_pos, int count) {
  return wrap<std::vector<Rate>>([&]{
    Writer w; w.str(symbol); w.u32((std::uint32_t)timeframe); w.u32((std::uint32_t)start_pos); w.u32((std::uint32_t)count);
    auto d = rpc(CmdCopyRatesFromPos, w.b);
    if (d.size() < 4) throw std::runtime_error("rates response too short");
    std::uint32_t n = u32(d,0); std::vector<Rate> out; out.reserve(n); size_t off=4;
    for(std::uint32_t i=0;i<n;++i){ Rate r; r.time=i64(d,off); r.open=f64(d,off+8); r.high=f64(d,off+16); r.low=f64(d,off+24); r.close=f64(d,off+32); r.tick_volume=i64(d,off+40); r.spread=i32(d,off+48); r.real_volume=i64(d,off+52); out.push_back(r); off+=60; }
    return out;
  });
}

Result<OrderResult> Mt5Client::order_send(const OrderRequest& q) {
  return wrap<OrderResult>([&]{
    Writer w; w.u32((std::uint32_t)q.action); w.i64(q.magic); w.i64(q.order); w.fixed_str(q.symbol,64); w.f64(q.volume); w.f64(q.price); w.f64(q.stop_limit); w.f64(q.sl); w.f64(q.tp); w.u64(q.deviation); w.u32((std::uint32_t)q.type); w.u32((std::uint32_t)q.type_filling); w.u32((std::uint32_t)q.type_time); w.i64(q.expiration); w.fixed_str(q.comment,64); w.i64(q.position); w.i64(q.position_by);
    auto d = rpc(CmdOrderSend, w.b);
    if (d.size() < 260) throw std::runtime_error("order_send response too short");
    OrderResult o; o.retcode=u32(d,0); o.deal=i64(d,4); o.order=i64(d,12); o.volume=f64(d,20); o.price=f64(d,28); o.bid=f64(d,36); o.ask=f64(d,44); o.comment=fixed_utf16(d,52,200); o.request_id=u32(d,252); o.retcode_ext=i32(d,256); return o;
  });
}

} // namespace mt5cpp
