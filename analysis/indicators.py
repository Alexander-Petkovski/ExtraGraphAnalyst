"""
ExtraGraphAnalyst - Technical Indicators
All computations are pure Python / numpy (no external TA libraries required).
Input:  a compact string: candle lines (open|high|low|close|volume) + "---" + flags line
Output: named sections separated by section headers, values pipe-delimited
"""
import math

NaN = float('nan')


# ── Parse input ───────────────────────────────────────────────────────────────
def _parse_input(arg):
    """Returns (opens, highs, lows, closes, volumes, flags_dict)"""
    parts = arg.split('---\n')
    candle_block = parts[0].strip()
    flags_line   = parts[1].strip() if len(parts) > 1 else ''

    opens, highs, lows, closes, vols = [], [], [], [], []
    for line in candle_block.splitlines():
        line = line.strip()
        if not line:
            continue
        vals = line.split('|')
        if len(vals) < 5:
            continue
        try:
            opens.append(float(vals[0]))
            highs.append(float(vals[1]))
            lows.append(float(vals[2]))
            closes.append(float(vals[3]))
            vols.append(float(vals[4]))
        except ValueError:
            continue

    # Parse flags: sma20|sma50|sma200|ema12|ema26|wma20|bb|rsi|macd|stoch|atr|vwap|fibo|sr|ichimoku
    flag_names = ['sma20','sma50','sma200','ema12','ema26','wma20','bb','rsi','macd',
                  'stoch','atr','vwap','fibo','sr','ichimoku']
    flags = {k: False for k in flag_names}
    if flags_line:
        vals = flags_line.split('|')
        for i, name in enumerate(flag_names):
            if i < len(vals):
                flags[name] = (vals[i] == '1')

    return opens, highs, lows, closes, vols, flags


# ── Utility functions ─────────────────────────────────────────────────────────
def _sma(data, period):
    n = len(data)
    result = [NaN] * n
    for i in range(period - 1, n):
        window = data[i - period + 1:i + 1]
        if all(math.isfinite(v) for v in window):
            result[i] = sum(window) / period
    return result


def _ema(data, period):
    n = len(data)
    result = [NaN] * n
    k = 2.0 / (period + 1)
    prev = NaN
    for i in range(n):
        if not math.isfinite(data[i]):
            continue
        if math.isnan(prev):
            # Seed with first valid value
            if i >= period - 1:
                window = data[i - period + 1:i + 1]
                if all(math.isfinite(v) for v in window):
                    prev = sum(window) / period
                    result[i] = prev
        else:
            prev = data[i] * k + prev * (1 - k)
            result[i] = prev
    return result


def _wma(data, period):
    n = len(data)
    result = [NaN] * n
    denom = period * (period + 1) / 2
    for i in range(period - 1, n):
        window = data[i - period + 1:i + 1]
        if all(math.isfinite(v) for v in window):
            result[i] = sum((j + 1) * v for j, v in enumerate(window)) / denom
    return result


def _stdev(data, mean, period):
    """Rolling standard deviation."""
    n = len(data)
    result = [NaN] * n
    for i in range(period - 1, n):
        window = data[i - period + 1:i + 1]
        if all(math.isfinite(v) for v in window) and math.isfinite(mean[i]):
            variance = sum((v - mean[i]) ** 2 for v in window) / period
            result[i] = math.sqrt(variance)
    return result


def _rsi(closes, period=14):
    n = len(closes)
    result = [NaN] * n
    gains = [0.0] * n
    losses = [0.0] * n
    for i in range(1, n):
        diff = closes[i] - closes[i - 1]
        gains[i]  = max(diff, 0.0)
        losses[i] = max(-diff, 0.0)
    avg_gain = sum(gains[1:period + 1]) / period
    avg_loss = sum(losses[1:period + 1]) / period
    for i in range(period, n):
        if i > period:
            avg_gain = (avg_gain * (period - 1) + gains[i])  / period
            avg_loss = (avg_loss * (period - 1) + losses[i]) / period
        rs = avg_gain / avg_loss if avg_loss > 1e-10 else 1e10
        result[i] = 100 - 100 / (1 + rs)
    return result


def _macd(closes, fast=12, slow=26, signal=9):
    ema_fast   = _ema(closes, fast)
    ema_slow   = _ema(closes, slow)
    n = len(closes)
    macd_line  = [NaN] * n
    for i in range(n):
        if math.isfinite(ema_fast[i]) and math.isfinite(ema_slow[i]):
            macd_line[i] = ema_fast[i] - ema_slow[i]
    sig_line   = _ema(macd_line, signal)
    histogram  = [NaN] * n
    for i in range(n):
        if math.isfinite(macd_line[i]) and math.isfinite(sig_line[i]):
            histogram[i] = macd_line[i] - sig_line[i]
    return macd_line, sig_line, histogram


def _bollinger(closes, period=20, std_mult=2.0):
    middle = _sma(closes, period)
    sd     = _stdev(closes, middle, period)
    n = len(closes)
    upper = [NaN] * n
    lower = [NaN] * n
    for i in range(n):
        if math.isfinite(middle[i]) and math.isfinite(sd[i]):
            upper[i] = middle[i] + std_mult * sd[i]
            lower[i] = middle[i] - std_mult * sd[i]
    return upper, middle, lower


def _stochastic(highs, lows, closes, k_period=14, d_period=3):
    n = len(closes)
    k_raw = [NaN] * n
    for i in range(k_period - 1, n):
        h = max(highs[i - k_period + 1:i + 1])
        l = min(lows[i  - k_period + 1:i + 1])
        if h - l > 1e-10:
            k_raw[i] = 100 * (closes[i] - l) / (h - l)
        else:
            k_raw[i] = 50.0
    k_smooth = _sma(k_raw, d_period)
    d_smooth = _sma(k_smooth, d_period)
    return k_smooth, d_smooth


def _atr(highs, lows, closes, period=14):
    n = len(closes)
    tr = [NaN] * n
    for i in range(1, n):
        tr[i] = max(highs[i] - lows[i],
                    abs(highs[i] - closes[i - 1]),
                    abs(lows[i]  - closes[i - 1]))
    return _ema(tr, period)


def _vwap(highs, lows, closes, volumes):
    """Cumulative VWAP (session-style from start of data)."""
    n = len(closes)
    result = [NaN] * n
    cum_tpv = 0.0
    cum_vol = 0.0
    for i in range(n):
        tp = (highs[i] + lows[i] + closes[i]) / 3.0
        cum_tpv += tp * volumes[i]
        cum_vol += volumes[i]
        result[i] = cum_tpv / cum_vol if cum_vol > 0 else tp
    return result


def _ichimoku(highs, lows, tenkan=9, kijun=26, span_b=52):
    n = len(highs)

    def mid(period, i):
        if i < period - 1:
            return NaN
        h = max(highs[i - period + 1:i + 1])
        l = min(lows[i  - period + 1:i + 1])
        return (h + l) / 2.0

    tenkan_line = [mid(tenkan, i) for i in range(n)]
    kijun_line  = [mid(kijun, i)  for i in range(n)]
    span_a      = [NaN] * n
    span_b_line = [NaN] * n
    for i in range(n):
        if math.isfinite(tenkan_line[i]) and math.isfinite(kijun_line[i]):
            span_a[i] = (tenkan_line[i] + kijun_line[i]) / 2.0
        span_b_line[i] = mid(span_b, i)
    return tenkan_line, kijun_line, span_a, span_b_line


def _fibonacci_levels(highs, lows):
    """Returns Fibonacci retracement levels based on global high/low."""
    valid_h = [h for h in highs if math.isfinite(h)]
    valid_l = [l for l in lows  if math.isfinite(l)]
    if not valid_h or not valid_l:
        return []
    high = max(valid_h)
    low  = min(valid_l)
    rng  = high - low
    ratios = [0.0, 0.236, 0.382, 0.5, 0.618, 0.786, 1.0]
    return [high - r * rng for r in ratios]


def _support_resistance(highs, lows, closes, lookback=20, min_touches=2):
    """Simple pivot-based S/R levels."""
    n = len(closes)
    levels = []
    for i in range(lookback, n - lookback):
        # Pivot high
        if highs[i] == max(highs[i - lookback:i + lookback + 1]):
            levels.append(highs[i])
        # Pivot low
        if lows[i] == min(lows[i - lookback:i + lookback + 1]):
            levels.append(lows[i])
    # Cluster nearby levels (within 0.5% of each other)
    levels.sort()
    clustered = []
    used = [False] * len(levels)
    for i in range(len(levels)):
        if used[i]:
            continue
        cluster = [levels[i]]
        for j in range(i + 1, len(levels)):
            if used[j]:
                continue
            if abs(levels[j] - levels[i]) / (levels[i] + 1e-10) < 0.005:
                cluster.append(levels[j])
                used[j] = True
        if len(cluster) >= min_touches:
            clustered.append(sum(cluster) / len(cluster))
        used[i] = True
    return clustered[:20]  # return top 20


# ── Format output ─────────────────────────────────────────────────────────────
def _fmt(values):
    """Format a list of floats as pipe-delimited string."""
    return '|'.join('nan' if (v is None or (isinstance(v, float) and math.isnan(v))) else f"{v:.6f}" for v in values)


def _section(name, values):
    return f"{name}\n{_fmt(values)}\n"


# ── Main entry point ──────────────────────────────────────────────────────────
def compute_all(arg):
    """
    Main function called by C++ PythonBridge.
    Returns all computed indicators as named sections.
    """
    try:
        opens, highs, lows, closes, vols, flags = _parse_input(arg)
        if not closes:
            return "ERROR:No candle data"

        result = ''

        if flags.get('sma20'):
            result += _section('SMA20', _sma(closes, 20))
        if flags.get('sma50'):
            result += _section('SMA50', _sma(closes, 50))
        if flags.get('sma200'):
            result += _section('SMA200', _sma(closes, 200))
        if flags.get('ema12'):
            result += _section('EMA12', _ema(closes, 12))
        if flags.get('ema26'):
            result += _section('EMA26', _ema(closes, 26))
        if flags.get('wma20'):
            result += _section('WMA20', _wma(closes, 20))
        if flags.get('bb'):
            upper, middle, lower = _bollinger(closes)
            result += _section('BB_UPPER',  upper)
            result += _section('BB_MIDDLE', middle)
            result += _section('BB_LOWER',  lower)
        if flags.get('rsi'):
            result += _section('RSI14', _rsi(closes))
        if flags.get('macd'):
            ml, sl, hist = _macd(closes)
            result += _section('MACD_LINE',   ml)
            result += _section('MACD_SIGNAL', sl)
            result += _section('MACD_HIST',   hist)
        if flags.get('stoch'):
            k, d = _stochastic(highs, lows, closes)
            result += _section('STOCH_K', k)
            result += _section('STOCH_D', d)
        if flags.get('atr'):
            result += _section('ATR14', _atr(highs, lows, closes))
        if flags.get('vwap'):
            result += _section('VWAP', _vwap(highs, lows, closes, vols))
        if flags.get('ichimoku'):
            t, k, sa, sb = _ichimoku(highs, lows)
            result += _section('ICHITENKAN', t)
            result += _section('ICHIKIJUN',  k)
            result += _section('ICHISPANA',  sa)
            result += _section('ICHISPANB',  sb)
        if flags.get('fibo'):
            fibo_levels = _fibonacci_levels(highs, lows)
            result += f"FIBOLEVELS\n{_fmt(fibo_levels)}\n"
        if flags.get('sr'):
            sr_levels = _support_resistance(highs, lows, closes)
            result += f"SRLEVELS\n{_fmt(sr_levels)}\n"

        return result if result else "OK:no_indicators_selected"

    except Exception as e:
        import traceback
        return f"ERROR:{type(e).__name__}: {e}\n{traceback.format_exc()}"
