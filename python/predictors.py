import numpy as np
from typing import List, Tuple, Callable

# ---------------------------------------------------------------------------
# Backtest engine
# ---------------------------------------------------------------------------

def _backtest_confidence(closes: List[float], predictor_fn: Callable, holdout_pct: float = 0.15) -> float:
    """
    Compute a confidence score via walk-forward backtesting.

    Splits closes into a training window and a holdout window.
    Runs the predictor on the training data, predicts exactly holdout_n bars,
    then measures normalised RMSE against the actual holdout prices.
    Returns a score in [0.05, 0.99] where higher means the model tracked
    the holdout period more closely.
    """
    n = len(closes)
    holdout_n = max(10, int(n * holdout_pct))

    if n - holdout_n < 20:
        return 0.3

    train  = closes[:n - holdout_n]
    actual = closes[n - holdout_n:]

    try:
        preds, _ = predictor_fn(train, bars_ahead=holdout_n)
        k = min(len(preds), len(actual))
        if k == 0:
            return 0.3
        rmse        = np.sqrt(np.mean((np.array(preds[:k]) - np.array(actual[:k])) ** 2))
        price_range = max(actual) - min(actual)
        score       = float(np.clip(1.0 - rmse / (price_range + 1e-9), 0.05, 0.99))
    except Exception:
        score = 0.1

    return score


# ---------------------------------------------------------------------------
# Predictors
# ---------------------------------------------------------------------------

def predict_linear_regression(closes: List[float], lookback: int = 50, bars_ahead: int = 30) -> Tuple[List[float], float]:
    """Linear regression — confidence via holdout backtest."""
    y = np.array(closes[-lookback:], dtype=float)
    x = np.arange(len(y), dtype=float)

    x_mean, y_mean = x.mean(), y.mean()
    slope     = np.sum((x - x_mean) * (y - y_mean)) / np.sum((x - x_mean) ** 2)
    intercept = y_mean - slope * x_mean

    future_x    = np.arange(len(y), len(y) + bars_ahead, dtype=float)
    predictions = (slope * future_x + intercept).tolist()

    def _fit(c, bars_ahead=30):
        yy = np.array(c[-lookback:], dtype=float)
        xx = np.arange(len(yy), dtype=float)
        xm, ym = xx.mean(), yy.mean()
        sl = np.sum((xx - xm) * (yy - ym)) / (np.sum((xx - xm) ** 2) + 1e-9)
        ic = ym - sl * xm
        fx = np.arange(len(yy), len(yy) + bars_ahead, dtype=float)
        return (sl * fx + ic).tolist(), 0.0

    confidence = _backtest_confidence(closes, _fit)
    return predictions, confidence


def predict_ema_projection(closes: List[float], period: int = 20, bars_ahead: int = 30) -> Tuple[List[float], float]:
    """EMA slope projection — confidence via holdout backtest."""
    from indicators import calc_ema

    ema_vals  = calc_ema(closes, period)
    ema_clean = [v for v in ema_vals if v is not None and not np.isnan(v)]

    if len(ema_clean) < 10:
        return [closes[-1]] * bars_ahead, 0.2

    slope    = np.mean(np.diff(ema_clean[-10:]))
    last_ema = ema_clean[-1]

    predictions = [last_ema + slope * (i + 1) for i in range(bars_ahead)]

    def _fit(c, bars_ahead=30):
        ev  = calc_ema(c, period)
        ec  = [v for v in ev if v is not None and not np.isnan(v)]
        if len(ec) < 10:
            return [c[-1]] * bars_ahead, 0.0
        sl = np.mean(np.diff(ec[-10:]))
        le = ec[-1]
        return [le + sl * (i + 1) for i in range(bars_ahead)], 0.0

    confidence = _backtest_confidence(closes, _fit)
    return predictions, confidence


def predict_momentum(closes: List[float], period: int = 14, bars_ahead: int = 30) -> Tuple[List[float], float]:
    """Momentum with RSI dampening — confidence via holdout backtest."""
    if len(closes) < period + 1:
        return [closes[-1]] * bars_ahead, 0.2

    roc = (closes[-1] - closes[-period - 1]) / (closes[-period - 1] + 1e-9)

    changes   = np.diff(closes[-period * 2:])
    gains     = np.where(changes > 0, changes, 0)
    losses    = np.where(changes < 0, -changes, 0)
    avg_gain  = np.mean(gains)  if len(gains)  > 0 else 0
    avg_loss  = np.mean(losses) if len(losses) > 0 else 0.001
    rsi       = 100 - 100 / (1 + avg_gain / (avg_loss + 1e-9))

    dampen = 0.3 if rsi > 70 or rsi < 30 else 1.0
    step   = closes[-1] * roc / (period + 1e-9) * dampen

    predictions = [closes[-1] + step * (i + 1) for i in range(bars_ahead)]

    def _fit(c, bars_ahead=30):
        if len(c) < period + 1:
            return [c[-1]] * bars_ahead, 0.0
        rc  = (c[-1] - c[-period - 1]) / (c[-period - 1] + 1e-9)
        ch  = np.diff(c[-period * 2:])
        g   = np.where(ch > 0, ch, 0)
        l   = np.where(ch < 0, -ch, 0)
        ag  = np.mean(g) if len(g) > 0 else 0
        al  = np.mean(l) if len(l) > 0 else 0.001
        rs  = 100 - 100 / (1 + ag / (al + 1e-9))
        dm  = 0.3 if rs > 70 or rs < 30 else 1.0
        st  = c[-1] * rc / (period + 1e-9) * dm
        return [c[-1] + st * (i + 1) for i in range(bars_ahead)], 0.0

    confidence = _backtest_confidence(closes, _fit)
    return predictions, confidence


def predict_holt_winters(closes: List[float], alpha: float = 0.3, beta: float = 0.1, bars_ahead: int = 30) -> Tuple[List[float], float]:
    """Holt's double exponential smoothing — confidence via holdout backtest."""
    data = np.array(closes, dtype=float)
    if len(data) < 4:
        return [closes[-1]] * bars_ahead, 0.2

    def _hw(c, bars_ahead=30):
        d     = np.array(c, dtype=float)
        level = d[0]
        trend = d[1] - d[0]
        for i in range(1, len(d)):
            prev_level = level
            level = alpha * d[i] + (1 - alpha) * (level + trend)
            trend = beta  * (level - prev_level) + (1 - beta) * trend
        return [level + trend * (i + 1) for i in range(bars_ahead)], 0.0

    predictions, _ = _hw(closes, bars_ahead)
    confidence     = _backtest_confidence(closes, _hw)
    return predictions, confidence


# ---------------------------------------------------------------------------
# Auto-recommend
# ---------------------------------------------------------------------------

def auto_recommend(closes: List[float], bars_ahead: int = 30) -> dict:
    """
    Rank all predictors by their holdout backtest score.
    Each score is already computed via _backtest_confidence inside each
    predictor, so we simply call them and collect the returned confidence.
    """
    predictors = [
        ('Holt-Winters',       lambda c: predict_holt_winters(c,       bars_ahead=bars_ahead)),
        ('Linear Regression',  lambda c: predict_linear_regression(c,  bars_ahead=bars_ahead)),
        ('EMA Projection',     lambda c: predict_ema_projection(c,      bars_ahead=bars_ahead)),
        ('Momentum',           lambda c: predict_momentum(c,            bars_ahead=bars_ahead)),
    ]

    results = {}
    for name, fn in predictors:
        try:
            _, score = fn(closes)
        except Exception:
            score = 0.1
        results[name] = score

    return results
