# -*- coding: utf-8 -*-
"""
从随机断电压测的时间线（.obos_watch/rpower_<prefix>.jsonl）重建报告。

用途：压测脚本 obos_upgrade_stress_randpower.py 只在全部结束时才写 md/csv，
      而一次压测往往要跑几十小时。这个工具可以在压测**仍在运行时**随时出报告，
      也可以在脚本被中断、异常退出导致没有产物时，从时间线补齐报告。

用法：
    python rpower_report.py --prefix AT_RPower_261_20260921
    python rpower_report.py --prefix AT_RPower_261_20260921 --top 0      # 逐轮明细全量输出
    python rpower_report.py --jsonl <绝对路径.jsonl>

输出（写到 .obos_watch/）：
    rpower_<prefix>.md     汇总报告
    rpower_<prefix>.csv    逐轮明细
    rpower_<prefix>_cuts.csv 断电事件明细
"""
import argparse
import csv
import io
import json
import os
import sys
from collections import Counter, defaultdict
from datetime import datetime

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STATE_DIR = os.path.join(SCRIPT_DIR, ".obos_watch")

PHASE_CN = {
    "pending": "未下发",
    "hangup": "挂起/已派发",
    "download": "下载中",
    "flash": "校验刷写中",
    "reboot": "成功重启窗口",
    "terminal": "未知中间态",
    "unknown": "状态未知",
}

OK_SET = ("成功", "DRY-RUN")
# 非设备因素导致的"未终结"：不计入断电/升级失败率，单独统计
#   - 卡住停测：脚本到时(stuck-timeout / max-hours)主动收尾，任务实际可能仍在平台侧运行
#   - 下发失败/提交失败：本机 MCP/代理接口抖动，任务未真正下发
#   - 进行中：尚未进入终态
NOT_UPGRADE_FAIL = ("卡住停测", "到时停止", "下发失败", "提交失败", "进行中")
# 平台占位文案：不是失败原因，展示时要过滤掉
PLACEHOLDERS = {
    "download...": "平台占位文案，表示正在下载",
    "file checking": "平台占位文案，表示设备正在校验文件",
}


def load_events(path):
    evs = []
    with io.open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                evs.append(json.loads(line))
            except ValueError:
                continue
    return evs


def to_dt(s):
    for fmt in ("%Y-%m-%d %H:%M:%S.%f", "%Y-%m-%d %H:%M:%S"):
        try:
            return datetime.strptime(s, fmt)
        except (ValueError, TypeError):
            continue
    return None


def build(evs, terminals=None, firmware=None, version=None,
          cut_interval=(300, 1200), acc_ch=3, main_ch=4, top=60):
    """把事件流聚合成 (rows, cuts, meta)。"""
    meta = {
        "terminals": [], "firmware": firmware or "-", "version": version or "-",
        "rounds": "-", "cut_interval": cut_interval, "acc_ch": acc_ch, "main_ch": main_ch,
        "first_ts": "", "last_ts": "", "pkgs": [],
    }
    sched = None
    rounds = {}        # (term, rnd) -> dict
    status_last = {}   # (term, rnd) -> 最后一次 status 事件
    cuts = []          # 断电事件
    cut_open = None
    lost = set()
    lost_reason = {}
    stuck = []
    postponed = 0
    auth_fails = 0
    corrections = []
    crash = []
    phase_mode = None
    phase_seen = []
    phase_fires = []
    cut_skipped = []

    for e in evs:
        ev = e.get("event")
        ts = e.get("ts", "")
        if ts:
            if not meta["first_ts"]:
                meta["first_ts"] = ts
            meta["last_ts"] = ts
        term = e.get("terminal") or ""
        rnd = e.get("round")

        if ev == "phase_mode_start":
            phase_mode = e

        elif ev == "phase_seen":
            phase_seen.append({"ts": ts, "terminal": term, "round": rnd,
                               "node": e.get("node"), "master": e.get("master"),
                               "fired": bool(e.get("fired")),
                               "state": e.get("state"), "progress": e.get("progress"),
                               "fail_reason": e.get("fail_reason") or ""})

        elif ev == "phase_fire":
            phase_fires.append({"ts": ts, "terminal": term, "round": rnd,
                                "node": e.get("node"), "seq": e.get("seq"),
                                "state": e.get("state"), "progress": e.get("progress"),
                                "fail_reason": e.get("fail_reason") or ""})

        elif ev == "cut_skipped":
            cut_skipped.append({"ts": ts, "terminal": term, "round": rnd,
                                "node": e.get("node"), "reason": e.get("reason"),
                                "gap_s": e.get("gap_s"), "need_s": e.get("need_s")})

        elif ev == "scheduler_start":
            sched = e
            meta["cut_interval"] = (e.get("interval_min", cut_interval[0]),
                                    e.get("interval_max", cut_interval[1]))
            meta["acc_ch"] = e.get("acc_ch", acc_ch)
            meta["main_ch"] = e.get("main_ch", main_ch)

        elif ev == "round_start":
            if term and term not in meta["terminals"]:
                meta["terminals"].append(term)
            key = (term, rnd)
            fw = e.get("firmware") or ""
            rounds[key] = {
                "round": rnd, "terminal": term, "plan": e.get("plan", ""),
                "result": "进行中", "state": "", "state_name": "", "fail_reason": "",
                "version": e.get("version", ""), "duration_s": "", "cut_cnt": 0,
                "cut_phase": "", "start_at": ts, "end_at": "",
                "recover_s": "", "backoff_s": 0, "note": "",
                "pkg": ("%s(%s)" % (fw, e.get("version", ""))) if fw else "",
            }
            if fw and fw not in meta["pkgs"]:
                meta["pkgs"].append(fw)

        elif ev == "submit_fail":
            r = rounds.get((term, rnd))
            if r:
                r.update({"result": "提交失败", "note": str(e.get("err"))[:120]})
        elif ev == "start_fail":
            r = rounds.get((term, rnd))
            if r:
                r.update({"result": "下发失败", "note": str(e.get("err"))[:120]})

        elif ev == "status":
            r = rounds.get((term, rnd))
            if r is not None:
                st = e.get("state")
                nm = e.get("state_name") or ""
                r["state"] = st if st is not None else r["state"]
                r["state_name"] = nm or r["state_name"]
                if e.get("version") and not r["version"]:
                    r["version"] = e.get("version")
                fr = e.get("fail_reason") or ""
                if fr and fr not in PLACEHOLDERS:
                    r["fail_reason"] = fr
                # 平台已置终态但 round_end 尚未到达时，先按 stateName 给出临时结果
                if any(k in nm for k in ("成功",)):
                    r["result"] = "成功"
                elif any(k in nm for k in ("失败", "取消", "超时", "异常")):
                    r["result"] = nm
                status_last[(term, rnd)] = e

        elif ev == "round_end":
            r = rounds.get((term, rnd))
            if r is not None:
                r["result"] = e.get("result", r["result"])
                r["state"] = e.get("state", r["state"])
                r["state_name"] = e.get("state_name", r["state_name"])
                r["duration_s"] = e.get("duration_s", "")
                r["end_at"] = ts

        elif ev == "round_cuts":
            r = rounds.get((term, rnd))
            if r is not None:
                r["cut_cnt"] = e.get("cut_cnt", 0)
                ph = e.get("phases") or []
                r["cut_phase"] = "/".join(str(x) for x in ph)

        elif ev == "recover_ok" or ev == "recover_timeout":
            r = rounds.get((term, rnd))
            if r is not None:
                r["recover_s"] = e.get("cost_s", "")
                if ev == "recover_timeout":
                    r["note"] = (r["note"] + " 回线超时").strip()

        elif ev == "round_gap":
            r = rounds.get((term, rnd))
            if r is not None:
                r["backoff_s"] = e.get("wait_s", 0)

        elif ev == "cut_begin":
            cut_open = {
                "seq": e.get("seq"), "scheduled_at": ts,
                "interval_from_prev_s": e.get("interval_from_prev_s"),
                "planned_duration": e.get("duration"),
                "power_off_at": "", "power_on_at": "", "off_duration_s": "",
                "power_off_ok": "", "power_on_ok": "",
                "ch_states_off": "", "ch_states_on": "",
                # 注意：时间线里该字段叫 devices（不是 devices_at_cut）
                "devices_at_cut": e.get("devices") or e.get("devices_at_cut") or {},
                "online_before": e.get("online_before") or {},
                "planned_duration": e.get("duration"),
                "cut_mode": e.get("cut_mode") or "random",
                "trigger_node": e.get("trigger_node"),
                "trigger_master": e.get("trigger_master"),
            }

        elif ev == "power_off":
            if cut_open is not None:
                cut_open.update({
                    "power_off_at": ts, "power_off_ok": e.get("ok"),
                    "ch_states_off": e.get("ch_states"),
                    "devices_at_cut": (e.get("devices") or e.get("devices_at_cut")
                                       or cut_open["devices_at_cut"]),
                })

        elif ev == "power_on":
            if cut_open is not None:
                cut_open.update({
                    "power_on_at": ts, "power_on_ok": e.get("ok"),
                    "ch_states_on": e.get("ch_states"),
                    # 实际断电时长由 power_on 事件携带
                    "off_duration_s": e.get("actual_off_s", ""),
                })

        elif ev == "cut_end":
            if cut_open is not None:
                if not cut_open.get("off_duration_s"):
                    cut_open["off_duration_s"] = e.get("off_duration_s", "")
                cuts.append(cut_open)
                cut_open = None

        elif ev == "plan_stuck" or ev == "plan_stuck_abort":
            stuck.append({
                "ts": ts, "terminal": term, "round": rnd,
                "kind": "已停测" if ev == "plan_stuck_abort" else "等待中",
                "state": e.get("state"), "state_name": e.get("state_name"),
                "waited_s": e.get("waited_s"),
            })
            r = rounds.get((term, rnd))
            if r is not None:
                r["result"] = "卡住停测" if ev == "plan_stuck_abort" else (r["result"] if r["result"] != "进行中" else "卡住未终态")
                if e.get("state_name"):
                    r["state_name"] = e["state_name"]
                if e.get("state"):
                    r["state"] = e["state"]
        elif ev == "cut_postponed":
            postponed = max(postponed, int(e.get("postponed") or 0))
        elif ev == "device_lost":
            if term:
                lost.add(term)
                lost_reason[term] = e.get("why") or ""
        elif ev == "mcp_auth_fail":
            auth_fails = max(auth_fails, int(e.get("count") or 0))
        elif ev == "post_review_correction":
            corrections.append(e.get("note") or "")
        elif ev == "worker_crash":
            crash.append((term, str(e.get("error"))[:120]))

    if cut_open is not None:  # 断电进行中时间线就断了
        cuts.append(cut_open)

    if terminals:
        for t in terminals:
            if t not in meta["terminals"]:
                meta["terminals"].append(t)

    rows = sorted(rounds.values(), key=lambda r: (r["round"] or 0, r["terminal"]))
    meta["lost"] = lost
    meta["lost_reason"] = lost_reason
    meta["stuck"] = stuck
    meta["postponed"] = postponed
    meta["auth_fails"] = auth_fails
    meta["corrections"] = corrections
    meta["crash"] = crash
    meta["phase_mode"] = phase_mode
    meta["phase_seen"] = phase_seen
    meta["phase_fires"] = phase_fires
    meta["cut_skipped"] = cut_skipped
    meta["top"] = top
    return rows, cuts, meta


def render(rows, cuts, meta):
    ok = sum(1 for r in rows if r["result"] in OK_SET)
    done = [r for r in rows if r["result"] != "进行中"]
    soft = sum(1 for r in rows if r["result"] in NOT_UPGRADE_FAIL)
    real_fail = len(rows) - ok - soft
    fail = len(done) - ok
    durs = [r["duration_s"] for r in rows if isinstance(r["duration_s"], int) and r["duration_s"] > 0]
    recs = [r["recover_s"] for r in rows if isinstance(r["recover_s"], int) and r["recover_s"] > 0]

    per_dev = {}
    for r in rows:
        d = per_dev.setdefault(r["terminal"], {"ok": 0, "rfail": 0, "soft": 0, "cuts": 0})
        if r["result"] in OK_SET:
            d["ok"] += 1
        elif r["result"] in NOT_UPGRADE_FAIL:
            d["soft"] += 1
        else:
            d["rfail"] += 1
        d["cuts"] += 1 if r.get("cut_cnt") else 0

    # 归因：切电瞬间阶段 -> 该轮结果
    by_phase = defaultdict(Counter)
    cut_rounds = {}
    for c in cuts:
        for term, snap in (c.get("devices_at_cut") or {}).items():
            rnd = (snap or {}).get("round")
            if rnd:
                cut_rounds[(term, rnd)] = PHASE_CN.get((snap or {}).get("phase", "unknown"), "?")
    for r in done:
        ph = cut_rounds.get((r["terminal"], r["round"]))
        if ph:
            by_phase[ph][r["result"]] += 1

    L = [
        "# OBOS 多设备并行升级 + %s断电压测报告" % (
            "节点定向" if meta.get("phase_mode") else "随机"), "",
        "> 由 `rpower_report.py` 从时间线重建；压测**仍在运行时**本报告为实时快照。", "",
        "- 设备（%d 台）: %s" % (len(meta["terminals"]), ", ".join(meta["terminals"]) or "-"),
        "- 固件 / 版本: %s / %s%s" % (
            meta["firmware"], meta["version"],
            ("（A/B 交替，实际用到：%s）" % ", ".join(meta["pkgs"])) if len(meta["pkgs"]) > 1 else ""),
        "- 时间跨度: %s ~ %s" % (meta["first_ts"] or "-", meta["last_ts"] or "-"),
        "- 轮次记录 %d 条 | 成功 %d | 设备升级失败 %d | 非设备因素未终结 %d（急停/代理抖动，非断电所致）" % (
            len(rows), ok, real_fail, soft),
        "- 断电模式: %s" % (
            "节点定向（目标 %s｜按轮交替%s｜master 顺延%s｜最小间隔 %ss）" % (
                ",".join((meta.get("phase_mode") or {}).get("nodes") or []),
                "开" if (meta.get("phase_mode") or {}).get("alternate") else "关",
                "开" if (meta.get("phase_mode") or {}).get("rotate") else "关",
                (meta.get("phase_mode") or {}).get("min_gap"))
            if meta.get("phase_mode") else
            "随机定时（间隔 U(%s,%s)s）" % (meta["cut_interval"][0], meta["cut_interval"][1])),
        "- 断电: 实际 %d 次 | 通道 ACC=%s 主电=%s" % (
            len(cuts), meta["acc_ch"], meta["main_ch"]),
        "- 单轮耗时: min=%s max=%s avg=%s s" % (
            min(durs) if durs else "-", max(durs) if durs else "-",
            int(sum(durs) / len(durs)) if durs else "-"),
        "- 断电后回线耗时: max=%s avg=%s s" % (max(recs) if recs else "-",
                                               int(sum(recs) / len(recs)) if recs else "-"),
        "- 长时间无终态（已记为卡住，期间不下发新任务）: %d 次%s" % (
            len(meta.get("stuck") or []), ("；最近：" + "、".join(
                "%s R%s 等%s仍未终态(%s)" % (x["terminal"], x["round"], x["waited_s"], x["state_name"])
                for x in (meta.get("stuck") or [])[-3:])) if meta.get("stuck") else ""),
        "- 断电时点无设备可切而推迟: 累计 %d 次复检" % (meta.get("postponed") or 0),
        "- 异常/失联设备（已停止该设备后续测试）: %s" % (
            "; ".join("%s —— %s" % (t, (meta.get("lost_reason") or {}).get(t, "原因未记录"))
                      for t in sorted(meta["lost"])) or "无"),
        "- 认证抖动: 累计 %d 次" % meta["auth_fails"],
    ]
    if meta.get("corrections"):
        L += ["- 事后复查修正: %s" % " / ".join(meta["corrections"]),
              "> 上方「异常/失联设备」为脚本运行当时的判定（受 --max-hours 到点收尾影响），"
              "事后复查见上一条修正说明，不构成设备侧失败。"]
    L += [
        "- 生成时间: %s" % datetime.now().strftime("%Y-%m-%d %H:%M:%S"), "",
        "## 一、分设备汇总", "",
        "| 设备 | 成功 | 设备升级失败 | 非设备因素未终结 | 经历断电轮次 |", "| --- | --- | --- | --- | --- |",
    ]
    for t in meta["terminals"]:
        d = per_dev.get(t, {"ok": 0, "rfail": 0, "soft": 0, "cuts": 0})
        L.append("| %s | %d | %d | %d | %d |" % (t, d["ok"], d["rfail"], d["soft"], d["cuts"]))

    L += ["", "## 二、断电归因（切电瞬间设备所处阶段 → 该轮结果）", "",
          "> 本次压测的核心产出：判断哪个阶段断电最致命。",
          "> 失败率仅统计**设备侧升级失败**；急停收尾、代理抖动等非断电因素单列，不参与失败率。", "",
          "| 切电时阶段 | 成功 | 设备失败 | 非设备未终结 | 合计 | 断电致失败率 |",
          "| --- | --- | --- | --- | --- | --- |"]
    if by_phase:
        for ph, c in sorted(by_phase.items(), key=lambda kv: -sum(kv[1].values())):
            tot = sum(c.values())
            okn = sum(v for k, v in c.items() if k in OK_SET)
            softn = sum(v for k, v in c.items() if k in NOT_UPGRADE_FAIL)
            rfail = tot - okn - softn
            L.append("| %s | %d | %d | %d | %d | %.0f%% |" % (
                ph, okn, rfail, softn, tot, (100.0 * rfail / tot) if tot else 0))
    else:
        L.append("| （尚无已完成的断电轮） | - | - | - | - | - |")

    # ---- 节点定向断电统计（phase 模式）----
    if meta.get("phase_mode") is not None:
        pm = meta.get("phase_mode") or {}
        fires = meta.get("phase_fires") or []
        seen = meta.get("phase_seen") or []
        skips = meta.get("cut_skipped") or []
        rmap = {(r["terminal"], r["round"]): r for r in rows}
        cut_by_seq = {c.get("seq"): c for c in cuts}

        by_node_fire = Counter(f["node"] for f in fires)
        by_node_seen = Counter(s["node"] for s in seen)

        L += ["", "## 三、节点定向断电（命中样本 / 连带样本）", "",
              "> 触发配置: 目标节点 %s | 按轮交替=%s | master 顺延=%s | 最小间隔 %ss | "
              "下载窗口提速 %ss（自下发后 %ss 起）" % (
                  ",".join(pm.get("nodes") or []),
                  "开" if pm.get("alternate") else "关",
                  "开" if pm.get("rotate") else "关",
                  pm.get("min_gap"), pm.get("fast_interval"), pm.get("fast_from")),
              "> **命中样本** = trigger master 在自己确实处于目标节点时触发的断电（有效证据）；",
              "> **连带样本** = 同一次断电中被一起打掉的另外两台（只有一组继电器，无法避免），",
              "> 其当时所处阶段仅供参考，**不参与**节点断电的失败率归因。", "",
              "- 节点被观测到 %d 次（%s）；其中触发断电 %d 次（%s）" % (
                  len(seen), dict(by_node_seen), len(fires), dict(by_node_fire)),
              "- 因最小间隔护栏放弃: %d 次（放弃即作废，不排队补切）" % len(skips), "",
              "| # | 触发节点 | 命中设备 | 轮次 | 命中时状态 | 该轮最终结果 | 连带设备(切电时阶段) |",
              "| --- | --- | --- | --- | --- | --- | --- |"]
        if fires:
            for f in fires:
                c = cut_by_seq.get(f.get("seq")) or {}
                others = []
                for t, sn in (c.get("devices_at_cut") or {}).items():
                    if t == f["terminal"]:
                        continue
                    others.append("%s=%s" % (t, PHASE_CN.get((sn or {}).get("phase", "unknown"), "?")))
                r = rmap.get((f["terminal"], f["round"]))
                L.append("| %s | %s | %s | R%s | %s prog=%s %s | %s | %s |" % (
                    f.get("seq"), f.get("node"), f["terminal"], f.get("round"),
                    f.get("state"), f.get("progress"),
                    (f.get("fail_reason") or "").replace("|", "/"),
                    (r or {}).get("result", "?"),
                    "; ".join(others) or "-"))
        else:
            L.append("| （尚未命中目标节点） | - | - | - | - | - | - |")

        if skips:
            L += ["", "### 因最小间隔护栏放弃的触发", "",
                  "| 时间 | 设备 | 轮次 | 节点 | 距上次断电s | 需要s |", "| --- | --- | --- | --- | --- | --- |"]
            for s in skips:
                L.append("| %s | %s | R%s | %s | %s | %s |" % (
                    s["ts"], s["terminal"], s["round"], s["node"], s["gap_s"], s["need_s"]))
            L.append("")

        L += ["", "### 目标节点「断电 → 该轮结果」归因", "",
              "| 触发节点 | 成功 | 设备失败 | 非设备未终结 | 合计 | 断电致失败率 |",
              "| --- | --- | --- | --- | --- | --- |"]
        by_node_res = defaultdict(Counter)
        for f in fires:
            r = rmap.get((f["terminal"], f["round"]))
            if r is None or r["result"] == "进行中":
                continue
            by_node_res[f["node"]][r["result"]] += 1
        if by_node_res:
            for nd, c in sorted(by_node_res.items()):
                tot = sum(c.values())
                okn = sum(v for k, v in c.items() if k in OK_SET)
                softn = sum(v for k, v in c.items() if k in NOT_UPGRADE_FAIL)
                rf = tot - okn - softn
                L.append("| %s | %d | %d | %d | %d | %.0f%% |" % (
                    nd, okn, rf, softn, tot, (100.0 * rf / tot) if tot else 0))
        else:
            L.append("| （尚无已完成的命中轮） | - | - | - | - | - |")

    L += ["", "## 四、断电事件明细", "",
          "| # | 间隔s | 触发 | 切电时刻 | 上电时刻 | 实际断电s | 通道状态(断/上) | 切电瞬间各台阶段 |",
          "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for c in cuts:
        devs = "; ".join("%s=%s" % (t, PHASE_CN.get((s or {}).get("phase", "unknown"), "?"))
                         for t, s in (c.get("devices_at_cut") or {}).items())
        trig = "-"
        if c.get("trigger_node"):
            trig = "%s@%s" % (c.get("trigger_node"), c.get("trigger_master"))
        L.append("| %s | %s | %s | %s | %s | %s | %s -> %s | %s |" % (
            c.get("seq"), c.get("interval_from_prev_s") if c.get("interval_from_prev_s") is not None else "-",
            trig,
            c.get("power_off_at") or "(进行中)", c.get("power_on_at") or "-",
            c.get("off_duration_s") or "-", c.get("ch_states_off") or "-",
            c.get("ch_states_on") or "-", devs or "-"))

    L += ["", "## 五、逐轮明细", ""]
    shown = rows
    if meta.get("top"):
        tail = rows[-meta["top"]:]
        shown = tail
        L.append("> 仅显示最近 %d 条（共 %d 条，完整明细见 CSV）" % (len(tail), len(rows)))
        L.append("")
    L += ["| 轮次 | 设备 | 本轮包 | 结果 | state | stateName | 上报版本 | 期间断电 | 切电阶段 | 耗时s | 回线s | 退让s | 备注 |",
          "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for r in shown:
        L.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["round"], r["terminal"], r.get("pkg", "") or "-", r["result"],
            r.get("state", ""), r.get("state_name", ""),
            r.get("version", ""), r.get("cut_cnt", 0), r.get("cut_phase", "") or "-",
            r.get("duration_s", ""), r.get("recover_s", "") if r.get("recover_s") != "" else "-",
            r.get("backoff_s", 0),
            ((r.get("fail_reason") or "") + " " + (r.get("note") or "")).strip().replace("|", "/")))

    if meta["crash"]:
        L += ["", "## 六、异常", ""]
    if meta.get("stuck"):
        L += ["### 无终态（卡住）轮次 —— 期间按规则未下发新任务", "",
              "| 时间 | 设备 | 轮次 | 状态 | 已等待s | 处理 |",
              "| --- | --- | --- | --- | --- | --- |"]
        for x in meta["stuck"]:
            L.append("| %s | %s | %s | %s %s | %s | %s |" % (
                x["ts"], x["terminal"], x["round"], x["state"], x["state_name"],
                x["waited_s"], x["kind"]))
        L.append("")

        for t, err in meta["crash"]:
            L.append("- worker 线程异常：%s —— %s" % (t, err))

    return "\n".join(L) + "\n"


def write_csv(path, rows, cuts):
    cols = ("round", "terminal", "plan", "pkg", "result", "state", "state_name", "fail_reason",
            "version", "cut_cnt", "cut_phase", "duration_s", "recover_s", "backoff_s",
            "start_at", "end_at", "note")
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(list(cols))
        for r in sorted(rows, key=lambda x: (x["round"] or 0, x["terminal"])):
            w.writerow([r.get(k, "") for k in cols])

    cp = path.replace(".csv", "_cuts.csv")
    with open(cp, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(["seq", "scheduled_at", "interval_from_prev_s", "power_off_at", "power_on_at",
                    "off_duration_s", "power_off_ok", "power_on_ok", "ch_states_off",
                    "ch_states_on", "devices_at_cut", "online_before"])
        for c in cuts:
            w.writerow([c.get("seq"), c.get("scheduled_at"), c.get("interval_from_prev_s"),
                        c.get("power_off_at"), c.get("power_on_at"), c.get("off_duration_s"),
                        c.get("power_off_ok"), c.get("power_on_ok"),
                        json.dumps(c.get("ch_states_off"), ensure_ascii=False),
                        json.dumps(c.get("ch_states_on"), ensure_ascii=False),
                        json.dumps(c.get("devices_at_cut"), ensure_ascii=False),
                        json.dumps(c.get("online_before"), ensure_ascii=False)])
    return cp


def generate(prefix, terminals=None, firmware=None, version=None, top=60, quiet=True):
    """按 prefix 重新生成 md/csv 报告。返回 (md, csv, cuts_csv) 或 None。"""
    jsonl = os.path.join(STATE_DIR, "rpower_%s.jsonl" % prefix)
    if not os.path.exists(jsonl):
        return None
    evs = load_events(jsonl)
    rows, cuts, meta = build(evs, terminals=terminals, firmware=firmware,
                             version=version, top=top)
    md = render(rows, cuts, meta)
    md_path = os.path.join(STATE_DIR, "rpower_%s.md" % prefix)
    csv_path = os.path.join(STATE_DIR, "rpower_%s.csv" % prefix)
    with io.open(md_path, "w", encoding="utf-8") as f:
        f.write(md)
    cuts_csv = write_csv(csv_path, rows, cuts)
    if not quiet:
        print("事件 %d 条 | 轮次 %d 条 | 断电 %d 次" % (len(evs), len(rows), len(cuts)))
    return md_path, csv_path, cuts_csv


def main():
    ap = argparse.ArgumentParser(description="从压测时间线重建报告（可在压测运行中随时执行）")
    ap.add_argument("--prefix", help="计划前缀，如 AT_RPower_261_20260921")
    ap.add_argument("--jsonl", help="直接指定 jsonl 时间线路径")
    ap.add_argument("-t", "--terminals", default="", help="设备列表，逗号分隔（可选，用于补全）")
    ap.add_argument("--firmware", default=None)
    ap.add_argument("--version", default=None)
    ap.add_argument("--top", type=int, default=60, help="md 中逐轮明细显示最近 N 条，0=全量")
    args = ap.parse_args()

    if args.jsonl:
        jsonl = args.jsonl
        prefix = os.path.basename(jsonl).replace("rpower_", "").replace(".jsonl", "")
    elif args.prefix:
        prefix = args.prefix
        jsonl = os.path.join(STATE_DIR, "rpower_%s.jsonl" % prefix)
    else:
        print("需要 --prefix 或 --jsonl", file=sys.stderr)
        return 2

    if not os.path.exists(jsonl):
        print("时间线不存在: %s" % jsonl, file=sys.stderr)
        return 2

    evs = load_events(jsonl)
    terms = [x.strip() for x in args.terminals.split(",") if x.strip()]
    rows, cuts, meta = build(evs, terminals=terms, firmware=args.firmware, version=args.version,
                             top=args.top)
    md = render(rows, cuts, meta)

    md_path = os.path.join(STATE_DIR, "rpower_%s.md" % prefix)
    csv_path = os.path.join(STATE_DIR, "rpower_%s.csv" % prefix)
    with io.open(md_path, "w", encoding="utf-8") as f:
        f.write(md)
    cuts_csv = write_csv(csv_path, rows, cuts)

    print("事件 %d 条 | 轮次 %d 条 | 断电 %d 次" % (len(evs), len(rows), len(cuts)))
    print("报告: %s" % md_path)
    print("明细: %s" % csv_path)
    print("断电: %s" % cuts_csv)
    return 0


if __name__ == "__main__":
    sys.exit(main())
