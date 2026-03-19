"""
ExtraGraphAnalyst - Data Loader
Loads CSV / Excel files containing OHLCV financial data.
Returns a pipe-delimited flat string for efficient C++ parsing.
"""
import os
import sys


def _try_import_pandas():
    try:
        import pandas as pd
        return pd
    except ImportError:
        return None


def _load_with_csv_module(filepath):
    """Fallback loader using stdlib csv module."""
    import csv
    rows = []
    with open(filepath, newline='', encoding='utf-8-sig') as f:
        reader = csv.reader(f)
        headers = None
        for row in reader:
            if headers is None:
                headers = [h.lower().strip() for h in row]
                continue
            rows.append(row)
    return headers, rows


def _find_col(headers, candidates):
    for c in candidates:
        if c in headers:
            return headers.index(c)
    return -1


def load_file_flat(filepath):
    """
    Load a CSV or Excel file.
    Returns a flat string: one line per candle, format:
        label|open|high|low|close|volume
    On error returns a string starting with 'ERROR:'
    """
    if not os.path.isfile(filepath):
        return f"ERROR:File not found: {filepath}"

    ext = os.path.splitext(filepath)[1].lower()
    pd  = _try_import_pandas()

    try:
        if pd is not None:
            if ext in ('.xlsx', '.xls'):
                df = pd.read_excel(filepath)
            else:
                # Try multiple common separators
                for sep in (',', ';', '\t', '|'):
                    try:
                        df = pd.read_csv(filepath, sep=sep)
                        if len(df.columns) >= 4:
                            break
                    except Exception:
                        continue
                else:
                    return "ERROR:Could not parse CSV with common separators"

            df.columns = [str(c).lower().strip() for c in df.columns]

            date_col  = _find_col(list(df.columns), ['date','time','datetime','timestamp','Date','Time','DateTime'])
            open_col  = _find_col(list(df.columns), ['open','o','Open','OPEN'])
            high_col  = _find_col(list(df.columns), ['high','h','High','HIGH'])
            low_col   = _find_col(list(df.columns), ['low','l','Low','LOW'])
            close_col = _find_col(list(df.columns), ['close','c','price','Close','CLOSE','Price','last'])
            vol_col   = _find_col(list(df.columns), ['volume','vol','v','Volume','VOL','VOLUME'])

            # Try index-based fallback (date=0, open=1, high=2, low=3, close=4, vol=5)
            cols = list(df.columns)
            if open_col < 0 and len(cols) >= 5:
                open_col  = 1
                high_col  = 2
                low_col   = 3
                close_col = 4
                if date_col < 0: date_col = 0
                if vol_col  < 0 and len(cols) >= 6: vol_col = 5

            if open_col < 0 or high_col < 0 or low_col < 0 or close_col < 0:
                return f"ERROR:Could not identify OHLC columns. Found: {list(df.columns)}"

            lines = []
            for idx, row in df.iterrows():
                try:
                    o = float(row.iloc[open_col])
                    h = float(row.iloc[high_col])
                    l = float(row.iloc[low_col])
                    c = float(row.iloc[close_col])
                    v = float(row.iloc[vol_col]) if vol_col >= 0 else 0.0
                    lbl = str(row.iloc[date_col]) if date_col >= 0 else str(idx)
                    # Trim label
                    lbl = lbl.replace('|', '-').strip()
                    if lbl == 'nan': lbl = str(idx)
                    lines.append(f"{lbl}|{o}|{h}|{l}|{c}|{v}")
                except (ValueError, TypeError):
                    continue
            if not lines:
                return "ERROR:No valid candles found in file"
            return '\n'.join(lines)

        else:
            # Fallback: stdlib csv
            if ext in ('.xlsx', '.xls'):
                return "ERROR:pandas required for Excel files. Run: pip install pandas openpyxl"
            headers, rows = _load_with_csv_module(filepath)
            if not headers:
                return "ERROR:Empty file"

            date_col  = _find_col(headers, ['date','time','datetime','timestamp'])
            open_col  = _find_col(headers, ['open','o'])
            high_col  = _find_col(headers, ['high','h'])
            low_col   = _find_col(headers, ['low','l'])
            close_col = _find_col(headers, ['close','c','price','last'])
            vol_col   = _find_col(headers, ['volume','vol','v'])

            if open_col < 0 and len(headers) >= 5:
                open_col = 1; high_col = 2; low_col = 3; close_col = 4
                if date_col < 0: date_col = 0
                if vol_col  < 0 and len(headers) >= 6: vol_col = 5

            if open_col < 0 or high_col < 0 or low_col < 0 or close_col < 0:
                return f"ERROR:Could not identify OHLC columns. Headers: {headers}"

            lines = []
            for i, row in enumerate(rows):
                try:
                    o = float(row[open_col])
                    h = float(row[high_col])
                    l = float(row[low_col])
                    c = float(row[close_col])
                    v = float(row[vol_col]) if vol_col >= 0 and vol_col < len(row) else 0.0
                    lbl = row[date_col].strip().replace('|', '-') if date_col >= 0 and date_col < len(row) else str(i)
                    lines.append(f"{lbl}|{o}|{h}|{l}|{c}|{v}")
                except (ValueError, IndexError):
                    continue
            if not lines:
                return "ERROR:No valid candles found"
            return '\n'.join(lines)

    except Exception as e:
        return f"ERROR:{type(e).__name__}: {e}"
