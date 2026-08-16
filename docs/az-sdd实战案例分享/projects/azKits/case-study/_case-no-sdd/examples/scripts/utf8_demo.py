import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

print("你好，这是 UTF-8 输出演示！")
print("AZKITS_UTF8_OK")
