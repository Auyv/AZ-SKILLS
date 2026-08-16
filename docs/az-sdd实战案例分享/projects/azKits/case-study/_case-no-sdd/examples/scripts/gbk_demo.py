import sys

text = "你好，这是 GBK 编码输出演示！"
sys.stdout.buffer.write(text.encode("gbk") + b"\n")
sys.stdout.buffer.write("AZKITS_GBK_OK".encode("gbk") + b"\n")
sys.stdout.flush()
