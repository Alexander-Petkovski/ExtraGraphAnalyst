"""
ExtraGraphAnalyst – Yahoo Finance Integration
Uses the yfinance library to download OHLCV data by ticker symbol.

C++ calls:
  fetch_ticker(arg)   arg = "TICKER|PERIOD|INTERVAL"
  search_ticker(arg)  arg = "QUERY"  (returns pipe-separated suggestions)
"""
import math


# ── Period / interval mappings ────────────────────────────────────────────────
# Maps ExtraGraphAnalyst timeframe labels to yfinance interval strings.
# yfinance intraday data (< 1d) is only available for the last 60 days.
TF_TO_INTERVAL = {
    '15m':     '15m',
    '30m':     '30m',
    '1H':      '60m',
    '4H':      '1h',    # yfinance has no 4H; use 1h and note it
    'Daily':   '1d',
    'Weekly':  '1wk',
    'Monthly': '1mo',
    'Yearly':  '3mo',
}

# Maximum period allowed per interval (yfinance hard limits)
INTERVAL_MAX_PERIOD = {
    '1m':  '7d',
    '5m':  '60d',
    '15m': '60d',
    '30m': '60d',
    '60m': '730d',
    '90m': '60d',
    '1h':  '730d',
    '1d':  'max',
    '5d':  'max',
    '1wk': 'max',
    '1mo': 'max',
    '3mo': 'max',
}

INTRADAY_INTERVALS = {'1m', '5m', '15m', '30m', '60m', '90m', '1h'}


def _period_days(period_str):
    """Rough conversion of a period string to days, for capping intraday."""
    mapping = {
        '1d': 1, '5d': 5, '7d': 7,
        '1mo': 30, '3mo': 90, '6mo': 180,
        '1y': 365, '2y': 730, '5y': 1825,
        '10y': 3650, 'ytd': 365, 'max': 99999,
        '60d': 60, '730d': 730,
    }
    return mapping.get(period_str.lower(), 365)


def _cap_period(interval, period):
    """
    Cap the requested period to the maximum allowed for this interval.
    Returns the (possibly adjusted) period string.
    """
    max_period = INTERVAL_MAX_PERIOD.get(interval, 'max')
    if max_period == 'max':
        return period
    if _period_days(period) > _period_days(max_period):
        return max_period
    return period


def fetch_ticker(arg):
    """
    Download OHLCV data from Yahoo Finance.
    arg format:  "TICKER|PERIOD|INTERVAL"
      TICKER   e.g. AAPL, BTC-USD, ^GSPC, EURUSD=X
      PERIOD   e.g. 1mo, 3mo, 6mo, 1y, 2y, 5y, 10y, max
      INTERVAL e.g. 15m, 30m, 1H, Daily, Weekly, Monthly (ExtraGraphAnalyst labels)

    Returns flat pipe-delimited string:
        label|open|high|low|close|volume
    one line per candle.  On error: "ERROR:reason"
    """
    try:
        import yfinance as yf
    except ImportError:
        return "ERROR:yfinance not installed. Run: pip install yfinance"

    parts = arg.strip().split('|')
    if len(parts) < 3:
        return "ERROR:Invalid argument format. Expected TICKER|PERIOD|INTERVAL"

    ticker_sym = parts[0].strip().upper()
    period     = parts[1].strip()
    tf_label   = parts[2].strip()

    if not ticker_sym:
        return "ERROR:Empty ticker symbol"

    # Map timeframe label to yfinance interval
    interval = TF_TO_INTERVAL.get(tf_label, '1d')

    # Cap period to what yfinance allows for this interval
    period = _cap_period(interval, period)

    try:
        ticker = yf.Ticker(ticker_sym)
        df     = ticker.history(period=period, interval=interval,
                                auto_adjust=True, actions=False)

        if df is None or df.empty:
            return f"ERROR:No data returned for {ticker_sym}. Check the symbol and try again."

        lines = []
        for ts, row in df.iterrows():
            try:
                # Format timestamp as readable label
                ts_str = str(ts)
                # Trim to date (+ time for intraday)
                if interval in INTRADAY_INTERVALS:
                    # e.g. "2024-01-15 09:30"
                    lbl = ts_str[:16].replace('T', ' ')
                else:
                    lbl = ts_str[:10]

                o = float(row['Open'])
                h = float(row['High'])
                l = float(row['Low'])
                c = float(row['Close'])
                v = float(row.get('Volume', 0) or 0)

                if any(math.isnan(x) for x in [o, h, l, c]):
                    continue

                lines.append(f"{lbl}|{o}|{h}|{l}|{c}|{v}")
            except (KeyError, ValueError, TypeError):
                continue

        if not lines:
            return f"ERROR:Data downloaded but all rows were invalid for {ticker_sym}"

        return '\n'.join(lines)

    except Exception as e:
        return f"ERROR:{type(e).__name__}: {e}"


def fetch_ticker_info(arg):
    """
    Get basic info about a ticker: name, sector, market cap, currency.
    arg = "TICKER"
    Returns pipe-delimited: name|sector|currency|marketCap|exchange
    On error: "ERROR:reason"
    """
    try:
        import yfinance as yf
    except ImportError:
        return "ERROR:yfinance not installed"

    ticker_sym = arg.strip().upper()
    if not ticker_sym:
        return "ERROR:Empty ticker"

    try:
        t    = yf.Ticker(ticker_sym)
        info = t.info

        name      = info.get('longName',    info.get('shortName', ticker_sym))
        sector    = info.get('sector',      info.get('quoteType', 'N/A'))
        currency  = info.get('currency',    'N/A')
        mktcap    = info.get('marketCap',   0)
        exchange  = info.get('exchange',    'N/A')

        # Format market cap
        if mktcap >= 1e12:
            mktcap_str = f"{mktcap / 1e12:.2f}T"
        elif mktcap >= 1e9:
            mktcap_str = f"{mktcap / 1e9:.2f}B"
        elif mktcap >= 1e6:
            mktcap_str = f"{mktcap / 1e6:.2f}M"
        else:
            mktcap_str = str(mktcap) if mktcap else 'N/A'

        # Sanitize: replace pipes in text fields
        name     = name.replace('|', '-')
        sector   = sector.replace('|', '-')
        exchange = exchange.replace('|', '-')

        return f"{name}|{sector}|{currency}|{mktcap_str}|{exchange}"

    except Exception as e:
        return f"ERROR:{type(e).__name__}: {e}"


def search_ticker(arg):
    """
    Search for tickers matching a query string.
    arg = "QUERY"
    Returns newline-delimited results, each: SYMBOL|NAME|EXCHANGE|TYPE
    On error or no results: "ERROR:reason" or empty string.
    """
    query = arg.strip()
    if not query or len(query) < 1:
        return ""

    try:
        import yfinance as yf
    except ImportError:
        return "ERROR:yfinance not installed"

    try:
        # yfinance.Search available in yfinance >= 0.2.18
        search = yf.Search(query, max_results=10, news_count=0)
        quotes = search.quotes

        if not quotes:
            return ""

        lines = []
        for q in quotes:
            sym      = q.get('symbol',      '').replace('|', '')
            name     = q.get('longname',    q.get('shortname', '')).replace('|', '-')
            exchange = q.get('exchange',    '').replace('|', '')
            qtype    = q.get('quoteType',   '').replace('|', '')
            if sym:
                lines.append(f"{sym}|{name}|{exchange}|{qtype}")

        return '\n'.join(lines)

    except AttributeError:
        # Older yfinance without Search – fall back to a few well-known patterns
        return ""
    except Exception as e:
        return f"ERROR:{type(e).__name__}: {e}"
