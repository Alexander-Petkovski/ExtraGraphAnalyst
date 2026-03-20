"""
ExtraGraphAnalyst Python Console Environment
Available variables: df, chart, indicators, predictors
"""
import numpy as np
import pandas as pd

def help_ega():
    print("ExtraGraphAnalyst Console Help")
    print("=" * 40)
    print("Variables:")
    print("  df           : pandas DataFrame (Date,Open,High,Low,Close,Volume)")
    print("  indicators   : module with calc_sma(), calc_ema(), calc_rsi(), etc.")
    print("  predictors   : module with predict_holt_winters(), etc.")
    print()
    print("Examples:")
    print("  df.describe()")
    print("  df['Close'].plot()")
    print("  indicators.calc_sma(df['Close'].tolist(), 20)")
    print("  predictors.predict_holt_winters(df['Close'].tolist(), bars_ahead=30)")
    print()
    print("Type help_ega() to see this again.")

print("ExtraGraphAnalyst Python Console ready.")
print("Type help_ega() for available commands.\n")
