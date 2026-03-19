"""
ExtraGraphAnalyst - Predictors / Forecasters
Each predictor returns N bars of projected close prices.
Input:  close prices (one per line) + "---" + flags (linreg|emaproj|momentum|holtwinters)
Output: PRED:name|recommended|confidence sections followed by pipe-delimited forecast values
"""
import math

NaN = float('nan')
FORECAST_BARS = 20  # how many bars ahead to project


# ── Parse input ───────────────────────────────────────────────────────────────
def _parse_input(arg):
    parts  = arg.split('---\n')
    closes = []
    for line in parts[0].strip().splitlines():
        line = line.strip()
        if line:
            try:
                closes.append(float(line))
            except ValueError:
                pass
    flags_line = parts[1].strip() if len(parts) > 1 else ''
    flag_names = ['linreg', 'emaproj', 'momentum', 'holtwinters']
    flags = {k: False for k in flag_names}
    if flags_line:
        vals = flags_line.split('|')
        for i, name in enumerate(flag_names):
            if i < len(vals):
                flags[name] = (vals[i] == '1')
    return closes, flags


# ── Predictors ────────────────────────────────────────────────────────────────
def _linear_regression(closes, n_bars=FORECAST_BARS, lookback=50):
    """Simple OLS linear regression extrapolation."""
    data = closes[-lookback:] if len(closes) > lookback else closes
    m    = len(data)
    if m < 5:
        return [NaN] * n_bars, 0.0

    x_mean = (m - 1) / 2.0
    y_mean = sum(data) / m

    ss_xy = sum((i - x_mean) * (data[i] - y_mean) for i in range(m))
    ss_xx = sum((i - x_mean) ** 2 for i in range(m))

    slope     = ss_xy / ss_xx if ss_xx > 1e-10 else 0.0
    intercept = y_mean - slope * x_mean

    # Compute R^2 for confidence
    y_pred   = [intercept + slope * i for i in range(m)]
    ss_res   = sum((data[i] - y_pred[i]) ** 2 for i in range(m))
    ss_tot   = sum((data[i] - y_mean) ** 2 for i in range(m))
    r2       = 1 - ss_res / ss_tot if ss_tot > 1e-10 else 0.0
    confidence = max(0.0, min(1.0, r2))

    forecast = [intercept + slope * (m + i) for i in range(n_bars)]
    return forecast, confidence


def _ema_projection(closes, n_bars=FORECAST_BARS, period=12):
    """Project the EMA trend forward."""
    if len(closes) < period:
        return [NaN] * n_bars, 0.0

    k = 2.0 / (period + 1)
    ema = closes[0]
    for v in closes[1:]:
        ema = v * k + ema * (1 - k)

    # Compute EMA velocity (slope of recent EMA)
    ema_vals = []
    ema2 = closes[0]
    for v in closes:
        ema2 = v * k + ema2 * (1 - k)
        ema_vals.append(ema2)

    recent = ema_vals[-period:]
    velocity = (recent[-1] - recent[0]) / len(recent) if len(recent) > 1 else 0.0

    last = closes[-1]
    forecast = [ema + velocity * (i + 1) for i in range(n_bars)]

    # Confidence: based on trend consistency
    diffs = [ema_vals[i + 1] - ema_vals[i] for i in range(len(ema_vals) - 1)]
    if diffs:
        same_dir = sum(1 for d in diffs[-20:] if (d > 0) == (velocity > 0))
        confidence = same_dir / min(20, len(diffs))
    else:
        confidence = 0.5

    return forecast, confidence


def _momentum_predictor(closes, n_bars=FORECAST_BARS, lookback=20):
    """Average momentum extrapolation."""
    if len(closes) < lookback + 1:
        return [NaN] * n_bars, 0.0

    recent = closes[-lookback:]
    returns = [(recent[i] - recent[i-1]) / recent[i-1]
               for i in range(1, len(recent))
               if recent[i-1] != 0]

    if not returns:
        return [NaN] * n_bars, 0.0

    avg_return = sum(returns) / len(returns)
    last = closes[-1]
    forecast = [last * (1 + avg_return) ** (i + 1) for i in range(n_bars)]

    # Confidence: low variance in returns = high confidence
    mean_r  = avg_return
    var_r   = sum((r - mean_r)**2 for r in returns) / len(returns)
    std_r   = math.sqrt(var_r) if var_r > 0 else 0.0
    confidence = 1.0 / (1.0 + std_r * 100)  # normalize

    return forecast, confidence


def _holt_winters(closes, n_bars=FORECAST_BARS, alpha=0.3, beta=0.1):
    """Double exponential smoothing (Holt's linear trend method)."""
    if len(closes) < 4:
        return [NaN] * n_bars, 0.0

    # Initialize
    level = closes[0]
    trend = closes[1] - closes[0]

    for v in closes[1:]:
        prev_level = level
        level = alpha * v + (1 - alpha) * (level + trend)
        trend = beta * (level - prev_level) + (1 - beta) * trend

    forecast = [level + trend * (i + 1) for i in range(n_bars)]

    # Confidence: how well the model fits recent data (MAE-based)
    fitted = [NaN] * len(closes)
    lv, tr = closes[0], closes[1] - closes[0]
    for i in range(1, len(closes)):
        prev_lv = lv
        lv = alpha * closes[i] + (1 - alpha) * (lv + tr)
        tr = beta * (lv - prev_lv) + (1 - beta) * tr
        fitted[i] = lv

    recent = closes[-20:]
    recent_fitted = fitted[-20:]
    errors = [abs(closes[i] - recent_fitted[i])
              for i in range(len(recent))
              if math.isfinite(recent_fitted[i])]
    if errors and closes[-1] != 0:
        mae_pct    = (sum(errors) / len(errors)) / abs(closes[-1])
        confidence = max(0.0, 1.0 - mae_pct * 10)
    else:
        confidence = 0.5

    return forecast, confidence


# ── Auto-recommend ────────────────────────────────────────────────────────────
def _pick_best(results):
    """Mark the predictor with highest confidence as recommended."""
    if not results:
        return
    best_idx = max(range(len(results)), key=lambda i: results[i][2])
    for i, r in enumerate(results):
        r[3] = (i == best_idx)


# ── Main entry point ──────────────────────────────────────────────────────────
def compute_all(arg):
    """
    Main function called by C++.
    Returns sections: PRED:name|recommended|confidence followed by forecast values.
    """
    try:
        closes, flags = _parse_input(arg)
        if not closes:
            return "ERROR:No close prices"

        results = []  # [name, forecast, confidence, recommended]

        if flags.get('linreg'):
            fc, conf = _linear_regression(closes)
            results.append(['Lin.Regression', fc, conf, False])

        if flags.get('emaproj'):
            fc, conf = _ema_projection(closes)
            results.append(['EMA Projection', fc, conf, False])

        if flags.get('momentum'):
            fc, conf = _momentum_predictor(closes)
            results.append(['Momentum', fc, conf, False])

        if flags.get('holtwinters'):
            fc, conf = _holt_winters(closes)
            results.append(['Holt-Winters', fc, conf, False])

        if not results:
            return "OK:no_predictors_selected"

        # Auto-recommend best
        _pick_best(results)

        out = ''
        for name, forecast, conf, recommended in results:
            out += f"PRED:{name}|{'1' if recommended else '0'}|{conf:.4f}\n"
            out += '|'.join('nan' if (v is None or math.isnan(v)) else f"{v:.6f}"
                            for v in forecast) + '\n'

        return out

    except Exception as e:
        import traceback
        return f"ERROR:{type(e).__name__}: {e}\n{traceback.format_exc()}"
