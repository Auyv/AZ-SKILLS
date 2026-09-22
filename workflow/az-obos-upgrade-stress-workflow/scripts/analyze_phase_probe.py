#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""阶段探针结果分析：验证 progress 二值性、还原阶段时间线、量化采样命中率。

用法：
    python analyze_phase_probe.py --jsonl .obos_watch/phase_probe_0049519998_xxx.jsonl
    python analyze_phase_probe.py --latest --terminal 0049519998
"""
import argparse
import glob
import io
import json
import os
from collections import Counter, OrderedDict

WS_ROOT = os.environ.get("HQ_OBOS_WS") or os.path.dirname(os.path.abspath(__file__))
STATE_DIR = os.path.join(WS_ROOT, ".obos_watch")


def load(jsonl):
    evs = []
    with io.open(jsonl, encoding="utf-8") as f:
        for l in f:
            l = l.strip()
            if l:
                try:
                    evs.append(json.loads(l))
                except Exception:
                    pass
    return evs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jsonl", default="")
    ap.add_argument("--latest", action="store_true")
    ap.add_argument("--terminal", default="")
    args = ap.parse_args()

    p = args.jsonl
    if args.latest or not p:
        pat = os.path.join(STATE_DIR, "phase_probe_%s*.jsonl" % (args.terminal or "*"))
        cands = sorted(glob.glob(pat), key=os.path.getmtime)
        if not cands:
            print("未找到探针数据: %s" % pat)
            return 1
        p = cands[-1]
    print("数据文件: %s" % p)
    evs = load(p)
    samples = [e for e in evs if e.get("event") == "sample"]
    print("事件 %d 条，有效采样 %d 条" % (len(evs), len(samples)))
    if not samples:
        print("无有效采样，无法分析。")
        return 1

    end = next((e for e in evs if e.get("event") == "probe_end"), None)
    if end:
        print("终态: %s progress=%s 总耗时 %.1fs 采样 %d 次" % (
            end.get("state_name"), end.get("progress"), end.get("elapsed_s"), end.get("samples")))
    print()

    # ---- 1. progress 取值分布 ----
    print("=== 1. progress 取值分布（验证是否二值）===")
    c = Counter(s.get("progress") for s in samples)
    for k, v in sorted(c.items(), key=lambda x: (x[0] is None, x[0])):
        print("  progress=%-6s %4d 次  (%.1f%%)" % (k, v, 100.0 * v / len(samples)))
    distinct = [k for k in c if k is not None]
    if len(samples) < 20:
        verdict = "【样本 %d 条偏少，不足以判定】" % len(samples)
    elif len(distinct) <= 2:
        verdict = "【确认二值：%d 条采样中无任何中间值】" % len(samples)
    else:
        verdict = "【存在中间值！旧结论需修正】"
    print("  -> 不同取值数 = %d %s" % (len(distinct), verdict))
    print()

    # ---- 2. failReason 分布 ----
    print("=== 2. failReason 取值分布 ===")
    c2 = Counter((s.get("fail_reason") or "").strip() for s in samples)
    for k, v in c2.most_common():
        print("  %-40r %4d 次" % (k, v))
    print()

    # ---- 3. 阶段时间线 ----
    print("=== 3. 阶段时间线（原始采样归并，间隔 %.1fs）===" % (
        samples[1]["elapsed_s"] - samples[0]["elapsed_s"] if len(samples) > 1 else 5.0))
    segs = []
    cur = samples[0].get("phase_key")
    start = samples[0].get("elapsed_s")
    start_ts = samples[0].get("ts")
    for s in samples[1:]:
        k = s.get("phase_key")
        if k != cur:
            segs.append((cur, start, s.get("elapsed_s"), start_ts, s.get("ts")))
            cur = k
            start = s.get("elapsed_s")
            start_ts = s.get("ts")
    last = samples[-1]
    segs.append((cur, start, last.get("elapsed_s"), start_ts, last.get("ts")))
    for k, a, b, ta, tb in segs:
        print("  %-18s %7.1fs -> %7.1fs  持续 %7.1fs   (%s ~ %s)" % (
            k, a, b, (b - a), (ta or "")[11:19], (tb or "")[11:19]))
    print()

    # ---- 4. 与 20s 轮询的命中率对比 ----
    print("=== 4. 采样间隔 vs 阶段窗口命中率（从 5s 序列下采样模拟）===")
    base = float(samples[0].get("elapsed_s") or 0)
    for n in (1, 2, 3, 4, 6, 8, 12):
        step = samples[n::n]
        keys = Counter(s.get("phase_key") for s in step)
        dl = sum(v for k, v in keys.items() if k in ("download_begin", "downloading"))
        fc = keys.get("filecheck", 0)
        print("  等效间隔 %5.1fs -> 采样 %4d 次 | 传输窗口命中 %3d 次 | filecheck 命中 %4d 次" % (
            (samples[1]["elapsed_s"] - samples[0]["elapsed_s"]) * n if len(samples) > 1 else 5.0 * n,
            len(step), dl, fc))
    print()

    # ---- 5. query 成本 ----
    costs = [s.get("query_cost_s") for s in samples if s.get("query_cost_s") is not None]
    if costs:
        cs = sorted(costs)
        print("=== 5. 单次状态查询耗时 ===")
        print("  min=%.2fs 中位=%.2fs max=%.2fs 均值=%.2fs 合计=%.1fs（占轮次 %.1f%%）" % (
            cs[0], cs[len(cs) // 2], cs[-1], sum(cs) / len(cs), sum(cs),
            100.0 * sum(cs) / (samples[-1].get("elapsed_s") or 1)))
    fails = [e for e in evs if e.get("event") in ("sample_fail",)]
    if fails:
        print()
        print("=== 6. 查询失败 %d 次（升级重启导致的正常瞬断）===" % len(fails))
        for e in fails[:10]:
            print("  %7.1fs  %s" % (e.get("elapsed_s"), str(e.get("error"))[:90]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
