#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""OBOS 升级压测脚本（循环 N 次）

每轮流程：确认终端在线 -> 提交升级计划 -> 下发 -> 轮询到终态 -> 等待设备重启回线 -> 冷却 -> 下一轮。
全程落盘：控制台日志、Markdown 汇总报告、CSV 明细（便于统计成功率/耗时分布）。

复用已登记的固件（默认不重复上传）。建议后台运行：
    python obos_upgrade_stress.py -t 0049519998 --rounds 10 --poll-interval 30

常用参数：
    -t/--terminal        终端号（必填）
    --product            产品型号，默认 HQT420-S01-01
    --firmware           固件名，默认 250.img
    --version            固件版本，默认 250
    --mode               升级方式，默认 HTTP
    --rounds             循环次数，默认 10
    --poll-interval      单轮内状态轮询间隔秒，默认 30
    --round-timeout      单轮最长等待秒，默认 1200
    --online-timeout     等待设备在线的最长秒数，默认 600
    --cooldown           每轮之间的冷却秒数，默认 30
    --max-consecutive-fail  连续失败几次中止，默认 3
    --plan-prefix        计划名前缀，默认 AT_Stress_<终端>_<版本>_<时间戳>
    --alt-firmware/--alt-version  可选的第二个包，交替使用（A/B 交替升级时用）
    --dry-run            只打印计划，不实际操作
    --no-wait-reboot     成功后不等设备回线，直接冷却进入下一轮（不推荐）

退出码：0 全部成功；2 中途因连续失败中止；1 参数/致命错误。
"""

import argparse
import csv
import json
import os
import sys
import time
from datetime import datetime

# 复用同目录脚本里的 MCP 调用与终态判定（模块内 main 有 __main__ 保护，可安全 import）
from obos_upgrade_watch import (  # noqa: E402
    call_mcp,
    query_status,
    query_online,
    is_terminal,
    KNOWN_PLACEHOLDERS,
)

TOOL_SUBMIT = "azkits_obos-cli-upgrade-submit"
TOOL_START = "azkits_obos-cli-upgrade-start"

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STATE_DIR = os.path.join(SCRIPT_DIR, ".obos_watch")


def submit_plan(plan, firmware, version, mode, product, terminal, timeout=40):
    body, err = call_mcp(
        TOOL_SUBMIT,
        {
            "PLAN_NAME": plan,
            "FIRMWARE": firmware,
            "FIRMWARE_VERSION": version,
            "MODE": mode,
            "PRODUCT_MODEL": product,
            "TERMINAL": terminal,
        },
        timeout=timeout,
    )
    if body is None:
        return False, err
    if not body.get("ok"):
        return False, "接口返回 ok=false: %s" % json.dumps(body, ensure_ascii=False)[:200]
    # 响应里 created 字段恒为 null（已知占位），改用状态查询复核计划是否真的建出来
    st = query_status(plan, timeout=timeout)
    if not st.get("ok"):
        return False, "计划已提交但查询不到记录: %s" % st.get("error")
    return True, None


def start_plan(plan, timeout=40):
    body, err = call_mcp(TOOL_START, {"PLAN_NAME": plan}, timeout=timeout)
    if body is None:
        return False, err
    data = body.get("data") or {}
    if not body.get("ok"):
        return False, "接口返回 ok=false: %s" % json.dumps(body, ensure_ascii=False)[:200]
    if not (data.get("started") or data.get("result")):
        return False, "下发未生效: %s" % json.dumps(data, ensure_ascii=False)[:200]
    return True, None


def wait_until_online(terminal, timeout, interval, log):
    """等待终端重新在线，返回 (ok, 耗时秒, 最近一次状态文案)。"""
    deadline = time.time() + timeout
    t0 = time.time()
    last_txt = None
    while True:
        online, txt = query_online(terminal)
        if txt is not None:
            last_txt = txt
        if online:
            return True, int(time.time() - t0), txt
        if time.time() >= deadline:
            return False, int(time.time() - t0), last_txt
        log("      设备当前 %s，等待回线（剩余 %ds）" % (txt or "未知", int(deadline - time.time())))
        time.sleep(interval)


def run_round(idx, args, log):
    """执行一轮升级，返回 dict 结果。"""
    stamp = datetime.now()
    firmware = args.firmware
    version = args.version
    if args.alt_firmware and args.alt_version and (idx % 2 == 0):
        firmware, version = args.alt_firmware, args.alt_version

    plan = "%s_R%d" % (args.plan_prefix, idx)
    res = {
        "round": idx,
        "plan": plan,
        "firmware": firmware,
        "version": version,
        "start_time": stamp.strftime("%Y-%m-%d %H:%M:%S"),
        "state": None,
        "state_name": None,
        "progress": None,
        "fail_reason": "",
        "duration_s": None,
        "reboot_recover_s": None,
        "result": "未执行",
        "note": "",
    }
    log("[%s] ===== 第 %d/%d 轮 | 终端 %s | %s(%s) | 计划 %s =====" %
        (stamp.strftime("%H:%M:%S"), idx, args.rounds, args.terminal, firmware, version, plan))

    if args.dry_run:
        res["result"] = "DRY-RUN"
        res["note"] = "仅演练，未实际操作"
        log("      (dry-run) 跳过实际操作")
        return res

    # 1) 确认在线
    online, txt = query_online(args.terminal)
    log("      在线状态: %s" % (txt or "未知"))
    if not online:
        ok, cost, txt = wait_until_online(args.terminal, args.online_timeout, args.poll_interval, log)
        if not ok:
            res["result"] = "失败"
            res["note"] = "开轮前设备离线且等待 %ds 未回线" % args.online_timeout
            log("      %s" % res["note"])
            return res
        log("      设备已回线，等待 %ds" % cost)

    # 2) 提交计划
    ok, err = submit_plan(plan, firmware, version, args.mode, args.product, args.terminal)
    if not ok:
        res["result"] = "失败"
        res["note"] = "提交计划失败: %s" % err
        log("      %s" % res["note"])
        return res
    log("      计划已提交")

    # 3) 下发
    ok, err = start_plan(plan)
    if not ok:
        res["result"] = "失败"
        res["note"] = "下发失败: %s" % err
        log("      %s" % res["note"])
        return res
    log("      已下发")

    # 4) 轮询到终态
    t0 = time.time()
    deadline = t0 + args.round_timeout
    last = None
    while True:
        st = query_status(plan)
        if st.get("ok"):
            last = st
            log("      [%s] state=%s %s progress=%s %s" % (
                datetime.now().strftime("%H:%M:%S"), st.get("state"), st.get("state_name"),
                st.get("progress"),
                KNOWN_PLACEHOLDERS.get(st.get("fail_reason") or "", st.get("fail_reason") or "-")))
            if is_terminal(st):
                break
        else:
            log("      [%s] 查询失败(继续重试): %s" % (datetime.now().strftime("%H:%M:%S"), st.get("error")))
        if time.time() >= deadline:
            res["result"] = "超时"
            res["note"] = "单轮 %ds 内未进入终态" % args.round_timeout
            break
        time.sleep(args.poll_interval)

    if last:
        res.update({
            "state": last.get("state"),
            "state_name": last.get("state_name"),
            "progress": last.get("progress"),
            "fail_reason": last.get("fail_reason") or "",
            "duration_s": int(time.time() - t0),
        })
    if res["result"] == "未执行":
        name = (last or {}).get("state_name") or ""
        res["result"] = "成功" if "成功" in name else "失败"
        if res["result"] == "成功":
            if res["fail_reason"] and res["fail_reason"] not in KNOWN_PLACEHOLDERS:
                res["note"] = "成功，回执: %s" % res["fail_reason"]
        else:
            res["note"] = "终态判为失败: %s / %s" % (name, res["fail_reason"])

    log("      >>> 第 %d 轮结果: %s（耗时 %ss）%s" %
        (idx, res["result"], res["duration_s"], (" | " + res["note"]) if res["note"] else ""))

    # 5) 等设备重启回线
    if res["result"] == "成功" and not args.no_wait_reboot:
        ok, cost, txt = wait_until_online(args.terminal, args.online_timeout, args.poll_interval, log)
        res["reboot_recover_s"] = cost
        if ok:
            log("      设备已回线（%ds）" % cost)
        else:
            res["note"] = (res["note"] + " | " if res["note"] else "") + \
                          "升级成功但 %ds 内未回线（可能仍在重启或掉线）" % args.online_timeout
            log("      %s" % res["note"])
    return res


def write_reports(prefix, rows, args, log_path):
    os.makedirs(STATE_DIR, exist_ok=True)
    md_path = os.path.join(STATE_DIR, "stress_%s.md" % prefix)
    csv_path = os.path.join(STATE_DIR, "stress_%s.csv" % prefix)

    ok_cnt = sum(1 for r in rows if r["result"] == "成功")
    fail_cnt = sum(1 for r in rows if r["result"] == "失败")
    timeout_cnt = sum(1 for r in rows if r["result"] == "超时")
    durs = [r["duration_s"] for r in rows if r.get("duration_s")]

    lines = [
        "# OBOS 升级压测报告",
        "",
        "- 终端: %s" % args.terminal,
        "- 产品/固件/版本/方式: %s / %s / %s / %s" % (args.product, args.firmware, args.version, args.mode),
        "- 计划轮次: %d，实际执行: %d" % (args.rounds, len(rows)),
        "- 成功 %d / 失败 %d / 超时 %d" % (ok_cnt, fail_cnt, timeout_cnt),
        "- 单轮耗时: min=%ss max=%ss avg=%ss" % (
            min(durs) if durs else "-", max(durs) if durs else "-",
            int(sum(durs) / len(durs)) if durs else "-"),
        "- 日志: `%s`" % log_path,
        "- 生成时间: %s" % datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "",
        "| 轮次 | 开始时间 | 计划名 | 结果 | state | stateName | progress | 耗时s | 回线s | 备注 |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for r in rows:
        lines.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["round"], r["start_time"], r["plan"], r["result"], r["state"], r["state_name"],
            r["progress"], r["duration_s"], r["reboot_recover_s"],
            (r.get("note") or "").replace("|", "/")))

    with open(md_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    with open(csv_path, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(["round", "start_time", "plan", "result", "state", "state_name", "progress",
                    "duration_s", "reboot_recover_s", "fail_reason", "note"])
        for r in rows:
            w.writerow([r["round"], r["start_time"], r["plan"], r["result"], r["state"], r["state_name"],
                        r["progress"], r["duration_s"], r["reboot_recover_s"], r.get("fail_reason", ""),
                        r.get("note", "")])
    return md_path, csv_path


def main():
    ap = argparse.ArgumentParser(description="OBOS 升级压测（循环 N 次）")
    ap.add_argument("-t", "--terminal", required=True, help="终端号")
    ap.add_argument("--product", default="HQT420-S01-01", help="产品型号，默认 HQT420-S01-01")
    ap.add_argument("--firmware", default="250.img", help="固件名，默认 250.img")
    ap.add_argument("--version", default="250", help="固件版本，默认 250")
    ap.add_argument("--mode", default="HTTP", help="升级方式，默认 HTTP")
    ap.add_argument("--alt-firmware", default="", help="可选：交替使用的第二个固件名")
    ap.add_argument("--alt-version", default="", help="可选：交替使用的第二个版本号")
    ap.add_argument("--rounds", type=int, default=10, help="循环次数，默认 10")
    ap.add_argument("--poll-interval", type=int, default=30, help="状态轮询间隔秒，默认 30")
    ap.add_argument("--round-timeout", type=int, default=1200, help="单轮超时秒，默认 1200")
    ap.add_argument("--online-timeout", type=int, default=600, help="等待在线超时秒，默认 600")
    ap.add_argument("--cooldown", type=int, default=30, help="轮间冷却秒，默认 30")
    ap.add_argument("--max-consecutive-fail", type=int, default=3, help="连续失败几次中止，默认 3")
    ap.add_argument("--plan-prefix", default="", help="计划名前缀，默认自动生成")
    ap.add_argument("--dry-run", action="store_true", help="只演练不实际操作")
    ap.add_argument("--no-wait-reboot", action="store_true", help="成功后不等设备回线")
    ap.add_argument("--timeout", type=int, default=40, help="单次 MCP 查询超时秒")
    args = ap.parse_args()

    if not args.plan_prefix:
        args.plan_prefix = "AT_Stress_%s_%s_%s" % (
            args.terminal, args.version, datetime.now().strftime("%Y%m%d_%H%M%S"))

    os.makedirs(STATE_DIR, exist_ok=True)
    log_path = os.path.join(STATE_DIR, "stress_%s.log" % args.plan_prefix)
    log_f = open(log_path, "a", encoding="utf-8")

    def log(line):
        print(line, flush=True)
        log_f.write(line + "\n")
        log_f.flush()

    log("[%s] 压测开始 | 终端 %s | 轮次 %d | 产品 %s | 固件 %s(%s) | %s"
        % (datetime.now().strftime("%H:%M:%S"), args.terminal, args.rounds,
           args.product, args.firmware, args.version, args.mode))
    log("计划名前缀: %s | 日志: %s" % (args.plan_prefix, log_path))
    if args.dry_run:
        log("*** DRY-RUN：不会提交/下发任何升级任务 ***")

    rows = []
    consecutive_fail = 0
    aborted = False
    interrupted = False
    try:
        for i in range(1, args.rounds + 1):
            r = run_round(i, args, log)
            rows.append(r)
            # 每轮结束即刷新报告：中途被 Ctrl+C / 强杀中断时，已跑的轮次仍有据可查
            write_reports(args.plan_prefix, rows, args, log_path)
            if r["result"] == "失败":
                consecutive_fail += 1
                if consecutive_fail >= args.max_consecutive_fail:
                    log("\n连续失败 %d 次，达阈值 %d，中止压测。" % (consecutive_fail, args.max_consecutive_fail))
                    aborted = True
                    break
            elif r["result"] == "成功":
                consecutive_fail = 0
            if i < args.rounds and not aborted:
                log("      冷却 %ds ..." % args.cooldown)
                time.sleep(args.cooldown)
    except KeyboardInterrupt:
        interrupted = True
        log("\n收到中断（Ctrl+C），停止后续轮次；已完成的轮次已写入报告。")

    md_path, csv_path = write_reports(args.plan_prefix, rows, args, log_path)
    ok_cnt = sum(1 for r in rows if r["result"] == "成功")
    log("\n===== 压测结束%s =====" % ("（未跑完，被中断/中止）" if (interrupted or aborted) else ""))
    log("执行 %d 轮，成功 %d，失败 %d，超时 %d" % (
        len(rows), ok_cnt,
        sum(1 for r in rows if r["result"] == "失败"),
        sum(1 for r in rows if r["result"] == "超时")))
    log("报告: %s" % md_path)
    log("明细: %s" % csv_path)
    log_f.close()
    if args.dry_run:
        return 0
    if aborted:
        return 2
    return 0 if ok_cnt == len(rows) else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n已手动停止（报告在 main 内按轮次刷新，见 .obos_watch/stress_*.md）")
        sys.exit(0)
