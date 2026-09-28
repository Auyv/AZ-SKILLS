#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""OBOS 多设备并行升级 + 随机断电（共享继电器）稳定性压测

场景约束（用户给定）：
  - 3 台终端并行，各自独立跑 N 轮（默认 100 轮/台）
  - 断电器数量有限：一次断电动作会同时打掉所有在测设备（全局事件）
  - 无法预知每台设备的升级阶段 -> 断电改为**与阶段解耦的随机定时触发**
  - 继电器通道：ch3 = ACC 开关，ch4 = 主电开关；通道 ON = 通电
  - 设备无备电，断主电即硬掉电；断电 10s 后恢复
  - 单轮失败 -> 退让 15min 后再下发下一轮
  - 不做变砖兜底：目标是验证稳定性 + 记录全过程状态与上下电信息，便于事后归因

架构：单进程
  - N 个 DeviceWorker 线程：各自跑「确认在线 -> submit -> start -> 轮询到终态」，互不等待
  - 1 个 PowerCutScheduler 线程：按 U(min,max) 随机间隔触发断电（切 ch3+ch4 -> 等 d 秒 -> 恢复）
  - 1 个 OnlineWatcher 线程：定期采三台在线状态，记录「离线/回线」事件与耗时
  - 1 个 RelayBank 单例：唯一能操作继电器的出口（worker 不得自行断电）
  - 1 个全局 MCP 锁：本机 azkits MCP 是单实例串行的，所有 MCP 调用必须串行化

产物（.obos_watch/ 下）：
  rpower_<prefix>.log          人读主日志
  rpower_<prefix>.jsonl        全局合并时间线（含每次 status 采样、上下电、在线变化）
  rpower_<prefix>.csv          逐轮明细
  rpower_<prefix>_cuts.csv     每次断电事件明细
  rpower_<prefix>.md           汇总报告（含「断电时阶段 vs 该轮结果」归因表）

安全阀：
  - 退出/中断路径**必定恢复供电**（always-on）
  - STOP 文件急停
  - 单台长时间不回线 -> 标记失联、停该台，其余继续（--abort-on-brick 可改为全局中止）
  - 断电总次数上限、总时长上限

用法：
  # 冒烟：3 台各 1 轮 + 1 次断电（验证链路）
  python obos_upgrade_stress_randpower.py -t 0049519998,0049519988,0049519989 \
      --rounds 1 --max-cuts 1 --first-cut-after 100 --plan-prefix SMOKE

  # 正式：3 台各 100 轮，5~20min 随机断一次，每次 10s
  python obos_upgrade_stress_randpower.py -t 0049519998,0049519988,0049519989 --rounds 100

退出码：0 正常跑完；2 存在失败/失联；1 参数或致命错误。
"""

import argparse
import csv
import json
import os
import random
import sys
import threading
import time
import urllib.error
import urllib.request
import uuid
from datetime import datetime
from collections import Counter

from obos_upgrade_watch import call_mcp, query_status, query_online, is_terminal, KNOWN_PLACEHOLDERS
from obos_upgrade_stress import submit_plan, start_plan

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STATE_DIR = os.path.join(SCRIPT_DIR, ".obos_watch")

# 本机 azkits MCP 单实例串行：所有 MCP 调用必须走这把锁，否则并发会互相报「脚本正在运行」
MCP_LOCK = threading.RLock()

# 阶段映射（与 OBOS 状态码对应），用于「断电瞬间设备在干什么」的归因
PHASE_CN = {
    "pending": "未下发",
    "hangup": "挂起/已派发",
    "download": "下载中",
    "flash": "校验刷写中",
    "reboot": "成功重启窗口",
    "terminal": "未知中间态",
    "unknown": "状态未知",
}

SUCCESS_KEYWORDS = ("成功",)
FAIL_KEYWORDS = ("失败", "取消", "超时", "异常")


# ---------------------------------------------------------------------------
# 通用工具
# ---------------------------------------------------------------------------
def now_str(ms=True):
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3] if ms else datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def hm():
    return datetime.now().strftime("%H:%M:%S")


class Logger:
    def __init__(self, path):
        self.lock = threading.Lock()
        self.f = open(path, "a", encoding="utf-8")

    def __call__(self, line):
        with self.lock:
            out = "[%s] %s" % (hm(), line)
            print(out, flush=True)
            self.f.write(out + "\n")
            self.f.flush()

    def close(self):
        try:
            self.f.close()
        except Exception:  # noqa: BLE001
            pass


class Timeline:
    """全局合并时间线：一行一个 JSON 事件，带毫秒时间戳，便于事后跨设备对齐。"""

    def __init__(self, path):
        self.lock = threading.Lock()
        self.f = open(path, "a", encoding="utf-8")

    def add(self, event, terminal="", **kw):
        rec = {"ts": now_str(), "event": event, "terminal": terminal}
        rec.update(kw)
        with self.lock:
            self.f.write(json.dumps(rec, ensure_ascii=False) + "\n")
            self.f.flush()

    def close(self):
        try:
            self.f.close()
        except Exception:  # noqa: BLE001
            pass


def phase_of(st):
    """OBOS 状态 -> 阶段名。"""
    if not st or not st.get("ok"):
        return "unknown"
    state = str(st.get("state") or "")
    name = st.get("state_name") or ""
    try:
        prog = int(st["progress"]) if st.get("progress") is not None else None
    except (TypeError, ValueError):
        prog = None
    if state == "14" or "未下发" in name:
        return "pending"
    if state == "15" or "挂起" in name:
        return "hangup"
    if state == "2":
        return "download" if (prog is not None and prog < 100) else "flash"
    if state == "0" or "成功" in name:
        return "reboot"
    return "terminal"


# --- 定向断电的「节点」判定（--cut-mode phase 用） ---------------------------
# failReason 才是真正的阶段信号（实测 4519 次轮询 + 83 次 5s 采样：progress 只有
# 0/100 两个值，它是状态指示器，不是百分比进度条）：
#   state=2 + failReason 'begin download'/'download...' -> 下载中，实测窗口仅 ~13.5s
#   state=2 + failReason 'file checking'                -> 校验+刷写，窗口中位 ~482s
DOWNLOAD_NODE_KEYS = ("begin download", "download")
FILECHECK_NODE_KEYS = ("file check", "filecheck", "checking")


def phase_node(st):
    """当前所处的定向断电节点：'download' / 'filecheck' / None（不在节点上）。"""
    if not st or not st.get("ok"):
        return None
    if str(st.get("state") or "") != "2":
        return None
    fr = str(st.get("fail_reason") or "").strip().lower()
    if any(k in fr for k in FILECHECK_NODE_KEYS):
        return "filecheck"
    if any(k in fr for k in DOWNLOAD_NODE_KEYS):
        return "download"
    try:
        prog = int(st["progress"]) if st.get("progress") is not None else None
    except (TypeError, ValueError):
        prog = None
    if prog is None:
        return None
    return "download" if prog < 100 else "filecheck"


def target_nodes(args, rnd):
    """本轮允许触发的目标节点。--node-alternate 打开时按轮在节点间交替
    （R1 打 download、R2 打 filecheck、R3 又回 download ...）。

    为什么必须交替：download 窗口在 filecheck 之前，若两个节点同时启用，
    master 一定先在 download 命中并断掉，filecheck 永远轮不到；而连击
    （同一轮断两次，间隔仅 ~90s）会破坏「设备需连续供电 10~15 分钟才收敛」
    的物理约束 —— AT_RP3 已实测过这种僵尸任务。
    """
    nodes = list(args.cut_node_set)
    if not nodes:
        return []
    if not args.node_alternate or len(nodes) == 1:
        return nodes
    return [nodes[(rnd - 1) % len(nodes)]]


AUTH_ERROR_HINTS = ("认证失败", "登录失败", "请检查账号", "未登录", "未授权", "token", "401")


def is_auth_error(err):
    """识别 OBOS 登录态失效类错误：这类错误属环境抖动，不能算作升级失败。"""
    if not err:
        return False
    e = str(err)
    return any(h in e for h in AUTH_ERROR_HINTS)


def mcp_status(ctx, plan):
    """带重试与认证抖动保护的升级状态查询。"""
    n = max(1, int(ctx.args.mcp_retry))
    st = None
    for i in range(n):
        with MCP_LOCK:
            st = query_status(plan)
        if st.get("ok"):
            ctx.pause_end()
            return st
        err = str(st.get("error") or "")
        if is_auth_error(err):
            ctx.note_auth_fail(err)
            time.sleep(ctx.args.auth_retry_interval)
            continue
        if i < n - 1:
            time.sleep(4)
    return st


def mcp_online(ctx, term):
    """带重试与认证抖动保护的在线状态查询。返回 (True/False/None, 文案)。"""
    n = max(1, int(ctx.args.mcp_retry))
    online, txt = None, None
    for i in range(n):
        with MCP_LOCK:
            online, txt = query_online(term)
        if online is not None:
            ctx.pause_end()
            return online, txt
        if is_auth_error(txt):
            ctx.note_auth_fail(str(txt))
            time.sleep(ctx.args.auth_retry_interval)
            continue
        if i < n - 1:
            time.sleep(4)
    return online, txt


# ---------------------------------------------------------------------------
# 继电器：唯一触碰电源的出口（HTTP -> 本地总服务 -> COM）
# ---------------------------------------------------------------------------
class RelayBank:
    """多通道继电器。ACC(ch3) 与 主电(ch4) 需分别下发，无批量接口。

    上下电顺序（车辆场景约定，可用参数覆盖）：
      断电：先断 ACC，再断主电   （模拟熄火后掉电）
      上电：先上主电，再上 ACC   （模拟上电后点火）
    """

    def __init__(self, base_url, acc_ch, main_ch, gap=0.4, dry_run=False, timeout=15, log=None):
        self.base = base_url.rstrip("/")
        self.acc_ch = int(acc_ch)
        self.main_ch = int(main_ch)
        self.gap = gap
        self.dry_run = dry_run
        self.timeout = timeout
        self.log = log or (lambda s: None)
        self._sim = {}

    def _call(self, path, payload=None):
        if self.dry_run:
            if payload is not None:
                ch = payload.get("channel")
                st = payload.get("state")
                if ch is not None and st in ("on", "always-on", "off"):
                    self._sim[int(ch)] = 1 if st in ("on", "always-on") else 0
            self.log("      [relay-dry-run] %s %s%s" % (
                "POST" if payload is not None else "GET", path,
                (" " + json.dumps(payload, ensure_ascii=False)) if payload is not None else ""))
            chans = {i: self._sim.get(i, 1) for i in (1, 2, 3, 4)}
            return {"ok": True, "dry_run": True, "channels": chans,
                    "payload": "status:" + ",".join("c%d=%d" % (k, v) for k, v in chans.items())}, None
        url = self.base + path
        data = json.dumps(payload).encode("utf-8") if payload is not None else None
        req = urllib.request.Request(
            url, data=data,
            method="POST" if data is not None else "GET",
            headers={"Content-Type": "application/json",
                     "X-Esp-Relay-Source": "script",
                     "X-Esp-Relay-Request-Id": uuid.uuid4().hex[:16]},
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as r:
                return json.loads(r.read().decode("utf-8", "replace")), None
        except urllib.error.URLError as e:
            return None, "继电器服务不可达(%s): %s" % (url, e)
        except Exception as e:  # noqa: BLE001
            return None, "继电器请求异常: %r" % e

    def _set(self, ch, state):
        body, err = self._call("/api/set", {"channel": ch, "state": state})
        if body is None:
            return False, err
        if not body.get("ok"):
            return False, "继电器拒绝: %s" % json.dumps(body, ensure_ascii=False)[:160]
        return True, None

    def read_states(self):
        """返回 ({ch: 0/1}, error)。"""
        body, err = self._call("/api/status")
        if body is None:
            return None, err
        out = {}
        ch = body.get("channels")
        if isinstance(ch, list):
            for item in ch:
                out[int(item.get("channel"))] = 1 if item.get("state") == "on" else 0
        elif isinstance(ch, dict):
            for k, v in ch.items():
                out[int(str(k).lstrip("cC"))] = int(v)
        return out, None

    def power_off(self):
        """断 ACC -> 断主电。返回 (ok, err, 各步骤记录)。"""
        steps = []
        ok1, e1 = self._set(self.acc_ch, "off")
        steps.append({"act": "acc_off", "ch": self.acc_ch, "ok": ok1, "err": e1})
        time.sleep(self.gap)
        ok2, e2 = self._set(self.main_ch, "off")
        steps.append({"act": "main_off", "ch": self.main_ch, "ok": ok2, "err": e2})
        if not (ok1 and ok2):
            return False, "; ".join("ch%s:%s" % (s["ch"], s["err"]) for s in steps if not s["ok"]), steps
        return True, None, steps

    def power_on(self):
        """上主电 -> 上 ACC，均用 always-on（总服务自动续租，避免租约到期意外掉电）。"""
        steps = []
        ok1, e1 = self._set(self.main_ch, "always-on")
        steps.append({"act": "main_on", "ch": self.main_ch, "ok": ok1, "err": e1})
        time.sleep(self.gap)
        ok2, e2 = self._set(self.acc_ch, "always-on")
        steps.append({"act": "acc_on", "ch": self.acc_ch, "ok": ok2, "err": e2})
        if not (ok1 and ok2):
            return False, "; ".join("ch%s:%s" % (s["ch"], s["err"]) for s in steps if not s["ok"]), steps
        return True, None, steps


# ---------------------------------------------------------------------------
# 共享状态
# ---------------------------------------------------------------------------
class SharedState:
    """每台设备的最新已知状态（供断电调度器在切电瞬间抓快照）+ 断电事件登记表。"""

    def __init__(self, terminals):
        self.lock = threading.Lock()
        self.latest = {t: None for t in terminals}   # term -> {ts, phase, state, state_name, progress, round, plan, round_elapsed_s}
        self.online = {t: None for t in terminals}   # term -> True/False
        self.cuts = []                               # 断电事件列表
        self.lost = set()                            # 判定失联的设备
        self.lost_reason = {}                        # term -> 判失联的原因（进报告）

    def set_latest(self, term, **kw):
        with self.lock:
            cur = self.latest.get(term) or {}
            cur.update(kw)
            cur["ts"] = now_str()
            self.latest[term] = cur

    def snapshot(self):
        with self.lock:
            return json.loads(json.dumps(self.latest, ensure_ascii=False))

    # ---- 「测试节拍」标记：断电调度据此判断此刻能不能对这台设备下手 ----
    # idle=空转(未开轮) / active=轮次进行中 / stuck=已超期无终态 / backoff=收尾退让中
    def set_round_state(self, term, rstate, **kw):
        # 形参用 rstate：调用方常同时传 state= 记录平台状态码，不能与形参同名
        self.set_latest(term, round_state=rstate, **kw)

    def eligible_for_cut(self, terminals, min_elapsed, budget, max_elapsed):
        """此刻"值得断电"的设备：
        - 处于进行中的轮次（空转/退让/已卡死的一律不打，避免白掉电还污染数据）
        - 本轮已跑够时长（避免刚提交就切，拿不到有价值的样本）
        - 未超过单轮预期时长（避免对着已卡死的任务反复补刀）
        - 本轮断电配额未用尽
        """
        out = []
        with self.lock:
            for t in terminals:
                if t in self.lost:
                    continue
                st = self.latest.get(t) or {}
                if st.get("round_state") != "active":
                    continue
                el = st.get("round_elapsed_s") or 0
                if el < min_elapsed:
                    continue
                if max_elapsed > 0 and el > max_elapsed:
                    continue
                if (st.get("cut_used") or 0) >= budget:
                    continue
                out.append(t)
        return out

    def note_cut_used(self, terms):
        with self.lock:
            for t in terms:
                cur = self.latest.get(t) or {}
                cur["cut_used"] = (cur.get("cut_used") or 0) + 1
                self.latest[t] = cur

    def mark_lost(self, term, why=""):
        with self.lock:
            self.lost.add(term)
            if why and term not in self.lost_reason:
                self.lost_reason[term] = why

    def is_lost(self, term):
        with self.lock:
            return term in self.lost

    def add_cut(self, ev):
        with self.lock:
            self.cuts.append(ev)
            return len(self.cuts) - 1

    def cut_count(self):
        with self.lock:
            return len(self.cuts)

    def cuts_since(self, idx):
        with self.lock:
            return list(self.cuts[idx:]), len(self.cuts)

    def all_cuts(self):
        with self.lock:
            return list(self.cuts)


class AppCtx:
    def __init__(self, terminals, args):
        self.args = args
        self.terminals = list(terminals)
        self.shared = SharedState(terminals)
        self.abort = threading.Event()
        self.stop_path = args.stop_file if os.path.isabs(args.stop_file) \
            else os.path.join(SCRIPT_DIR, args.stop_file)
        self.logger = None
        self.tl = None
        self.relay = None
        self.t0 = time.time()
        # 认证抖动/环境暂停计时：暂停时段要从单轮超时预算里扣除，避免误判"超时"
        self.pause_lock = threading.Lock()
        self.paused_total = 0.0
        self._pause_mark = None
        self.auth_fail_count = 0
        self.trigger = None          # PhaseTrigger（仅 --cut-mode phase 时创建）

    # ---- 环境暂停（认证抖动等）----
    def pause_begin(self):
        with self.pause_lock:
            if self._pause_mark is None:
                self._pause_mark = time.time()

    def pause_end(self):
        with self.pause_lock:
            if self._pause_mark is not None:
                self.paused_total += time.time() - self._pause_mark
                self._pause_mark = None

    def paused_now(self):
        with self.pause_lock:
            v = self.paused_total
            if self._pause_mark is not None:
                v += time.time() - self._pause_mark
            return v

    def note_auth_fail(self, err):
        self.auth_fail_count += 1
        self.pause_begin()
        msg = "MCP 认证/登录失败（第 %d 次），暂停 %ds 后重试：%s" % (
            self.auth_fail_count, self.args.auth_retry_interval, str(err)[:160])
        if self.logger:
            self.logger("!!! " + msg)
        if self.tl:
            self.tl.add("mcp_auth_fail", count=self.auth_fail_count, err=str(err)[:200])
        if self.auth_fail_count >= self.args.auth_fail_max:
            if self.logger:
                self.logger("!!! 认证失败累计达 %d 次，全局中止（避免整段数据失效）。" % self.args.auth_fail_max)
            self.abort.set()

    def stop_requested(self):
        if self.abort.is_set():
            return True
        if os.path.exists(self.stop_path):
            self.logger("检测到急停文件 %s，停止后续动作。" % self.stop_path)
            self.abort.set()
            return True
        if self.args.max_hours > 0 and (time.time() - self.t0) > self.args.max_hours * 3600:
            self.logger("已达总时长上限 %sh，停止。" % self.args.max_hours)
            self.abort.set()
            return True
        return False


# ---------------------------------------------------------------------------
# 断电动作（两种模式共用）：一次打掉所有在测设备（只有一组继电器，必然共享）
# ---------------------------------------------------------------------------
def perform_cut(ctx, seq, elig, trigger=None, prev_end=None):
    """执行一次全局断电并登记事件。返回 (cut_ev, 恢复供电时刻)。

    trigger=None 表示随机定时模式；否则是定向模式的触发上下文
    {node, master, round, st}，把「谁在哪个节点上触发的」写进证据。
    """
    args = ctx.args
    log, tl, relay, shared = ctx.logger, ctx.tl, ctx.relay, ctx.shared

    snap = shared.snapshot()
    with shared.lock:
        online_before = dict(shared.online)

    # ---- 切电前抓快照：这是「断电瞬间各台在干什么」的唯一证据 ----
    dev_desc = []
    for t in ctx.terminals:
        s = snap.get(t) or {}
        dev_desc.append("%s[%s 轮%s %ss]" % (
            t, PHASE_CN.get(s.get("phase", "unknown"), "?"),
            s.get("round", "?"), s.get("round_elapsed_s", "?")))
    tail = ""
    if trigger:
        tail = " <<< 定向触发: %s 在 %s 节点(轮 %s)" % (
            trigger.get("master"), trigger.get("node"), trigger.get("round"))
    log(">>> 第 %d 次断电 | 切电瞬间: %s%s" % (seq, " | ".join(dev_desc), tail))

    cut_ev = {
        "seq": seq, "scheduled_at": now_str(),
        "interval_from_prev_s": None if not prev_end else int(time.time() - prev_end),
        "duration_cfg": args.cut_duration,
        "devices_at_cut": {t: (snap.get(t) or {}) for t in ctx.terminals},
        "online_before": online_before,
        "cut_mode": args.cut_mode,
    }
    if trigger:
        cut_ev.update({"trigger_node": trigger.get("node"),
                       "trigger_master": trigger.get("master"),
                       "trigger_round": trigger.get("round"),
                       "trigger_status": trigger.get("st") or {}})

    tl.add("cut_begin", seq=seq, duration=args.cut_duration, cut_mode=args.cut_mode,
           trigger_node=(trigger or {}).get("node"),
           trigger_master=(trigger or {}).get("master"),
           devices={t: {k: (snap.get(t) or {}).get(k) for k in ("phase", "state", "state_name", "progress", "round", "round_elapsed_s", "plan")}
                    for t in ctx.terminals})

    # ---- 断电 ----
    t_off0 = time.time()
    ok, err, steps = relay.power_off()
    t_off1 = time.time()
    ch_states, cerr = relay.read_states()
    if not ok:
        log("  !!! 断电失败：%s" % err)
    else:
        log("  已断电（耗时 %.2fs，通道状态 %s）" % (t_off1 - t_off0, ch_states))
    cut_ev.update({"power_off_at": now_str(), "power_off_ok": ok, "power_off_err": err,
                   "steps_off": steps, "ch_states_off": ch_states})
    tl.add("power_off", seq=seq, ok=ok, err=err, ch_states=ch_states, cost_s=round(t_off1 - t_off0, 3))

    # ---- 保持断电 ----
    held = 0.0
    while held < args.cut_duration:
        if ctx.abort.is_set():
            break
        time.sleep(min(1.0, args.cut_duration - held))
        held = time.time() - t_off1

    # ---- 恢复供电 ----
    t_on0 = time.time()
    ok2, err2, steps2 = relay.power_on()
    t_on1 = time.time()
    ch_states2, _ = relay.read_states()
    if not ok2:
        log("  !!! 恢复供电失败：%s（请立即人工处理）" % err2)
    else:
        log("  已恢复供电（实际断电 %.1fs，通道状态 %s）" % (t_on1 - t_off1, ch_states2))
    cut_ev.update({"power_on_at": now_str(), "power_on_ok": ok2, "power_on_err": err2,
                   "steps_on": steps2, "ch_states_on": ch_states2,
                   "off_duration_s": round(t_on1 - t_off1, 2)})
    tl.add("power_on", seq=seq, ok=ok2, err=err2, ch_states=ch_states2,
           actual_off_s=round(t_on1 - t_off1, 2))
    cut_ev["eligible"] = elig
    shared.add_cut(cut_ev)
    if elig:
        shared.note_cut_used(elig)
    return cut_ev, t_on1


class PhaseTrigger:
    """定向断电触发器（--cut-mode phase）：按升级节点触发，而不是按随机时钟。

    设计约束（2026-09-22 与用户逐条确认）：
    1. **trigger master 制**：一次断电只由一个设备「负责」。只有一组继电器，
       断电必然同时打掉三台；三台进度不同步，若「谁先命中谁触发」，另两台会被
       切在无关阶段，归因被污染（AT_RP3 已踩过这个坑）。非 master 的节点命中
       只记 phase_seen，作为连带样本。
    2. **节点打标去重**：(term, round, node) 打标，同一轮同一节点最多触发一次。
       实测「下载中断电后重启仍处于下载中」，只靠次数配额会出现
       「断下载 -> 重启 -> 又下载 -> 又断」的死循环，把这一轮彻底打死。
    3. **最小间隔护栏**：两次断电至少间隔 --min-cut-gap 秒。设备终态收敛需要连续供电
       10~15 分钟，连击会把任务拖成不终态的僵尸。护栏内命中则放弃并记 cut_skipped，
       **绝不排队补切**。
    4. **master 顺延**：每执行一次断电后轮到下一台，保证三台公平拿到节点命中样本。
    """

    def __init__(self, ctx):
        self.ctx = ctx
        self.lock = threading.Lock()
        self.idx = 0
        self.seq = 0
        self.last_end = 0.0
        self.done = {}        # (term, round, node) -> 触发时刻
        self.seen = {}        # (term, round, node) -> 首次观测时刻（含非 master）

    def master(self):
        terms = [t for t in self.ctx.terminals if not self.ctx.shared.is_lost(t)]
        if not terms:
            return None
        return terms[self.idx % len(terms)]

    def is_master(self, term):
        return term == self.master()

    def note_seen(self, term, node, rnd, st):
        """记录节点命中（无论是否 master）。返回 True 表示首次观测到。"""
        key = (term, rnd, node)
        with self.lock:
            if key in self.seen:
                return False
            self.seen[key] = now_str()
        self.ctx.tl.add("phase_seen", terminal=term, round=rnd, node=node,
                        state=st.get("state"), progress=st.get("progress"),
                        fail_reason=st.get("fail_reason") or "",
                        master=self.master(), fired=(term == self.master()))
        return True

    def try_fire(self, term, node, rnd, st):
        """worker 轮询命中节点时调用。返回 True 表示本次真的执行了断电。"""
        args = self.ctx.args
        tl, log = self.ctx.tl, self.ctx.logger
        # 节点命中证据先记录（幂等）：无论是否 master 都要留下「此处到过 target 节点」
        # 的见证，非 master 的记为连带样本，报告据此区分命中样本与连带样本。
        self.note_seen(term, node, rnd, st)
        if node not in args.cut_node_set:
            return False
        if node not in target_nodes(args, rnd):
            return False                       # 本轮目标不是这个节点（按轮交替）
        if self.ctx.abort.is_set() or self.ctx.shared.is_lost(term):
            return False
        if term != self.master():
            return False                       # 非 master：只做见证，不触发
        key = (term, rnd, node)
        if key in self.done:
            return False
        gap = time.time() - self.last_end
        if self.last_end and gap < args.min_cut_gap:
            self.done[key] = now_str()
            tl.add("cut_skipped", terminal=term, round=rnd, node=node,
                   reason="min_gap", gap_s=int(gap), need_s=args.min_cut_gap)
            log("  [%s] R%d 命中 %s 节点，但距上次断电仅 %ds(<%ds) -> 放弃本次，不排队补切"
                % (term, rnd, node, gap, args.min_cut_gap))
            return False
        with self.lock:
            if key in self.done:
                return False
            if args.max_cuts > 0 and self.seq >= args.max_cuts:
                return False
            self.done[key] = now_str()
            self.seq += 1
            trig = {"node": node, "master": term, "round": rnd,
                    "st": {k: st.get(k) for k in ("state", "state_name", "progress", "fail_reason")}}
            log("  [%s] R%d 命中 %s 节点 -> 触发第 %d 次定向断电" % (term, rnd, node, self.seq))
            tl.add("phase_fire", terminal=term, round=rnd, node=node, seq=self.seq,
                   state=st.get("state"), progress=st.get("progress"),
                   fail_reason=st.get("fail_reason") or "")
            _ev, t_end = perform_cut(self.ctx, self.seq, [term], trigger=trig,
                                     prev_end=self.last_end or None)
            self.last_end = t_end
            return True

    def rotate(self):
        args = self.ctx.args
        if not args.trigger_rotate:
            return
        with self.lock:
            before = self.master()
            self.idx += 1
            after = self.master()
        self.ctx.logger("  trigger master 顺延: %s -> %s" % (before, after))

    def summary(self):
        with self.lock:
            by_node = {}
            for (_t, _r, n) in self.done:
                by_node[n] = by_node.get(n, 0) + 1
            return {"seq": self.seq, "by_node": by_node,
                    "seen": len(self.seen), "fired": len(self.done)}


def poll_interval_now(ctx, term, rnd, plan_started_at, flash_seen):
    """动态轮询间隔。

    phase 模式下只有 trigger master 提速，且只在「下发后 -> 进入 filecheck 之前、
    且本轮目标节点是 download」这一段 —— 那是唯一需要高分辨率的窗口（实测仅 13.5s）。
    三台全程 5s 轮询会打满 MCP 单实例串行通道：实测单次查询中位 2.4s，
    3 台 x 580s/5s x 2.4s = 835s > 580s（144% 饱和），必然退化成约 8.7s 实际间隔，
    照样踩不住窗口。
    """
    args = ctx.args
    if args.cut_mode != "phase" or ctx.trigger is None:
        return args.poll_interval
    if flash_seen or not ctx.trigger.is_master(term):
        return args.poll_interval
    if "download" not in target_nodes(args, rnd):
        return args.poll_interval
    if plan_started_at and (time.time() - plan_started_at) >= args.fast_poll_from:
        return args.fast_poll_interval
    return args.poll_interval


# ---------------------------------------------------------------------------
# 断电调度器：与阶段解耦的随机定时触发（全局事件，一次打掉所有在测设备）
# ---------------------------------------------------------------------------
def power_cut_scheduler(ctx):
    args = ctx.args
    log, tl, relay, shared = ctx.logger, ctx.tl, ctx.relay, ctx.shared

    # 首次触发起点：显式指定或随机
    if args.first_cut_after >= 0:
        first = args.first_cut_after
    else:
        first = random.uniform(args.cut_interval_min, args.cut_interval_max)
    next_at = time.time() + first
    seq = 0
    postponed = 0
    cut_ev_prev_end = None
    log("断电调度已启动：首次 %.0fs 后 | 触发间隔 U(%d,%d)s | 每次 %ds | 通道 ACC=%d 主电=%d"
        % (first, args.cut_interval_min, args.cut_interval_max, args.cut_duration,
           relay.acc_ch, relay.main_ch))
    log("断电准入规则：每设备每轮最多 %d 次 | 轮次开跑满 %ds 才可切 | 只对进行中轮次下手（%s）"
        % (args.max_cuts_per_round, args.cut_min_round_elapsed,
           "开启" if args.cut_only_when_active else "关闭"))
    tl.add("scheduler_start", first_after_s=int(first),
           interval_min=args.cut_interval_min, interval_max=args.cut_interval_max,
           duration=args.cut_duration, acc_ch=relay.acc_ch, main_ch=relay.main_ch,
           max_per_round=args.max_cuts_per_round, min_round_elapsed=args.cut_min_round_elapsed,
           only_when_active=args.cut_only_when_active)

    while not ctx.abort.is_set():
        remain = next_at - time.time()
        while remain > 0:
            if ctx.stop_requested():
                return
            time.sleep(min(2.0, remain))
            remain = next_at - time.time()
        if ctx.stop_requested():
            return
        if args.max_cuts > 0 and seq >= args.max_cuts:
            log("断电次数已达上限 %d，调度器退出。" % args.max_cuts)
            tl.add("scheduler_stop", reason="max_cuts", count=seq)
            return
        if all(shared.is_lost(t) for t in ctx.terminals):
            log("所有设备均已失联，断电调度器退出。")
            tl.add("scheduler_stop", reason="all_lost")
            return

        # ---- 合格性闸门：没有设备处在"值得打"的窗口就推迟，不空放无用的一次断电 ----
        elig = shared.eligible_for_cut(ctx.terminals, args.cut_min_round_elapsed,
                                       args.max_cuts_per_round, args.round_timeout)
        if args.cut_only_when_active and not elig:
            # 推迟不打招呼会给日志刷屏：只在首次和之后每 10 次提示一次
            if postponed % 10 == 0:
                log("  到达断电时点，但无设备处于可断电窗口（均未开轮/退让中/已卡死/本轮配额已用尽）-> 每 %ds 复检"
                    % args.cut_recheck_interval)
                tl.add("cut_postponed", reason="no_eligible_device",
                       recheck_s=args.cut_recheck_interval, postponed=postponed + 1)
            next_at = time.time() + args.cut_recheck_interval
            postponed += 1
            continue
        if postponed:
            log("  已推迟 %d 次，%d 台设备进入可断电窗口 -> 本轮触发" % (postponed, len(elig)))
        postponed = 0

        seq += 1
        cut_ev, cut_ev_prev_end = perform_cut(ctx, seq, elig, prev_end=cut_ev_prev_end)

        nxt = random.uniform(args.cut_interval_min, args.cut_interval_max)
        next_at = time.time() + nxt
        log("  下次断电 %.0fs 后（约 %s）" % (nxt, datetime.fromtimestamp(next_at).strftime("%H:%M:%S")))
        tl.add("cut_end", seq=seq, next_in_s=int(nxt),
               next_at=datetime.fromtimestamp(next_at).strftime("%Y-%m-%d %H:%M:%S"))


# ---------------------------------------------------------------------------
# 在线状态观察者：记录「掉线/回线」事件与耗时（间接验证上下电是否真的生效）
# ---------------------------------------------------------------------------
def online_watcher(ctx):
    args = ctx.args
    log, tl, shared = ctx.logger, ctx.tl, ctx.shared
    prev = {}
    t_offline = {}
    while not ctx.abort.is_set():
        for t in ctx.terminals:
            if shared.is_lost(t):
                continue
            online, txt = mcp_online(ctx, t)
            if online is None:
                continue  # 查询失败不改变判定
            old = prev.get(t)
            if old is None:
                prev[t] = online
                with shared.lock:
                    shared.online[t] = online
                tl.add("online_sample", terminal=t, online=online, text=txt)
                continue
            if online != old:
                prev[t] = online
                with shared.lock:
                    shared.online[t] = online
                if not online:
                    t_offline[t] = time.time()
                    log("  [%s] 已离线" % t)
                    tl.add("online_lost", terminal=t, text=txt)
                else:
                    cost = int(time.time() - t_offline.pop(t, time.time()))
                    log("  [%s] 已回线（离线 %ds）" % (t, cost))
                    tl.add("online_gain", terminal=t, text=txt, offline_s=cost)

            # 持续离线判定：不看当前是否在轮次内，只要「连续」离线达到阈值即判异常。
            # 判异常的设备立刻停测（worker 下一轮开头会 break），其余设备继续。
            t0 = t_offline.get(t)
            if t0 is not None and online is False:
                held = int(time.time() - t0)
                if held >= args.lost_threshold:
                    why = "连续离线 %ds（阈值 %ds）未回线" % (held, args.lost_threshold)
                    _mark_lost(ctx, t, why)
                    log("  [%s] !!! 判定异常：%s —— 该设备停止后续测试，等待排查" % (t, why))
                    t_offline.pop(t, None)
        # 固定节拍等待，同时响应停止
        for _ in range(int(max(1, args.online_interval))):
            if ctx.abort.is_set():
                return
            time.sleep(1)
    return


# ---------------------------------------------------------------------------
# 单设备 worker：独立跑 N 轮
# ---------------------------------------------------------------------------
def ensure_online(ctx, term, timeout, tag=""):
    """等到设备在线。返回 (ok, waited_s, last_txt)。"""
    t0 = time.time()
    online, txt = mcp_online(ctx, term)
    n = 0
    while online is not True and (time.time() - t0) < timeout and not ctx.abort.is_set():
        if ctx.stop_requested():
            break
        n += 1
        if n % 4 == 1:
            ctx.logger("  [%s] %s等待回线 %ds（当前 %s）..." % (term, tag, int(time.time() - t0), txt or "未知"))
        time.sleep(ctx.args.poll_interval)
        online, txt = mcp_online(ctx, term)
    return (online is True), int(time.time() - t0), txt


def worker_guard(ctx, term):
    """worker 线程兜底：任何未捕获异常都要留下日志与时间线，绝不允许静默退出。"""
    try:
        return device_worker(ctx, term)
    except Exception:  # noqa: BLE001
        import traceback
        tb = traceback.format_exc()
        last = tb.strip().splitlines()[-1] if tb.strip() else "unknown"
        ctx.logger("!!! [%s] worker 线程异常，已捕获：%s" % (term, last))
        ctx.logger(tb)
        ctx.tl.add("worker_crash", terminal=term, error=last)
        return [{
            "round": 0, "terminal": term, "plan": "", "result": "worker异常",
            "state": "", "state_name": "", "fail_reason": "", "version": "",
            "duration_s": "", "cut_cnt": 0, "cut_phase": "", "submit_at": "",
            "start_at": "", "terminal_at": "", "backoff_s": 0,
            "online_before_round": "", "recover_s": "", "note": last,
        }]


def pick_pkg(args, term, rnd):
    """按 --ab-alternate-by 决定本轮用 A 包还是 B 包，返回 (firmware, version)。

    device（默认）：按设备交替 —— 第 1,3,5... 台全程 A 包，第 2,4,6... 台全程 B 包。
                   同一台设备每轮版本都在变化（A 刷完刷 B，再刷 A），符合"避免刷同一版本"的诉求。
    round：按轮次交替 —— 奇数轮 A 包、偶数轮 B 包，三台同轮用同一个包。
    未配 B 包时全程 A 包。
    """
    if not getattr(args, "pkgB_firmware", ""):
        return args.pkgA_firmware, args.pkgA_version
    if args.ab_alternate_by == "round":
        use_b = (rnd % 2 == 0)
    else:
        try:
            idx = list(args.terminals).index(term)
        except (ValueError, AttributeError):
            idx = 0
        use_b = (idx % 2 == 1)
    if use_b:
        return args.pkgB_firmware, (args.pkgB_version or args.pkgA_version)
    return args.pkgA_firmware, args.pkgA_version


def device_worker(ctx, term):
    args = ctx.args
    log, tl, shared = ctx.logger, ctx.tl, ctx.shared
    rows = []
    rounds_ok = rounds_fail = 0

    for rnd in range(1, args.rounds + 1):
        if ctx.stop_requested():
            break
        if shared.is_lost(term):
            log("  [%s] 已判定失联，跳过剩余轮次。" % term)
            break

        # 计划名必须带终端号：OBOS 的 start 是按「计划名」启动的，
        # 三台共用同名计划会导致后启动的报「当前计划已启动，请勿重复启动」。
        plan = "%s_%s_R%d" % (args.plan_prefix, term, rnd)

        # A/B 交替：奇数轮用 A 包，偶数轮用 B 包（未配 B 包则全程 A 包）。
        # 目的：每轮版本都在变，避免"版本未变被跳过"，也更贴近真实升级场景。
        fw, ver = pick_pkg(args, term, rnd)
        pkg = "%s(%s)" % (fw, ver)

        row = {
            "round": rnd, "terminal": term, "plan": plan,
            "result": "未执行", "state": "", "state_name": "", "fail_reason": "",
            "version": "", "pkg": pkg, "duration_s": "", "cut_cnt": 0, "cut_phase": "",
            "submit_at": "", "start_at": "", "terminal_at": "", "backoff_s": 0,
            "online_before_round": "", "recover_s": "", "note": "",
        }
        cut_idx0 = shared.cut_count()
        t_round0 = time.time()
        shared.set_latest(term, round=rnd, plan=plan, phase="pending",
                          state=None, state_name=None, progress=None, round_elapsed_s=0,
                          round_state="active", cut_used=0, pkg=pkg)

        # ---- 干跑模式 ----
        if args.dry_run:
            tl.add("round_start", terminal=term, round=rnd, plan=plan, dry_run=True,
                   firmware=fw, version=ver)
            for ph in ("hangup", "download", "flash", "reboot"):
                shared.set_latest(term, phase=ph, state="2", state_name="执行中",
                                  round_elapsed_s=int(time.time() - t_round0))
                time.sleep(args.dry_phase_sleep)
            row.update({"result": "DRY-RUN", "state": "0", "state_name": "成功",
                        "duration_s": int(time.time() - t_round0)})
            tl.add("round_end", terminal=term, round=rnd, result="DRY-RUN")
            rows.append(row)
            if rnd < args.rounds and not ctx.abort.is_set():
                end = time.time() + args.round_cooldown
                while time.time() < end and not ctx.abort.is_set():
                    if ctx.stop_requested():
                        break
                    time.sleep(1)
            continue

        # ---- 0) 开轮前确认在线 ----
        if args.skip_online_check:
            online, waited, txt = True, 0, "跳过检查"
        else:
            online, waited, txt = ensure_online(ctx, term, args.online_timeout, tag="开轮前")
        row["online_before_round"] = str(txt)
        if not online:
            row.update({"result": "离线未回", "duration_s": int(time.time() - t_round0),
                        "note": "开轮前离线且等待 %ds 未回线" % args.online_timeout})
            log("  [%s] R%d %s" % (term, rnd, row["note"]))
            tl.add("round_end", terminal=term, round=rnd, result=row["result"])
            rows.append(row)
            # 已经等够 online_timeout，超过失联阈值则判失联
            if waited >= args.lost_threshold:
                _mark_lost(ctx, term, "开轮前离线 %ds 未回线" % waited)
            continue

        tl.add("round_start", terminal=term, round=rnd, plan=plan, firmware=fw, version=ver)

        # ---- 1) 提交 + 下发 ----
        row["submit_at"] = now_str(False)
        with MCP_LOCK:
            ok, err = submit_plan(plan, fw, ver, args.mode, args.product, term)
        if not ok:
            row.update({"result": "提交失败", "duration_s": int(time.time() - t_round0),
                        "note": "提交计划失败: %s" % err})
            log("  [%s] R%d 提交失败: %s" % (term, rnd, err))
            tl.add("submit_fail", terminal=term, round=rnd, err=err)
            rows.append(row)
        else:
            tl.add("submit", terminal=term, round=rnd, plan=plan)
            row["start_at"] = now_str(False)
            with MCP_LOCK:
                ok2, err2 = start_plan(plan)
            if not ok2:
                row.update({"result": "下发失败", "duration_s": int(time.time() - t_round0),
                            "note": "下发失败: %s" % err2})
                log("  [%s] R%d 下发失败: %s" % (term, rnd, err2))
                tl.add("start_fail", terminal=term, round=rnd, err=err2)
                rows.append(row)
            else:
                log("  [%s] R%d 已下发 %s [%s]" % (term, rnd, plan, pkg))
                tl.add("start", terminal=term, round=rnd, plan=plan)

                # ---- 2) 轮询到终态 ----
                # 关键约束：一旦状态迟迟不落到「成功/失败」，**绝不再给这台设备下发新任务**。
                # 叠加下发会让平台任务与端侧状态错位，现场证据被破坏、也无法归因。
                # 所以超过 round_timeout 不是"记个超时就开下一轮"，而是转入「挂起等待」：
                # 继续盯同一个计划，直到出现终态；再超过 stuck_timeout 才判该设备异常并停测。
                plan_started_at = time.time()   # 提速窗口的计时起点（下发成功时刻）
                flash_seen = False              # 一旦见到 filecheck 就不再提速
                pause0 = ctx.paused_now()
                soft_deadline = time.time() + args.round_timeout      # 超过 = 记为「未在预期时间出终态」
                hard_deadline = soft_deadline + args.stuck_timeout    # 再超过 = 判异常，停止该设备
                last = None
                done = False
                fail_streak = 0
                stuck_noted = False
                while (time.time() < hard_deadline + (ctx.paused_now() - pause0)
                       and not ctx.abort.is_set()):
                    if ctx.stop_requested():
                        break
                    if shared.is_lost(term):
                        break
                    st = mcp_status(ctx, plan)
                    el = int(time.time() - t_round0)
                    if st.get("ok"):
                        fail_streak = 0
                        last = st
                        ph = phase_of(st)
                        shared.set_latest(term, phase=ph, state=st.get("state"),
                                          state_name=st.get("state_name"),
                                          progress=st.get("progress"), round_elapsed_s=el)
                        tl.add("status", terminal=term, round=rnd, plan=plan,
                               state=st.get("state"), state_name=st.get("state_name"),
                               progress=st.get("progress"), phase=ph,
                               fail_reason=st.get("fail_reason") or "",
                               version=st.get("version") or "")
                        log("  [%s] R%d %s %s progress=%s %s" % (
                            term, rnd, st.get("state"), st.get("state_name"), st.get("progress"),
                            KNOWN_PLACEHOLDERS.get(st.get("fail_reason") or "", st.get("fail_reason") or "-")))
                        # ---- 定向断电：命中 download / filecheck 节点 ----
                        if ctx.trigger is not None:
                            node = phase_node(st)
                            if node:
                                ctx.trigger.note_seen(term, node, rnd, st)
                                if node == "filecheck":
                                    flash_seen = True
                                try:
                                    if ctx.trigger.try_fire(term, node, rnd, st):
                                        # 断电重启后设备会重新走 download，但本轮目标节点
                                        # 已打标不会再触发，因此不重置提速标记（省 MCP 通道）。
                                        ctx.trigger.rotate()
                                except Exception as _e:  # noqa: BLE001
                                    log("  [%s] R%d 定向断电触发异常：%s" % (term, rnd, str(_e)[:160]))
                        if is_terminal(st):
                            done = True
                            break
                    else:
                        fail_streak += 1
                        if fail_streak <= 3 or fail_streak % 10 == 0:
                            log("  [%s] R%d 状态查询失败(%d 次连续): %s" % (term, rnd, fail_streak, st.get("error")))
                        tl.add("status_error", terminal=term, round=rnd, streak=fail_streak, err=st.get("error"))
                    # 设备失联检测：连续多次查不到 + 在线为离线
                    if fail_streak >= 5:
                        q_online, _txt = mcp_online(ctx, term)
                        if q_online is False and (time.time() - t_round0) > args.lost_threshold:
                            _mark_lost(ctx, term, "轮内长时间离线（>%ds）" % args.lost_threshold)
                            break
                    # 超过预期时长仍无终态：记录「卡住」，继续等待，但不下发任何新任务
                    if not stuck_noted and time.time() >= soft_deadline + (ctx.paused_now() - pause0):
                        stuck_noted = True
                        _sn = (last or {}).get("state_name") or ""
                        _ss = (last or {}).get("state") or ""
                        row["result"] = "卡住未终态"
                        row.update({"state": _ss, "state_name": _sn,
                                    "fail_reason": (last or {}).get("fail_reason") or "",
                                    "version": (last or {}).get("version") or "",
                                    "duration_s": el})
                        row["note"] = "%ds 未进终态(最后 %s %s)：不再下发新任务，继续等待" % (
                            el, _ss, _sn)
                        shared.set_round_state(term, "stuck")  # 已超期：不再对它切电
                        tl.add("plan_stuck", terminal=term, round=rnd, plan=plan,
                               state=_ss, state_name=_sn, waited_s=el,
                               stuck_timeout=args.stuck_timeout)
                        log("  !!! [%s] R%d 已等 %ds 仍无终态(%s %s) -> 不再下发新任务，最多再等 %ds" % (
                            term, rnd, el, _ss, _sn, args.stuck_timeout))
                    time.sleep(poll_interval_now(ctx, term, rnd, plan_started_at, flash_seen))

                row["terminal_at"] = now_str(False)
                if last:
                    row.update({"state": last.get("state"), "state_name": last.get("state_name") or "",
                                "fail_reason": last.get("fail_reason") or "",
                                "version": last.get("version") or ""})
                name = (last or {}).get("state_name") or ""
                waited_s = int(time.time() - t_round0)
                if any(k in name for k in SUCCESS_KEYWORDS):
                    row["result"] = "成功"
                    if stuck_noted:
                        row["note"] = "延迟终态：等待 %ds 后才成功（期间未下发新任务）" % waited_s
                elif any(k in name for k in FAIL_KEYWORDS):
                    row["result"] = "失败"
                    if stuck_noted:
                        row["note"] = "延迟终态：等待 %ds 后才失败（期间未下发新任务）" % waited_s
                elif not done:
                    _ss = (last or {}).get("state") or "-"
                    _sn = (last or {}).get("state_name") or "-"
                    if ctx.abort.is_set() and not shared.is_lost(term):
                        # 主动停止（总时长上限 / STOP 急停文件）导致本轮未等到终态。
                        # 此时任务通常仍在平台侧跑，绝不能判为设备异常或失联 ——
                        # 否则报告会把「到时收尾」误算成升级失败，污染断电归因结论。
                        row["result"] = "到时停止"
                        row["note"] = "脚本主动停止（时长上限/急停），本轮仍在平台侧 %s，未等到终态" % _sn
                        tl.add("aborted_round", terminal=term, round=rnd, plan=plan,
                               state=_ss, state_name=_sn, waited_s=waited_s)
                        log("  [%s] R%d 脚本主动停止，任务仍在平台侧(%s)，记为『到时停止』，不计异常。"
                            % (term, rnd, _sn))
                    else:
                        # 既没成功也没失败 -> 判异常，停止这台设备，保留现场等排查
                        row["result"] = "卡住停测"
                        row["note"] = "持续 %ds 无终态(最后 %s %s)，达 stuck-timeout，停止该设备待排查" % (
                            waited_s, _ss, _sn)
                        tl.add("plan_stuck_abort", terminal=term, round=rnd, plan=plan,
                               state=_ss, state_name=_sn, waited_s=waited_s)
                        log("  ### [%s] R%d 持续 %ds 无终态(%s %s) -> 停止该设备测试，等待排查 ###" % (
                            term, rnd, waited_s, _ss, _sn))
                        _mark_lost(ctx, term, "计划 %s 持续 %ds 无终态（最后 %s %s）" % (plan, waited_s, _ss, _sn))
                row["duration_s"] = waited_s
                tl.add("round_end", terminal=term, round=rnd, result=row["result"],
                       state=row["state"], state_name=row["state_name"],
                       duration_s=row["duration_s"])

        # ---- 3) 本轮断电关联 + 断电后回线确认 ----
        new_cuts, _ = shared.cuts_since(cut_idx0)
        row["cut_cnt"] = len(new_cuts)
        if new_cuts:
            phases = [PHASE_CN.get((c.get("devices_at_cut") or {}).get(term, {}).get("phase", "unknown"), "?")
                      for c in new_cuts]
            row["cut_phase"] = "/".join(phases)
            tl.add("round_cuts", terminal=term, round=rnd, cut_cnt=len(new_cuts), phases=phases)
            # 断电过的轮次：确认设备是否回线（间接证明上电生效）
            t_on = time.time()
            online, _txt = mcp_online(ctx, term)
            while online is not True and (time.time() - t_on) < args.recover_timeout and not ctx.abort.is_set():
                if ctx.stop_requested():
                    break
                time.sleep(args.poll_interval)
                online, _txt = mcp_online(ctx, term)
            cost = int(time.time() - t_on)
            if online is True:
                row["recover_s"] = cost
                log("  [%s] R%d 断电后已回线（%ds）" % (term, rnd, cost))
                tl.add("recover_ok", terminal=term, round=rnd, cost_s=cost)
            else:
                row["recover_s"] = cost
                row["note"] = (row["note"] + " | " if row["note"] else "") + \
                              "断电后 %ds 未回线" % args.recover_timeout
                log("  !!! [%s] R%d 断电后 %ds 未回线" % (term, rnd, cost))
                tl.add("recover_timeout", terminal=term, round=rnd, cost_s=cost)
                if cost >= args.lost_threshold:
                    _mark_lost(ctx, term, "断电后 %ds 未回线" % cost)

        rows.append(row)
        # 轮次已收尾 -> 非活跃；退让/冷却期间不再被断电选中
        shared.set_round_state(term, "backoff", phase="done",
                               state=row.get("state"), state_name=row.get("state_name"))
        if row["result"] == "成功":
            rounds_ok += 1
        else:
            rounds_fail += 1

        # ---- 4) 轮间节奏：失败退让 N 秒，成功小冷却 ----
        is_last = (rnd >= args.rounds)
        # 已判定失联/停测的设备：不再等待，直接收尾（也不会再有新任务）
        if shared.is_lost(term):
            log("  [%s] 已判定异常，停止后续轮次。" % term)
            break
        if not is_last and not ctx.abort.is_set():
            wait = args.fail_backoff if row["result"] != "成功" else args.round_cooldown
            if row["result"] != "成功":
                log("  [%s] R%d 结果=%s -> 退让 %ds 后开始下一轮" % (term, rnd, row["result"], wait))
            row["backoff_s"] = wait
            tl.add("round_gap", terminal=term, next_round=rnd + 1, wait_s=wait, because=row["result"])
            end = time.time() + wait
            while time.time() < end and not ctx.abort.is_set():
                if ctx.stop_requested():
                    break
                time.sleep(1)

    log("  [%s] worker 结束：成功 %d / 失败 %d" % (term, rounds_ok, rounds_fail))
    tl.add("worker_end", terminal=term, ok=rounds_ok, fail=rounds_fail)
    return rows


def _mark_lost(ctx, term, why):
    ctx.shared.mark_lost(term)
    ctx.logger("### [%s] 判定失联：%s ###" % (term, why))
    ctx.tl.add("device_lost", terminal=term, why=why)
    if ctx.args.abort_on_brick:
        ctx.logger("--- abort-on-brick 已启用：全局中止 ---")
        ctx.abort.set()


# ---------------------------------------------------------------------------
# 报告
# ---------------------------------------------------------------------------
OK_SET = ("成功", "DRY-RUN")


def write_reports(ctx, rows):
    args = ctx.args
    prefix = args.plan_prefix
    md_path = os.path.join(STATE_DIR, "rpower_%s.md" % prefix)
    csv_path = os.path.join(STATE_DIR, "rpower_%s.csv" % prefix)
    cuts_csv = os.path.join(STATE_DIR, "rpower_%s_cuts.csv" % prefix)
    log_path = os.path.join(STATE_DIR, "rpower_%s.log" % prefix)
    tl_path = os.path.join(STATE_DIR, "rpower_%s.jsonl" % prefix)

    ok = sum(1 for r in rows if r["result"] in OK_SET)
    fail = sum(1 for r in rows if r["result"] not in OK_SET)
    durs = [r["duration_s"] for r in rows if isinstance(r.get("duration_s"), int) and r["duration_s"] > 0]
    cuts = ctx.shared.all_cuts()
    recs = [r["recover_s"] for r in rows if isinstance(r.get("recover_s"), int) and r["recover_s"] > 0]

    # 归因表：切电瞬间设备所处阶段 -> 该轮结果分布
    by_phase = {}
    cut_rounds = {}
    for c in cuts:
        for term, snap in (c.get("devices_at_cut") or {}).items():
            rnd = (snap or {}).get("round")
            if rnd:
                cut_rounds[(term, rnd)] = PHASE_CN.get((snap or {}).get("phase", "unknown"), "?")
    for r in rows:
        ph = cut_rounds.get((r["terminal"], r["round"]))
        if not ph:
            continue
        d = by_phase.setdefault(ph, Counter())
        d[r["result"]] += 1

    # 每台设备统计
    per_dev = {}
    for r in rows:
        d = per_dev.setdefault(r["terminal"], {"ok": 0, "fail": 0, "cuts": 0})
        if r["result"] in OK_SET:
            d["ok"] += 1
        else:
            d["fail"] += 1
        d["cuts"] += 1 if r.get("cut_cnt") else 0

    L = [
        "# OBOS 多设备并行升级 + 随机断电压测报告", "",
        "- 设备（%d 台）: %s" % (len(args.terminals), ", ".join(args.terminals)),
        "- 固件 / 版本 / 方式: %s / %s / %s（产品名 %s）" % (args.pkgA_firmware, args.pkgA_version, args.mode, args.product),
        "- A/B 交替: %s" % (("开启 —— A=%s(%s) / B=%s(%s)（交替方式见 --ab-alternate-by）"
                            % (args.pkgA_firmware, args.pkgA_version, args.pkgB_firmware, args.pkgB_version or args.pkgA_version))
                           if args.pkgB_firmware else "关闭（全程 A 包）"),
        "- 轮次: 每台 %d 轮 | 已完成记录 %d 条 | 成功 %d | 失败 %d" % (args.rounds, len(rows), ok, fail),
        "- 断电: 实际 %d 次 | 计划间隔 U(%d,%d)s | 每次 %ds | 通道 ACC=%d 主电=%d" % (
            len(cuts), args.cut_interval_min, args.cut_interval_max, args.cut_duration,
            args.acc, args.main),
        "- 失败退让: %ds" % args.fail_backoff,
        "- 单轮耗时: min=%s max=%s avg=%s s" % (
            min(durs) if durs else "-", max(durs) if durs else "-",
            int(sum(durs) / len(durs)) if durs else "-"),
        "- 断电后回线耗时: max=%s avg=%s s" % (max(recs) if recs else "-",
                                               int(sum(recs) / len(recs)) if recs else "-"),
        "- 异常/失联设备（已停止该设备后续测试）: %s" % (
            "; ".join("%s —— %s" % (t, ctx.shared.lost_reason.get(t, "原因未记录"))
                      for t in sorted(ctx.shared.lost)) or "无"),
        "- 环境暂停: 认证抖动累计 %ds（%d 次，已从单轮超时预算中扣除）" % (
            int(ctx.paused_now()), ctx.auth_fail_count),
        "- 主日志: `%s`" % log_path,
        "- 合并时间线: `%s`" % tl_path,
        "- 生成时间: %s" % now_str(False), "",
        "## 一、分设备汇总", "",
        "| 设备 | 成功 | 失败 | 经历断电轮次 |", "| --- | --- | --- | --- |",
    ]
    for t in args.terminals:
        d = per_dev.get(t, {"ok": 0, "fail": 0, "cuts": 0})
        L.append("| %s | %d | %d | %d |" % (t, d["ok"], d["fail"], d["cuts"]))

    L += ["", "## 二、断电归因（切电瞬间设备所处阶段 → 该轮结果）", "",
          "> 这是本次压测的核心产出：判断哪个阶段断电最致命。", "",
          "| 切电时阶段 | 成功 | 失败 | 合计 | 失败率 |", "| --- | --- | --- | --- | --- |"]
    if by_phase:
        for ph, c in sorted(by_phase.items(), key=lambda kv: -sum(kv[1].values())):
            tot = sum(c.values())
            okn = sum(v for k, v in c.items() if k in OK_SET)
            f = tot - okn
            L.append("| %s | %d | %d | %d | %.0f%% |" % (ph, okn, f, tot, 100.0 * f / tot))
    else:
        L.append("| （无） | - | - | - | - |")

    L += ["", "## 三、断电事件明细", "",
          "| # | 计划间隔s | 切电时刻 | ACC/主电 | 实际断电s | 通道状态(断/上) | 切电瞬间各台阶段 |",
          "| --- | --- | --- | --- | --- | --- | --- |"]
    for c in cuts:
        devs = "; ".join("%s=%s" % (t, PHASE_CN.get((s or {}).get("phase", "unknown"), "?"))
                         for t, s in (c.get("devices_at_cut") or {}).items())
        L.append("| %s | %s | %s | %s/%s | %s | %s -> %s | %s |" % (
            c.get("seq"), c.get("interval_from_prev_s") if c.get("interval_from_prev_s") is not None else "-",
            c.get("power_off_at", ""), args.acc, args.main,
            c.get("off_duration_s", ""), c.get("ch_states_off"), c.get("ch_states_on"), devs))

    L += ["", "## 四、逐轮明细", "",
          "| 轮次 | 设备 | 本轮包 | 结果 | state | stateName | 上报版本 | 期间断电 | 切电阶段 | 耗时s | 回线s | 退让s | 备注 |",
          "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"]
    for r in sorted(rows, key=lambda x: (x["round"], x["terminal"])):
        L.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["round"], r["terminal"], r.get("pkg", "") or "-", r["result"],
            r.get("state", ""), r.get("state_name", ""),
            r.get("version", ""), r.get("cut_cnt", 0), r.get("cut_phase", "") or "-",
            r.get("duration_s", ""), r.get("recover_s", "") if r.get("recover_s") != "" else "-",
            r.get("backoff_s", 0), (r.get("note") or "").replace("|", "/")))

    with open(md_path, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")

    cols = ("round", "terminal", "plan", "pkg", "result", "state", "state_name", "fail_reason", "version",
            "cut_cnt", "cut_phase", "duration_s", "recover_s", "backoff_s",
            "online_before_round", "submit_at", "start_at", "terminal_at", "note")
    with open(csv_path, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(list(cols))
        for r in sorted(rows, key=lambda x: (x["round"], x["terminal"])):
            w.writerow([r.get(k, "") for k in cols])

    with open(cuts_csv, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(["seq", "scheduled_at", "interval_from_prev_s", "power_off_at", "power_on_at",
                    "off_duration_s", "power_off_ok", "power_on_ok", "ch_states_off", "ch_states_on",
                    "devices_at_cut", "online_before"])
        for c in cuts:
            w.writerow([c.get("seq"), c.get("scheduled_at"), c.get("interval_from_prev_s"),
                        c.get("power_off_at"), c.get("power_on_at"), c.get("off_duration_s"),
                        c.get("power_off_ok"), c.get("power_on_ok"),
                        json.dumps(c.get("ch_states_off"), ensure_ascii=False),
                        json.dumps(c.get("ch_states_on"), ensure_ascii=False),
                        json.dumps({t: {k: (s or {}).get(k) for k in ("phase", "state", "progress", "round")}
                                    for t, s in (c.get("devices_at_cut") or {}).items()}, ensure_ascii=False),
                        json.dumps(c.get("online_before"), ensure_ascii=False)])
    return md_path, csv_path, cuts_csv


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="OBOS 多设备并行升级 + 随机断电压测")
    ap.add_argument("-t", "--terminals", required=True, help="终端号，逗号分隔")
    ap.add_argument("--product", default="HQT420-S01-01", help="产品名（固件登记时使用的产品型号）")
    ap.add_argument("--firmware", "--pkgA-firmware", dest="pkgA_firmware", default="259.img",
                    help="A 包固件名（简写 --firmware）")
    ap.add_argument("--version", "--pkgA-version", dest="pkgA_version", default="259",
                    help="A 包版本号（简写 --version）")
    ap.add_argument("--pkgB-firmware", dest="pkgB_firmware", default="",
                    help="B 包固件名；留空则全程用 A 包")
    ap.add_argument("--pkgB-version", dest="pkgB_version", default="",
                    help="B 包版本号，缺省同 A 包版本")
    ap.add_argument("--ab-alternate-by", choices=("device", "round"), default="device",
                    help="A/B 交替方式：device=按设备交替(第1,3,5台用A、第2,4,6台用B)；round=按轮次交替(奇偶轮)")
    ap.add_argument("--mode", default="HTTP")
    ap.add_argument("--rounds", type=int, default=100, help="每台设备的轮次")
    ap.add_argument("--plan-prefix", default="")

    # 继电器
    ap.add_argument("--relay-url", default="http://127.0.0.1:8080")
    ap.add_argument("--acc", "--relay-acc-channel", dest="acc", type=int, default=3,
                    help="ACC 开关继电器通道（简写 --acc）")
    ap.add_argument("--main", "--relay-main-channel", dest="main", type=int, default=4,
                    help="主电开关继电器通道（简写 --main）")
    ap.add_argument("--switch-gap", type=float, default=0.4, help="两通道动作间隔秒")
    ap.add_argument("--relay-dry-run", action="store_true", help="真实升级但不动继电器")
    ap.add_argument("--dry-run", action="store_true", help="全程演练：不升级也不动继电器")
    ap.add_argument("--dry-phase-sleep", type=float, default=0.5, help="dry-run 每个模拟阶段停留秒")

    # 断电策略
    ap.add_argument("--cut-interval-min", type=int, default=300, help="距上次断电的随机间隔下限秒(默认5min)")
    ap.add_argument("--cut-interval-max", type=int, default=1200, help="距上次断电的随机间隔上限秒(默认20min)")
    ap.add_argument("--first-cut-after", type=int, default=-1, help="首次断电延迟秒；-1 表示按随机间隔")
    ap.add_argument("--cut-duration", type=int, default=10, help="断电时长秒")
    ap.add_argument("--max-cuts-per-round", type=int, default=1,
                    help="每台设备每轮最多被断电几次（默认1：一轮最多打断一次，让多数轮次能跑完整）")
    ap.add_argument("--cut-min-round-elapsed", type=int, default=120,
                    help="轮次开跑至少多少秒后才允许对这台设备断电（避免刚提交就切）")
    ap.add_argument("--cut-recheck-interval", type=int, default=30,
                    help="到达触发时点但无设备可切时，推迟多少秒复检")
    ap.add_argument("--cut-only-when-active", type=int, default=1,
                    help="1=只在有设备处于进行中轮次时才切电（默认）；0=到点即切")
    ap.add_argument("--max-cuts", type=int, default=0, help="断电总次数上限，0=不限")
    ap.add_argument("--cut-mode", choices=("random", "phase"), default="random",
                    help="random=随机定时断电(基线)；phase=按升级节点定向断电")
    ap.add_argument("--cut-nodes", default="download,filecheck",
                    help="phase 模式的目标节点，逗号分隔（download,filecheck）")
    ap.add_argument("--node-alternate", type=int, default=1,
                    help="phase 模式：目标节点是否按轮交替（1=开；关则两节点都用，会一直被 download 抢占）")
    ap.add_argument("--min-cut-gap", type=int, default=600,
                    help="两次断电之间的最小间隔秒；护栏内命中则放弃本次，不排队补切")
    ap.add_argument("--trigger-rotate", type=int, default=1,
                    help="phase 模式：每次断电后 trigger master 是否顺延给下一台（1=顺延）")
    ap.add_argument("--fast-poll-interval", type=int, default=5,
                    help="phase 模式：trigger master 在下载窗口附近的轮询间隔秒")
    ap.add_argument("--fast-poll-from", type=int, default=15,
                    help="phase 模式：下发后多少秒开始为 trigger master 提速")

    # 轮次节奏
    ap.add_argument("--fail-backoff", type=int, default=900, help="单轮失败后退让秒（默认15min）")
    ap.add_argument("--round-cooldown", type=int, default=15, help="单轮成功后到下一轮的间隔秒")
    ap.add_argument("--poll-interval", type=int, default=20, help="状态轮询间隔秒")
    ap.add_argument("--mcp-retry", type=int, default=3, help="单次 MCP 查询的重试次数")
    ap.add_argument("--auth-retry-interval", type=int, default=60, help="认证抖动后的重试间隔秒")
    ap.add_argument("--auth-fail-max", type=int, default=12, help="认证失败累计上限，超过则全局中止")
    ap.add_argument("--online-interval", type=int, default=30, help="在线状态采样间隔秒")
    ap.add_argument("--round-timeout", type=int, default=1800, help="单轮升级预期上限秒（超时后不再下发新任务，转入挂起等待）")
    ap.add_argument("--stuck-timeout", type=int, default=3600,
                    help="超过 round-timeout 后最多再等多久仍无成功/失败就判该设备异常并停测；0=一直等到 max-hours")
    ap.add_argument("--online-timeout", type=int, default=900, help="等待设备在线上限秒")
    ap.add_argument("--recover-timeout", type=int, default=900, help="断电后等待回线上限秒")
    ap.add_argument("--lost-threshold", type=int, default=1200,
                    help="连续离线多少秒判定异常并停止该设备（默认 1200s = 20min）")
    ap.add_argument("--abort-on-brick", action="store_true", help="设备失联时全局中止（默认仅停该台）")
    ap.add_argument("--skip-online-check", action="store_true", help="开轮前不检查在线（调试用）")
    ap.add_argument("--max-hours", type=float, default=30.0, help="总时长上限小时，超出即停")
    ap.add_argument("--stop-file", default="STOP")
    args = ap.parse_args()

    args.cut_node_set = [s.strip() for s in str(args.cut_nodes).split(",")
                         if s.strip() in ("download", "filecheck")]
    if args.cut_mode == "phase" and not args.cut_node_set:
        print("--cut-nodes 至少需要一个 download/filecheck", file=sys.stderr)
        return 1
    args.terminals = [t.strip() for t in args.terminals.split(",") if t.strip()]
    if not args.terminals:
        print("--terminals 不能为空", file=sys.stderr)
        return 1
    if not args.plan_prefix:
        args.plan_prefix = "AT_RPower_%s" % datetime.now().strftime("%Y%m%d_%H%M%S")

    os.makedirs(STATE_DIR, exist_ok=True)
    ctx = AppCtx(args.terminals, args)
    ctx.logger = Logger(os.path.join(STATE_DIR, "rpower_%s.log" % args.plan_prefix))
    ctx.tl = Timeline(os.path.join(STATE_DIR, "rpower_%s.jsonl" % args.plan_prefix))
    ctx.relay = RelayBank(args.relay_url, args.acc, args.main,
                          gap=args.switch_gap, dry_run=(args.relay_dry_run or args.dry_run), log=ctx.logger)
    log, tl = ctx.logger, ctx.tl

    log("=" * 78)
    log("随机断电压测开始 | 设备 %d 台: %s" % (len(args.terminals), ", ".join(args.terminals)))
    log("固件 A=%s(%s)%s | 模式 %s | 产品 %s | 每台 %d 轮" % (
        args.pkgA_firmware, args.pkgA_version,
        (" B=%s(%s) —— A/B 交替" % (args.pkgB_firmware, args.pkgB_version or args.pkgA_version))
        if args.pkgB_firmware else "",
        args.mode, args.product, args.rounds))
    if args.pkgB_firmware:
        log("A/B 交替方式: %s" % (
            "按设备（第 1,3,5... 台 A 包、第 2,4,6... 台 B 包）" if args.ab_alternate_by == "device"
            else "按轮次（奇数轮 A 包、偶数轮 B 包）"))
    log("继电器 %s | ACC=ch%d 主电=ch%d | 上电语义: ON=通电" % (
        args.relay_url, args.acc, args.main))
    if args.cut_mode == "phase":
        log("断电策略: 节点定向 | 目标节点 %s | 按轮交替 %s | 时长 %ds | 最小间隔 %ds | 上限 %s 次" % (
            ",".join(args.cut_node_set), "开" if args.node_alternate else "关",
            args.cut_duration, args.min_cut_gap, args.max_cuts if args.max_cuts else "不限"))
        log("           触发节奏: R1->%s, R2->%s, R3->%s ... | master 顺延 %s | 下载窗口提速 %ds(自 %ds 起)" % (
            target_nodes(args, 1)[0], target_nodes(args, 2)[0], target_nodes(args, 3)[0],
            "开" if args.trigger_rotate else "关", args.fast_poll_interval, args.fast_poll_from))
    else:
        log("断电策略: 每 U(%d,%d)s 随机一次 | 时长 %ds | 上限 %s 次" % (
            args.cut_interval_min, args.cut_interval_max, args.cut_duration,
            args.max_cuts if args.max_cuts else "不限"))
    log("失败退让 %ds | 单轮上限 %ds | 离线失联阈值 %ds" % (
        args.fail_backoff, args.round_timeout, args.lost_threshold))
    log("急停文件 %s | 计划前缀 %s" % (ctx.stop_path, args.plan_prefix))
    if args.dry_run:
        log("*** DRY-RUN：不升级、不动继电器 ***")
    elif args.relay_dry_run:
        log("*** RELAY-DRY-RUN：真实升级，但不切电 ***")

    # 开跑前供电自检
    st, err = ctx.relay.read_states()
    log("继电器初始状态: %s %s" % (st, err or ""))
    if st and (st.get(args.acc) != 1 or st.get(args.main) != 1):
        log("检测到通道未通电，执行初始上电 ...")
    ok, err, _ = ctx.relay.power_on()
    log("初始上电: %s" % ("OK" if ok else "失败 %s" % err))

    rows = []
    workers = []
    if args.cut_mode == "phase":
        # 定向模式：断电由 worker 在命中节点时发起，随机时钟调度器必须关闭，
        # 否则两套触发逻辑会互相打架（随机切电会污染节点归因）。
        ctx.trigger = PhaseTrigger(ctx)
        log("*** 定向断电已启用 | 初始 trigger master: %s ***" % ctx.trigger.master())
        tl.add("phase_mode_start", nodes=args.cut_node_set,
               alternate=bool(args.node_alternate), rotate=bool(args.trigger_rotate),
               min_gap=args.min_cut_gap, fast_interval=args.fast_poll_interval,
               fast_from=args.fast_poll_from, master=ctx.trigger.master())
    else:
        sched = threading.Thread(target=power_cut_scheduler, args=(ctx,), daemon=True)
        sched.start()
    watcher = threading.Thread(target=online_watcher, args=(ctx,), daemon=True)
    watcher.start()
    for term in args.terminals:
        th = threading.Thread(target=lambda tm=term: rows.extend(worker_guard(ctx, tm)), daemon=True)
        workers.append(th)
        th.start()

    # 长跑（几十小时）期间定期刷新报告：脚本自然结束前也能随时查看进展
    last_report_ts = 0.0
    try:
        while any(w.is_alive() for w in workers):
            for w in workers:
                w.join(timeout=2)
            if ctx.stop_requested():
                log("收到停止请求，等待 worker 收尾 ...")
                break
            if args.plan_prefix and (time.time() - last_report_ts) >= 180:
                last_report_ts = time.time()
                try:
                    import rpower_report
                    rpower_report.generate(args.plan_prefix, terminals=args.terminals,
                                           firmware=args.pkgA_firmware, version=args.pkgA_version)
                except Exception as _e:  # noqa: BLE001
                    log("（报告刷新失败，不影响压测：%s）" % str(_e)[:120])
        for w in workers:
            w.join(timeout=args.round_timeout + args.stuck_timeout + 180)
    except KeyboardInterrupt:
        log("\n收到中断（Ctrl+C），停止后续轮次。")
        ctx.abort.set()
    finally:
        ctx.abort.set()
        ok, err, _ = ctx.relay.power_on()
        log("退出前恢复供电: %s" % ("OK" if ok else "失败 %s —— 请人工确认！" % err))
        st, serr = ctx.relay.read_states()
        log("退出时继电器状态: %s %s" % (st, serr or ""))
        md, csvp, cutscsv = write_reports(ctx, rows)
        ok_cnt = sum(1 for r in rows if r["result"] in OK_SET)
        log("=" * 78)
        log("压测结束 | 记录 %d 条 | 成功 %d | 失败 %d | 断电 %d 次 | 失联 %s" % (
            len(rows), ok_cnt, len(rows) - ok_cnt, ctx.shared.cut_count(),
            ",".join(sorted(ctx.shared.lost)) or "无"))
        if ctx.trigger is not None:
            s = ctx.trigger.summary()
            log("定向断电统计: 触发 %d 次 %s | 节点命中观测 %d 次" % (
                s["seq"], s["by_node"], s["seen"]))
        log("报告: %s" % md)
        log("逐轮明细: %s" % csvp)
        log("断电明细: %s" % cutscsv)
        log("时间线: %s" % os.path.join(STATE_DIR, "rpower_%s.jsonl" % args.plan_prefix))
        log.close()
        tl.close()

    bad = sum(1 for r in rows if r["result"] not in OK_SET)
    return 2 if (bad or ctx.shared.lost) else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n已手动停止")
        sys.exit(0)
