import pandas as pd
import numpy as np
from datetime import datetime
import os

def load_csv(path: str) -> list:
    """
    Load OHLCV data from CSV. Auto-detects column names.
    Returns list of dicts: {timestamp, date_str, open, high, low, close, volume}
    """
    df = pd.read_csv(path)
    df.columns = [c.strip().lower() for c in df.columns]

    date_col = next((c for c in df.columns if c in
                     ['date','datetime','timestamp','time','period']), df.columns[0])

    def find_col(candidates):
        for c in candidates:
            if c in df.columns: return c
        return None

    open_col   = find_col(['open', 'o', 'open_price'])
    high_col   = find_col(['high', 'h', 'high_price'])
    low_col    = find_col(['low', 'l', 'low_price'])
    close_col  = find_col(['close', 'c', 'close_price', 'last', 'price'])
    volume_col = find_col(['volume', 'v', 'vol', 'quantity'])

    date_formats = [
        '%Y-%m-%d', '%d/%m/%Y', '%m/%d/%Y',
        '%Y-%m-%d %H:%M:%S', '%d/%m/%Y %H:%M',
        '%Y%m%d', '%d-%m-%Y', '%b %d, %Y',
        '%Y-%m-%dT%H:%M:%S'
    ]

    parsed_dates = None
    for fmt in date_formats:
        try:
            parsed_dates = pd.to_datetime(df[date_col], format=fmt)
            break
        except:
            continue
    if parsed_dates is None:
        parsed_dates = pd.to_datetime(df[date_col], infer_datetime_format=True)

    df['_ts'] = parsed_dates
    df = df.sort_values('_ts').reset_index(drop=True)

    result = []
    for _, row in df.iterrows():
        bar = {
            'timestamp': int(row['_ts'].timestamp()),
            'date_str': row['_ts'].strftime('%Y-%m-%d %H:%M'),
            'open':   float(row[open_col])   if open_col  and pd.notna(row[open_col])  else float(row[close_col]),
            'high':   float(row[high_col])   if high_col  and pd.notna(row[high_col])  else float(row[close_col]),
            'low':    float(row[low_col])    if low_col   and pd.notna(row[low_col])   else float(row[close_col]),
            'close':  float(row[close_col])  if pd.notna(row[close_col]) else 0.0,
            'volume': float(row[volume_col]) if volume_col and pd.notna(row[volume_col]) else 0.0,
        }
        result.append(bar)
    return result
