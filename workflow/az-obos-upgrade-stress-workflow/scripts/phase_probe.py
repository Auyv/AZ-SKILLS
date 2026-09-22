#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""升级阶段探针（只读监控 + 单轮升级，不动继电器）

目的：以 ≤5s 间隔连续采样一轮完整升级的 state / stateName / progress / failReason，
回答三个问题：
  1. progress 是否真的只有 0/100 两个值（会不会是 20s 粒度漏采的假象）
  2. failReason 在 file checking 内部是否恒定（能否分出校验/刷写边界）
  3. 文件传输窗口的精确时长与抖动范围

安全边界：全程只调用 状态查询 / 在线查询；只在下发那一刻调用 submit+start 一次，
绝不触碰继电器。不做任何上下电。
"""
import argparse
import io
import json
import os
import sys
import time
from datetime import datetime

# 工作区根目录：优先环境变量 HQ_OBOS_WS，否则取脚本所在目录。
# 依赖 obos_upgrade_watch.py / obos_upgrade_stress.py 与本脚本同目录。
WS_ROOT = os.environ.get("HQ_OBOS_WS") or os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, WS_ROOT)

from obos_upgrade_watch import query_status, query_online  # noqa: E402
from obos_upgrade_stress import submit_plan, start_plan  # noqa: E402

STATE_DIR = os.path.join(WS_ROOT, ".obos_watch")
TERMINAL_KEYWORDS = ("成功", "失败", "取消", "超时", "异常")


def now_str(ms=True):
    return (datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            if ms else datetime.now().strftime("%Y-%m-%d %H:%M:%S"))


class Sink:
    def __init__(self, log_path, jsonl_path):
        self.f = io.open(log_path, "a", encoding="utf-8")
        self.j = io.open(jsonl_path, "a", encoding="utf-8")

    def log(self, s):
        line = "[%s] %s" % (datetime.now().strftime("%H:%M:%S"), s)
        print(line, flush=True)
        self.f.write(line + "\n")
        self.f.flush()

    def ev(self, **kw):
        kw["ts"] = now_str()
        self.j.write(json.dumps(kw, ensure_ascii=False) + "\n")
        self.j.flush()


def classify(st):
    """把一次状态采样归一成 (阶段key, 阶段中文)。"""
    if not st.get("ok"):
        return "query_fail", "查询失败"
    state = str(st.get("state") or "")
    name = st.get("state_name") or ""
    fr = (st.get("fail_reason") or "").strip()
    prog = st.get("progress")
    if "成功" in name:
        return "success", "成功"
    if any(k in name for k in ("失败", "取消", "超时", "异常")):
        return "failed", "失败"
    if state == "14" or "未下发" in name:
        return "pending", "未下发"
    if state == "15" or "挂起" in name:
        return "hangup", "挂起/已派发"
    if state == "2":
        if fr in ("begin download", "download..."):
            return "download_begin" if fr == "begin download" else "downloading", \
                   "文件传输中(%s)" % fr
        if fr == "file checking":
            return "filecheck", "校验刷写中"
        if fr == "":
            return "exec_other", "执行中(无表述)"
        return "exec_other", "执行中(%s)" % fr
    return "unknown", "状态未知"


def main():
    ap = argparse.ArgumentParser(description="OBOS 升级阶段探针（不动继电器）")
    ap.add_argument("-t", "--terminal", default="0049519998", help="终端号")
    ap.add_argument("--product", default="HQT420-S01-01")
    ap.add_argument("--firmware", default="261.img")
    ap.add_argument("--version", default="261")
    ap.add_argument("--mode", default="HTTP")
    ap.add_argument("--plan", default="", help="计划名，默认自动生成")
    ap.add_argument("-i", "--interval", type=float, default=5.0, help="轮询间隔秒(默认5)")
    ap.add_argument("--max-wait", type=int, default=3600, help="最长等待秒(默认3600)")
    ap.add_argument("--no-submit", action="store_true",
                    help="跳过下发，仅监控已存在的计划")
    args = ap.parse_args()

    plan = args.plan or ("PROBE_PHASE_%s_R1" % args.terminal)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = os.path.join(STATE_DIR, "phase_probe_%s_%s.log" % (args.terminal, stamp))
    jsonl_path = os.path.join(STATE_DIR, "phase_probe_%s_%s.jsonl" % (args.terminal, stamp))
    sink = Sink(log_path, jsonl_path)

    sink.log("=== 阶段探针启动（只读，不动继电器）===")
    sink.log("终端=%s 计划=%s 固件=%s/%s 轮询间隔=%.1fs" % (
        args.terminal, plan, args.firmware, args.version, args.interval))
    sink.log("落盘: %s" % jsonl_path)

    online, online_txt = query_online(args.terminal)
    sink.log("设备在线状态: %s" % (online_txt or "未知"))
    sink.ev(event="probe_start", terminal=args.terminal, plan=plan,
            interval_s=args.interval, firmware=args.firmware, version=args.version,
            online=online_txt)

    if not args.no_submit:
        ok, err = submit_plan(plan, args.firmware, args.version, args.mode,
                              args.product, args.terminal)
        if not ok:
            sink.log("!!! 提交失败: %s" % err)
            sink.ev(event="submit_fail", terminal=args.terminal, plan=plan, error=str(err))
            return 2
        sink.log("提交成功: %s" % plan)
        sink.ev(event="submit_ok", terminal=args.terminal, plan=plan)

        ok2, err2 = start_plan(plan)
        if not ok2:
            sink.log("!!! 下发失败: %s" % err2)
            sink.ev(event="start_fail", terminal=args.terminal, plan=plan, error=str(err2))
            return 3
        sink.log("下发成功，开始高频采样")
        sink.ev(event="start_ok", terminal=args.terminal, plan=plan)

    t0 = time.time()
    seq = 0
    prev_key = None
    transitions = []
    seq_list = []
    while True:
        el = time.time() - t0
        if el > args.max_wait:
            sink.log("达到最长等待 %ds，停止。" % args.max_wait)
            sink.ev(event="probe_timeout", terminal=args.terminal, elapsed_s=round(el, 1))
            break
        seq += 1
        t_a = time.time()
        st = query_status(plan)
        t_b = time.time()
        key, cn = classify(st)
        if st.get("ok"):
            sink.ev(event="sample", seq=seq, terminal=args.terminal, plan=plan,
                    elapsed_s=round(el, 1), state=st.get("state"),
                    state_name=st.get("state_name"), progress=st.get("progress"),
                    fail_reason=st.get("fail_reason"), phase_key=key, phase_cn=cn,
                    query_cost_s=round(t_b - t_a, 2))
            seq_list.append((round(el, 1), key))
            if key != prev_key:
                transitions.append({"elapsed_s": round(el, 1), "from": prev_key,
                                    "to": key, "cn": cn})
                sink.log("  %7.1fs  state=%-3s %-6s prog=%-5s failReason=%-34r -> %s" % (
                    el, st.get("state"), st.get("state_name"), st.get("progress"),
                    st.get("fail_reason"), cn))
                sink.ev(event="phase_transition", terminal=args.terminal,
                        elapsed_s=round(el, 1), frm=prev_key, to=key, cn=cn)
                prev_key = key
        else:
            sink.ev(event="sample_fail", seq=seq, terminal=args.terminal, plan=plan,
                    elapsed_s=round(el, 1), error=str(st.get("error"))[:200],
                    query_cost_s=round(t_b - t_a, 2))

        if st.get("ok") and ("成功" in (st.get("state_name") or "")
                             or any(k in (st.get("state_name") or "") for k in TERMINAL_KEYWORDS)):
            sink.log("进入终态: %s progress=%s（总耗时 %.1fs，采样 %d 次）" % (
                st.get("state_name"), st.get("progress"), el, seq))
            sink.ev(event="probe_end", terminal=args.terminal, elapsed_s=round(el, 1),
                    samples=seq, state=st.get("state"), state_name=st.get("state_name"),
                    progress=st.get("progress"), fail_reason=st.get("fail_reason"))
            break

        sleep_for = args.interval - (time.time() - t_b)
        if sleep_for > 0:
            time.sleep(sleep_for)

    # ---- 汇总各阶段时长 ----
    sink.log("")
    sink.log("=== 阶段时间线（按采样序列归并）===")
    if seq_list:
        segs = []
        cur = seq_list[0][1]
        start = seq_list[0][0]
        for t, k in seq_list[1:]:
            if k != cur:
                segs.append((cur, start, t, round(t - start, 1)))
                cur = k
                start = t
        last_t = seq_list[-1][0]
        segs.append((cur, start, last_t, round(last_t - start, 1)))
        for k, a, b, d in segs:
            sink.log("  %-16s %7.1fs -> %7.1fs   持续 %7.1fs" % (k, a, b, d))

    # ---- 关键问答结论 ----
    sink.log("")
    sink.log("=== 关键验证 ===")
    sink.log("  采样总数: %d" % seq)
    sink.log("  阶段跃迁次数: %d" % len(transitions))
    sink.log("  原始数据: %s" % jsonl_path)
    sink.log("  可执行: python -c \"import json,io; [print(json.loads(l).get('elapsed_s'), "
             "json.loads(l).get('progress'), json.loads(l).get('fail_reason')) "
             "for l in io.open(r'%s',encoding='utf-8') "
             "if json.loads(l).get('event')=='sample']\"" % jsonl_path)
    sink.f.close()
    sink.j.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
