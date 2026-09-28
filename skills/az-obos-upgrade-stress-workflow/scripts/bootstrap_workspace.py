#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""把 az-obos-upgrade-stress 的脚本资产部署到目标工作区，并做环境预检。

用法：
    python bootstrap_workspace.py --workspace C:\\path\\to\\ws
    python bootstrap_workspace.py --workspace <ws> --force    # 覆盖已存在的同名脚本
    python bootstrap_workspace.py --workspace <ws> --check    # 只做预检，不复制

预检项（非致命，逐项报告）：
    1. 继电器总服务的 GET /api/status 是否可达
    2. 本机 OBOS 代理端口是否在监听
    3. 目标工作区内依赖脚本是否齐备、能否 py_compile
    4. .obos_watch 数据目录是否可写

设计意图：这套压测脚本依赖多个同目录模块（obos_upgrade_watch /
obos_upgrade_stress），不允许跨越工作区 import。因此新机器/新工作区接入时，
先把脚本整体落到同一目录，再跑预检确认继电器与代理两条链路都通。
"""
import argparse
import io
import json
import os
import socket
import shutil
import sys

SKILL_DIR = os.path.dirname(os.path.abspath(__file__))

# 必须同时落到目标工作区的脚本（顺序无关，都是一级依赖）
ASSETS = [
    "obos_upgrade_watch.py",           # 状态/在线查询底层封装
    "obos_upgrade_stress.py",          # submit_plan / start_plan
    "obos_upgrade_stress_randpower.py",  # 压测主脚本（随机 + 节点定向断电）
    "rpower_report.py",                # 时间线 -> md/csv 报告重建
    "phase_probe.py",                  # 单轮高频只读阶段探针（不动继电器）
    "analyze_phase_probe.py",          # 探针结果分析
]

DEFAULT_RELAY_URL = "http://127.0.0.1:8080"
DEFAULT_OBOS_PROXY = ("127.0.0.1", 7897)


def check_relay(url, timeout=3.0):
    """继电器总服务连通性。只用标准库，避免引入依赖。"""
    import urllib.request
    try:
        with urllib.request.urlopen(url.rstrip("/") + "/api/status",
                                    timeout=timeout) as r:
            body = r.read().decode("utf-8", "ignore")[:200]
        return True, "HTTP %s | %s" % (r.status, body.replace("\n", " "))
    except Exception as e:  # noqa: BLE001
        return False, "%s: %s" % (type(e).__name__, str(e)[:160])


def check_port(host, port, timeout=2.0):
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True, "%s:%d 可连接" % (host, port)
    except Exception as e:  # noqa: BLE001
        return False, "%s: %s" % (type(e).__name__, str(e)[:160])


def main():
    ap = argparse.ArgumentParser(description="部署 OBOS 升级压测脚本资产并预检")
    ap.add_argument("--workspace", required=True, help="目标工作区绝对路径")
    ap.add_argument("--force", action="store_true", help="覆盖已存在的同名脚本")
    ap.add_argument("--check", action="store_true", help="只预检，不复制")
    ap.add_argument("--relay-url", default=DEFAULT_RELAY_URL)
    ap.add_argument("--obos-proxy-host", default=DEFAULT_OBOS_PROXY[0])
    ap.add_argument("--obos-proxy-port", type=int, default=DEFAULT_OBOS_PROXY[1])
    args = ap.parse_args()

    ws = os.path.abspath(args.workspace)
    report = {"status": "success", "workspace": ws,
              "copied": [], "skipped": [], "missing_assets": [],
              "compile": {}, "checks": {}, "warnings": []}

    # ---- 1. 目录 ----
    if not os.path.isdir(ws):
        os.makedirs(ws, exist_ok=True)
        report["warnings"].append("目标工作区不存在，已创建: %s" % ws)
    watch = os.path.join(ws, ".obos_watch")
    os.makedirs(watch, exist_ok=True)

    # ---- 2. 复制资产 ----
    if not args.check:
        for name in ASSETS:
            src = os.path.join(SKILL_DIR, name)
            dst = os.path.join(ws, name)
            if not os.path.isfile(src):
                report["missing_assets"].append(name)
                continue
            if os.path.isfile(dst) and not args.force:
                report["skipped"].append(name)
                continue
            shutil.copy2(src, dst)
            report["copied"].append(name)

    # ---- 3. 编译校验 ----
    import py_compile
    for name in ASSETS:
        p = os.path.join(ws, name)
        if not os.path.isfile(p):
            report["compile"][name] = "MISSING"
            continue
        try:
            py_compile.compile(p, doraise=True)
            report["compile"][name] = "ok"
        except Exception as e:  # noqa: BLE001
            report["compile"][name] = "FAIL: %s" % str(e)[:160]
            report["status"] = "error"

    # ---- 4. 环境预检 ----
    ok, txt = check_relay(args.relay_url)
    report["checks"]["relay"] = {"ok": ok, "detail": txt, "url": args.relay_url}
    if not ok:
        report["warnings"].append(
            "继电器总服务不可达 -> 断电动作会全部失败，只能跑 dry-run / 只读探针")

    ok2, txt2 = check_port(args.obos_proxy_host, args.obos_proxy_port)
    report["checks"]["obos_proxy"] = {"ok": ok2, "detail": txt2}
    if not ok2:
        report["warnings"].append(
            "本机 OBOS 代理端口不可达 -> MCP 状态查询会失败，先确认 OBOS 服务与登录态")

    probe = io.open(os.path.join(watch, ".write_probe"), "w", encoding="utf-8")
    probe.write("ok")
    probe.close()
    os.remove(os.path.join(watch, ".write_probe"))
    report["checks"]["state_dir"] = {"ok": True, "detail": watch}

    # ---- 5. 同目录依赖校验（脚本之间不允许跨工作区 import）----
    need = ["obos_upgrade_watch.py", "obos_upgrade_stress.py"]
    absent = [n for n in need if not os.path.isfile(os.path.join(ws, n))]
    report["checks"]["co_located_deps"] = {
        "ok": not absent, "detail": "缺失: %s" % absent if absent else "齐备"}
    if absent:
        report["status"] = "error"
        report["warnings"].append(
            "phase_probe.py / 主脚本依赖同目录模块 %s，缺失则无法运行" % absent)

    print(json.dumps(report, ensure_ascii=False, indent=2))
    print()
    print("--- 下一步 ---")
    print("  只读探针（不动继电器，先拿阶段时间线）:")
    print("    set HQ_OBOS_WS=%s" % ws)
    print("    python phase_probe.py -t <终端号> --firmware <包> --version <版本> -i 5")
    print("  节点定向压测:")
    print("    python obos_upgrade_stress_randpower.py -t <t1,t2,t3> --acc 3 --main 4 \\")
    print("      --firmware <A包> --version <A版> --pkgB-firmware <B包> --pkgB-version <B版> \\")
    print("      --ab-alternate-by device --rounds 40 --cut-mode phase \\")
    print("      --cut-nodes download,filecheck --node-alternate 1 --min-cut-gap 600 \\")
    print("      --trigger-rotate 1 --fast-poll-interval 5 --fast-poll-from 15 \\")
    print("      --poll-interval 20 --max-hours 12 --plan-prefix <前缀>")
    return 0 if report["status"] == "success" else 1


if __name__ == "__main__":
    sys.exit(main())
