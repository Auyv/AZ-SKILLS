import os
import sys
import time


seconds = int(sys.argv[1]) if len(sys.argv) > 1 else 5
print(f"Python demo started. AZKITS_DEMO={os.getenv('AZKITS_DEMO', '')}", flush=True)
for index in range(1, seconds + 1):
    print(f"Python progress: {index}/{seconds}", flush=True)
    time.sleep(1)
print("Python demo completed.", flush=True)
