import numpy as np
from typing import List, Tuple

def calc_sma(closes: List[float], period: int = 20) -> List[float]:
    """Simple Moving Average"""
    result = [np.nan] * len(closes)
    for i in range(period - 1, len(closes)):
        result[i] = np.mean(closes[i - period + 1:i + 1])
    return result

def calc_ema(closes: List[float], period: int = 9) -> List[float]:
    """Exponential Moving Average"""
    result = [np.nan] * len(closes)
    if len(closes) < period:
        return result

    multiplier = 2.0 / (period + 1)
    sma_sum = sum(closes[:period])
    result[period - 1] = sma_sum / period

    for i in range(period, len(closes)):
        result[i] = closes[i] * multiplier + result[i - 1] * (1 - multiplier)

    return result

def calc_wma(closes: List[float], period: int = 20) -> List[float]:
    """Weighted Moving Average"""
    result = [np.nan] * len(closes)
    weights = np.arange(1, period + 1, dtype=float)
    weight_sum = np.sum(weights)

    for i in range(period - 1, len(closes)):
        wma = np.sum(np.array(closes[i - period + 1:i + 1]) * weights) / weight_sum
        result[i] = wma

    return result

def calc_rsi(closes: List[float], period: int = 14) -> List[float]:
    """Wilder's RSI with Wilder smoothing"""
    result = [np.nan] * len(closes)
    if len(closes) < period + 1:
        return result

    deltas = np.diff(closes)
    gains = np.where(deltas > 0, deltas, 0.0)
    losses = np.where(deltas < 0, -deltas, 0.0)

    avg_gain = np.mean(gains[:period])
    avg_loss = np.mean(losses[:period])

    result[period] = 100 - (100 / (1 + avg_gain / max(avg_loss, 1e-9)))

    for i in range(period + 1, len(closes)):
        avg_gain = (avg_gain * (period - 1) + gains[i - 1]) / period
        avg_loss = (avg_loss * (period - 1) + losses[i - 1]) / period
        rs = avg_gain / max(avg_loss, 1e-9)
        result[i] = 100 - (100 / (1 + rs))

    return result

def calc_macd(closes: List[float], fast: int = 12, slow: int = 26, signal: int = 9) -> Tuple[List[float], List[float], List[float]]:
    """MACD with signal line and histogram"""
    ema_fast = calc_ema(closes, fast)
    ema_slow = calc_ema(closes, slow)

    macd_line = [ema_fast[i] - ema_slow[i] if not np.isnan(ema_fast[i]) and not np.isnan(ema_slow[i])
                 else np.nan for i in range(len(closes))]

    signal_line = calc_ema(macd_line, signal)

    histogram = [macd_line[i] - signal_line[i] if not np.isnan(macd_line[i]) and not np.isnan(signal_line[i])
                 else np.nan for i in range(len(closes))]

    return macd_line, signal_line, histogram

def calc_bollinger(closes: List[float], period: int = 20, num_std: float = 2.0) -> Tuple[List[float], List[float], List[float]]:
    """Bollinger Bands"""
    sma = calc_sma(closes, period)
    upper = [np.nan] * len(closes)
    lower = [np.nan] * len(closes)

    for i in range(period - 1, len(closes)):
        subset = closes[i - period + 1:i + 1]
        std = np.std(subset)
        middle = sma[i]
        upper[i] = middle + num_std * std
        lower[i] = middle - num_std * std

    return upper, sma, lower

def calc_stochastic(highs: List[float], lows: List[float], closes: List[float],
                    k_period: int = 14, d_period: int = 3, smooth: int = 3) -> Tuple[List[float], List[float]]:
    """Stochastic Oscillator"""
    k_line = [np.nan] * len(closes)

    for i in range(k_period - 1, len(closes)):
        h = max(highs[i - k_period + 1:i + 1])
        l = min(lows[i - k_period + 1:i + 1])
        k_line[i] = 100 * (closes[i] - l) / (h - l) if h != l else 50.0

    k_smooth = calc_sma(k_line, smooth)
    d_line = calc_sma(k_smooth, d_period)

    return k_smooth, d_line

def calc_atr(highs: List[float], lows: List[float], closes: List[float],
             period: int = 14) -> List[float]:
    """Average True Range"""
    tr = [highs[0] - lows[0]]

    for i in range(1, len(closes)):
        high_low = highs[i] - lows[i]
        high_close = abs(highs[i] - closes[i - 1])
        low_close = abs(lows[i] - closes[i - 1])
        tr.append(max(high_low, high_close, low_close))

    atr = [np.nan] * len(closes)
    atr[period - 1] = np.mean(tr[:period])

    for i in range(period, len(tr)):
        atr[i] = (atr[i - 1] * (period - 1) + tr[i]) / period

    return atr

def calc_vwap(highs: List[float], lows: List[float], closes: List[float],
              volumes: List[float]) -> List[float]:
    """Volume Weighted Average Price"""
    vwap = [np.nan] * len(closes)
    tp = [(h + l + c) / 3.0 for h, l, c in zip(highs, lows, closes)]

    cum_tp_vol = 0.0
    cum_vol = 0.0

    for i in range(len(closes)):
        cum_tp_vol += tp[i] * volumes[i]
        cum_vol += volumes[i]
        if cum_vol > 0:
            vwap[i] = cum_tp_vol / cum_vol

    return vwap

def calc_fibonacci(closes: List[float], highs: List[float], lows: List[float],
                   lookback: int = 50) -> List[float]:
    """Fibonacci Retracement Levels"""
    start_idx = max(0, len(closes) - lookback)
    swing_high = max(highs[start_idx:])
    swing_low = min(lows[start_idx:])

    range_val = swing_high - swing_low

    levels = [
        swing_low,                                          # 0%
        swing_low + 0.236 * range_val,                     # 23.6%
        swing_low + 0.382 * range_val,                     # 38.2%
        swing_low + 0.5 * range_val,                       # 50%
        swing_low + 0.618 * range_val,                     # 61.8%
        swing_low + 0.786 * range_val,                     # 78.6%
        swing_high,                                        # 100%
    ]

    return levels

def calc_support_resistance(closes: List[float], highs: List[float], lows: List[float],
                             lookback: int = 50) -> Tuple[List[float], List[float]]:
    """Support and Resistance Pivots"""
    start_idx = max(0, len(closes) - lookback)
    subset_highs = highs[start_idx:]
    subset_lows = lows[start_idx:]

    support_levels = []
    resistance_levels = []

    for i in range(2, len(subset_lows) - 2):
        if subset_lows[i] < subset_lows[i-1] and subset_lows[i] < subset_lows[i+1] and \
           subset_lows[i] < subset_lows[i-2] and subset_lows[i] < subset_lows[i+2]:
            support_levels.append(subset_lows[i])

        if subset_highs[i] > subset_highs[i-1] and subset_highs[i] > subset_highs[i+1] and \
           subset_highs[i] > subset_highs[i-2] and subset_highs[i] > subset_highs[i+2]:
            resistance_levels.append(subset_highs[i])

    return support_levels, resistance_levels

def calc_ichimoku(highs: List[float], lows: List[float], closes: List[float]) -> dict:
    """Ichimoku Cloud"""
    tenkan = [np.nan] * len(closes)
    kijun = [np.nan] * len(closes)
    span_a = [np.nan] * len(closes)
    span_b = [np.nan] * len(closes)
    chikou = [np.nan] * len(closes)

    for i in range(8, len(closes)):
        h9 = max(highs[i-8:i+1])
        l9 = min(lows[i-8:i+1])
        tenkan[i] = (h9 + l9) / 2.0

    for i in range(25, len(closes)):
        h26 = max(highs[i-25:i+1])
        l26 = min(lows[i-25:i+1])
        kijun[i] = (h26 + l26) / 2.0

    for i in range(26, len(closes)):
        if not np.isnan(tenkan[i]) and not np.isnan(kijun[i]):
            span_a[i] = (tenkan[i] + kijun[i]) / 2.0

    for i in range(51, len(closes)):
        h52 = max(highs[i-51:i+1])
        l52 = min(lows[i-51:i+1])
        span_b[i] = (h52 + l52) / 2.0

    for i in range(26):
        if i + 26 < len(closes):
            chikou[i + 26] = closes[i]

    return {
        'tenkan': tenkan,
        'kijun': kijun,
        'span_a': span_a,
        'span_b': span_b,
        'chikou': chikou
    }
