#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""OBOS 升级任务状态轮询（本地手动执行 / 可后台运行）

直接调用本机 azkits MCP（Streamable HTTP）的 obos-cli-upgrade-status 工具，
按固定间隔轮询指定升级计划的执行状态，逐轮打印并追加落盘；
进入终态（成功/失败/完成/取消/超时 或 progress>=100）时提前结束。

用法示例：
    python obos_upgrade_watch.py --plan AT_HQT420_0049519998_250
    python obos_upgrade_watch.py --plan AT_HQT420_0049519998_250 --once
    python obos_upgrade_watch.py --plan AT_HQT420_0049519998_250 -i 180 -r 20 --with-online -t 0049519998

参数：
    --plan        升级计划名（必填）
    -i/--interval 轮询间隔秒，默认 180
    -r/--rounds   最多轮询次数，默认 20（0 表示不限次）
    --once        只查一次
    --with-online 每轮附带查询终端 OBOS 在线状态（需 -t）
    -t/--terminal 终端号，配合 --with-online 使用
    --log         日志文件路径，默认 .obos_watch/upgrade_<plan>.log

退出码：0 正常（含进入终态或达到轮次上限）；1 致命错误。
"""

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime

MCP_URL = "http://127.0.0.1:12345/mcp"
TOOL_STATUS = "azkits_obos-cli-upgrade-status"
TOOL_DEVICE = "azkits_obos-cli-device-status"

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STATE_DIR = os.path.join(SCRIPT_DIR, ".obos_watch")

# 终态判定关键词（stateName 命中即认为任务已结束）
# 注意 1：只放明确的结束态词，"完成"不放进来 —— 形如"下载完成，待升级"的中间态会误判为结束。
# 注意 2：不能用 progress>=100 当终态 —— 实测 progress 到 100 时 stateName 仍是"执行中"、
#         failReason 显示 "file checking"，说明只是下载/传输完成、设备还在校验与刷写，任务未结束。
TERMINAL_KEYWORDS = ("成功", "失败", "取消", "超时", "异常")

# 平台已知占位说明（不是失败原因）
KNOWN_PLACEHOLDERS = {
    "download...": "平台占位文案，表示正在下载",
    "file checking": "平台占位文案，表示设备正在校验文件",
}


def call_mcp(tool_name, arguments, timeout=40):
    """调用 azkits MCP 工具，返回解析后的 stdout dict。"""
    payload = {
        "jsonrpc": "2.0",
        "id": int(time.time() * 1000) % 2147483647,
        "method": "tools/call",
        "params": {"name": tool_name, "arguments": arguments},
    }
    req = urllib.request.Request(
        MCP_URL,
        data=json.dumps(payload).encode("utf-8"),
        method="POST",
        headers={"Content-Type": "application/json", "Accept": "application/json, text/event-stream"},
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            raw = r.read().decode("utf-8", "replace")
    except urllib.error.URLError as e:
        return None, "HTTP 失败: %s" % e
    except Exception as e:  # noqa: BLE001
        return None, "请求异常: %r" % e

    lines = [ln[6:] for ln in raw.splitlines() if ln.startswith("data: ")]
    if not lines:
        return None, "响应无 data 段: %s" % raw[:200]
    try:
        result = json.loads(lines[0])["result"]
        text = result["content"][0]["text"]
        outer = json.loads(text)
        if outer.get("state") != "succeeded":
            return None, "脚本未成功: state=%s stderr=%s" % (outer.get("state"), (outer.get("stderr") or "")[:200])
        return json.loads(outer.get("stdout") or "{}"), None
    except Exception as e:  # noqa: BLE001
        return None, "解析失败: %r | raw=%s" % (e, raw[:300])


def query_status(plan, timeout=40):
    """返回 dict {ok, state, state_name, progress, fail_reason, row, error}。"""
    body, err = call_mcp(TOOL_STATUS, {"PLAN_NAME": plan}, timeout=timeout)
    if body is None:
        return {"ok": False, "error": err}
    rows = ((body.get("data") or {}).get("rows")) or []
    if not rows:
        return {"ok": False, "error": "未查到计划 %s 的记录" % plan}
    row = rows[0]
    return {
        "ok": True,
        "state": row.get("state"),
        "state_name": row.get("stateName"),
        "progress": row.get("progress"),
        "fail_reason": row.get("failReason") or "",
        "software": row.get("softwareName"),
        "version": row.get("version"),
        "model": row.get("modelName"),
        "tbox": row.get("tbox"),
        "update_time": row.get("updateTime"),
        "row": row,
        "error": None,
    }


def query_online(terminal, timeout=40):
    body, err = call_mcp(TOOL_DEVICE, {"TERMINAL": terminal}, timeout=timeout)
    if body is None:
        return None, err
    data = body.get("data") or {}
    txt = data.get("online") or (data.get("record") or {}).get("online")
    return (txt == "在线"), txt


def is_terminal(st):
    if not st.get("ok"):
        return False
    name = st.get("state_name") or ""
    return any(k in name for k in TERMINAL_KEYWORDS)


def main():
    ap = argparse.ArgumentParser(description="OBOS 升级任务状态轮询")
    ap.add_argument("--plan", required=True, help="升级计划名")
    ap.add_argument("-i", "--interval", type=int, default=180, help="轮询间隔秒，默认 180")
    ap.add_argument("-r", "--rounds", type=int, default=20, help="最多轮询次数，默认 20；0 表示不限")
    ap.add_argument("--once", action="store_true", help="只查询一次")
    ap.add_argument("--with-online", action="store_true", help="附带查询终端在线状态")
    ap.add_argument("-t", "--terminal", default="", help="终端号（配合 --with-online）")
    ap.add_argument("--timeout", type=int, default=40, help="单次查询超时秒")
    ap.add_argument("--log", default="", help="日志文件路径")
    args = ap.parse_args()

    os.makedirs(STATE_DIR, exist_ok=True)
    log_path = args.log or os.path.join(STATE_DIR, "upgrade_%s.log" % args.plan)
    last_path = os.path.join(STATE_DIR, "upgrade_%s_last.json" % args.plan)
    log_f = open(log_path, "a", encoding="utf-8")

    rounds = 1 if args.once else (args.rounds if args.rounds > 0 else 10 ** 9)

    def emit(line):
        print(line, flush=True)
        log_f.write(line + "\n")
        log_f.flush()

    emit("[%s] 开始轮询计划 %s | 间隔 %ds | 最多 %s 次 | 日志 %s"
         % (datetime.now().strftime("%H:%M:%S"), args.plan, args.interval,
            "1(--once)" if args.once else rounds, log_path))

    for seq in range(1, rounds + 1):
        st = query_status(args.plan, timeout=args.timeout)
        stamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        if not st.get("ok"):
            emit("[%s] #%d 查询失败（不计入状态判定）: %s" % (stamp, seq, st.get("error")))
            last = {"time": stamp, "seq": seq, "ok": False, "error": st.get("error")}
        else:
            extra = ""
            if args.with_online and args.terminal:
                online, online_txt = query_online(args.terminal, timeout=args.timeout)
                extra = " | 终端在线=%s" % (online_txt if online_txt else "未知")
            emit("[%s] #%d state=%s stateName=%s progress=%s fail=%s%s"
                 % (stamp, seq, st.get("state"), st.get("state_name"),
                    st.get("progress"),
                    KNOWN_PLACEHOLDERS.get(st.get("fail_reason") or "", st.get("fail_reason") or "-"),
                    extra))
            last = {
                "time": stamp, "seq": seq, "ok": True,
                "state": st.get("state"), "stateName": st.get("state_name"),
                "progress": st.get("progress"), "failReason": st.get("fail_reason"),
                "row": st.get("row"),
            }

        with open(last_path, "w", encoding="utf-8") as f:
            json.dump(last, f, ensure_ascii=False, indent=2)

        if is_terminal(st):
            emit("[%s] 进入终态：%s（progress=%s），停止轮询。"
                 % (datetime.now().strftime("%H:%M:%S"), st.get("state_name"), st.get("progress")))
            log_f.close()
            return 0

        if seq < rounds:
            try:
                time.sleep(args.interval)
            except KeyboardInterrupt:
                emit("[%s] 已手动停止，累计 %d 次。" % (datetime.now().strftime("%H:%M:%S"), seq))
                log_f.close()
                return 0

    emit("[%s] 已达轮询次数上限，停止轮询（任务可能仍在进行，未进入终态）。"
         % datetime.now().strftime("%H:%M:%S"))
    log_f.close()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n已手动停止")
        sys.exit(0)
