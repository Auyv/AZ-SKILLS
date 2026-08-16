import os
import sys

count = 5
if len(sys.argv) > 1:
    count = int(sys.argv[1])
print("Python 示例, env AZKITS_DEMO =", os.environ.get("AZKITS_DEMO", "(未设置)"))
for i in range(count):
    print(f"进度 {i + 1}/{count}")
print("Python 完成")
