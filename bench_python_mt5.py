import sys, time, statistics
import MetaTrader5 as mt5

symbol = sys.argv[1] if len(sys.argv) > 1 else "EURUSDc"
loops = int(sys.argv[2]) if len(sys.argv) > 2 else 10000

if not mt5.initialize():
    print("initialize failed", mt5.last_error())
    sys.exit(1)
print("version", mt5.version(), "account", mt5.account_info().login if mt5.account_info() else None)
mt5.symbol_select(symbol, True)

def bench_call(name, n, fn):
    lat=[]; ok=fail=0
    t0=time.perf_counter()
    for i in range(n):
        a=time.perf_counter()
        r=fn(i)
        b=time.perf_counter()
        if r is None:
            fail += 1
        else:
            ok += 1
        lat.append((b-a)*1000)
    total=(time.perf_counter()-t0)*1000
    lat.sort()
    def pct(p):
        return lat[min(len(lat)-1, int(p*(len(lat)-1)))] if lat else 0
    avg=sum(lat)/len(lat) if lat else 0
    print(f"{name}: ok={ok} fail={fail} avg_ms={avg:.6f} p50={pct(.50):.6f} p95={pct(.95):.6f} p99={pct(.99):.6f} total_ms={total:.3f}")

bench_call("py_tick", loops, lambda i: mt5.symbol_info_tick(symbol))
bench_call("py_rates", max(1, loops//10), lambda i: mt5.copy_rates_from_pos(symbol, mt5.TIMEFRAME_M1, 0, 10))
bench_call("py_account", max(1, loops//25), lambda i: mt5.account_info())

for count in [1000, 10000, 100000, 1000000]:
    t0=time.perf_counter()
    ticks=mt5.copy_ticks_from(symbol, 0, count, mt5.COPY_TICKS_ALL)
    ms=(time.perf_counter()-t0)*1000
    got=0 if ticks is None else len(ticks)
    print(f"py_copy_ticks count={count} got={got} ms={ms:.3f} ticks_per_sec={(got*1000/ms if ms else 0):.3f}")

mt5.shutdown()
