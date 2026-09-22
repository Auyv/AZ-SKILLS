---
name: az-obos-upgrade-stress-workflow
description: |
  当用户要求对 OBOS 终端做升级稳定性压测、掉电韧性验证、升级中断恢复验证、多机并行跑轮次、
  随机断电或定点断电，或需要还原某次压测的时间线、重建压测报告时使用。
  典型触发语：压测升级、断电测试、掉电韧性、升级中断恢复、跑 N 轮、定向断电、踩下载窗口、
  出一份压测报告、OBOS 升级稳不稳。普通单次升级下发与状态跟踪不触发。
version: 1.0.0
---

# OBOS 升级压测工作流

在 OBOS 平台上对一台或多台终端反复下发升级任务，配合继电器做上下电，用「掉电」这一最恶劣的
异常环境验证升级功能的可靠性，并产出可归因的报告。

核心方法论：**先只读探针拿实测数据，再算通道容量，最后才压测**。窗口时长决定采样间隔，
采样间隔决定方案是否可行——这些数字实测出来之前不拍参数。

## 输入与边界

开始前确认以下信息；缺少会实质影响方案时才询问用户：

| 输入 | 说明 | 缺省处理 |
| --- | --- | --- |
| 终端号列表 | 逗号分隔，需平台可查在线状态 | 必填 |
| 固件包 | **必须是 full 包**，不能是 secure_ota 包 | 必填 |
| 工作区路径 | 脚本落地的目录，可用 `HQ_OBOS_WS` 环境变量 | 必填 |
| 产品型号 | 固件登记用 | `HQT420-S01-01` |
| B 包 | 与 A 包交替下发；空则只跑 A 包 | 空 |
| 轮次目标 | 每台设备 | 100 |
| 断电模式 | `random`（随机定时）/ `phase`（按升级节点定向） | `phase` |
| 继电器通道 | ACC / 主电通道号 | **必须先读 `/api/status` 确认，不假设** |

硬边界：

- **不得凭直觉修改阶段判定逻辑**，语义结论来自实测（见下节），改之前先跑探针复核。
- **不得在设备无终态时下发新任务**，平台侧会排队堆积且无取消接口。
- **不得为追求覆盖率而放松断电最小间隔护栏**，设备收敛依赖连续供电。
- 继电器操作的退出路径必须复电；异常分支不得让设备停在掉电状态。
- 工作区含可能被覆盖的无关修改时停止并说明，不顺手卷进提交。

## 前置认知：实测状态语义

这些结论两次独立验证（4159 次 20s 轮询 + 83 次 5s 采样），**不要用直觉覆盖**：

| state | stateName | progress | failReason | 含义 | 实测时长 |
| --- | --- | --- | --- | --- | --- |
| 15 | 挂起 | 0 | 空 | 已提交待派发 | 中位 67s |
| 2 | 执行中 | 0 | `begin download` / `download...` | **文件传输中** | **约 13.5s** |
| 2 | 执行中 | 100 | `file checking` | 校验 + 刷写（不可分） | 中位 482s |
| 0 | 成功 | 100 | `update success- version:Vx.y.z` | 终态成功 | — |

三条最容易用错的事实：

1. **`progress` 是二值的，只有 0 和 100**，它是状态指示器不是百分比进度条。
   任何「progress ≥ N 触发」的策略都无法落地。
2. **`failReason` 才是阶段信号**；`file checking` 内部文案恒定，无法分出校验/刷写边界。
3. **`progress=100` ≠ 完成**，设备还需连续供电 10~15 分钟才收敛终态；
   终态判定只看 `stateName` 命中 成功/失败/取消/超时/异常。

## 流程

### 第 1 步 · 部署与预检

```bash
python scripts/bootstrap_workspace.py --workspace <工作区>
```

铺设 6 个脚本 → 建 `.obos_watch` → 逐脚本编译校验 → 环境预检
（继电器 `/api/status`、OBOS 代理端口、同目录依赖齐备性）。输出 JSON，
`checks` 有 false 时先修环境再继续。

脚本之间必须同目录（不允许跨工作区 import）。`phase_probe.py` 与
`analyze_phase_probe.py` 读 `HQ_OBOS_WS`，否则取脚本所在目录。

### 第 2 步 · 只读探针（约 12 分钟，不碰继电器）

单台、单轮升级 + 5 秒高频采样，**全程不下电**：

```bash
python phase_probe.py -t <终端号> --firmware <包> --version <版本> -i 5 --max-wait 1800
python analyze_phase_probe.py --latest --terminal <终端号>
```

要拿到四个数字：`progress` 取值个数、`failReason` 序列、**下载窗口精确时长**、
单次状态查询耗时。换固件或换机型后必须重新确认。

### 第 3 步 · 算 MCP 通道容量

MCP 单实例串行，单次查询实测中位 2.4s。三台并行时单轮约 580s 的预算：

| 轮询间隔 | 3 台总查询 | MCP 占用 | 结论 |
| --- | --- | --- | --- |
| 5s | 348 | 835s = **144%** | **饱和，不可行** |
| 10s | 174 | 418s = 72% | 偏高 |
| 20s | 87 | 209s = 36% | 健康 |
| 30s | 58 | 139s = 24% | 健康 |

→ 用**窗口逼近式提速**：慢轮询打底，只给 trigger master 在预期下载窗口区间切到 5s，
见到 filecheck 即降速。三台总占用约 26~45%。

### 第 4 步 · 压测

```bash
python obos_upgrade_stress_randpower.py \
  -t <t1,t2,t3> --acc <ACC通道> --main <主电通道> \
  --firmware <A包> --version <A版> \
  --pkgB-firmware <B包> --pkgB-version <B版> --ab-alternate-by device \
  --rounds 40 --cut-mode phase --cut-nodes download,filecheck \
  --node-alternate 1 --min-cut-gap 600 --trigger-rotate 1 \
  --fast-poll-interval 5 --fast-poll-from 15 --poll-interval 20 \
  --round-timeout 2700 --stuck-timeout 1800 --fail-backoff 900 \
  --lost-threshold 1200 --max-hours 12 --plan-prefix <前缀>
```

**先跑 1 轮验证触发时序**（真的切到目标节点、且不会二次触发），再放量。
急停：在工作目录放 `STOP` 文件。

### 第 5 步 · 报告与事后复查

```bash
python rpower_report.py --prefix <前缀> -t <t1,t2,t3> --firmware <包> --version <版本>
```

时间线 jsonl 是唯一事实来源，报告随时可从它重建；不要手改 md。

`--max-hours` 到点收尾时，脚本会把处于「执行中」的轮次标为未终结。
**必须复查平台终态并回填**（历史案例：3 笔判为卡住的，实际 7~8 分钟后全部收敛成功）。
回填前备份原始时间线（`.jsonl.bak*`），追加审计事件，不篡改原始数据。

## 定向断电模式的六个机制

每一个都对应一个实测踩过的坑，**删任何一个都会出事**：

| 机制 | 参数 | 为什么不能省 |
| --- | --- | --- |
| 节点判定 | `failReason` | progress 二值，按百分比判定落不了地 |
| 节点打标去重 | `(term, round, node)` | 下载中断电后重启仍显示下载中 → 纯计数配额会造成断→重启→又断的死循环 |
| 最小间隔护栏 | `--min-cut-gap 600` | 收敛需连续供电 10~15min；护栏内命中**放弃并记 `cut_skipped`，绝不排队补切** |
| trigger master 制 | `--trigger-rotate 1` | 只有一组继电器，一次断电打掉全部设备；三台进度不同步，谁先命中谁触发会污染归因 |
| 按轮交替节点 | `--node-alternate 1` | download 在 filecheck 之前，两节点同开会被 download 永久抢占；同轮连击违反收敛约束 |
| 窗口逼近式提速 | `--fast-poll-interval 5` | 三台全速 5s 会让 MCP 通道饱和到 144% |

非 master 的节点见闻只作**连带样本**，与**命中样本**分栏统计，不混算失败率。

## 验证

| 阶段 | 验证方法 | 通过标准 |
| --- | --- | --- |
| 部署 | `bootstrap_workspace.py` 的 JSON | `checks.relay.ok` / `checks.obos_proxy.ok` 均为 true |
| 探针 | 分析脚本输出 | `progress` 仅 0/100；拿到下载窗口实测时长 |
| 首轮 | 日志出现 `命中 <node> 节点 -> 触发第 1 次定向断电` | 切电瞬间快照里 master 阶段与目标节点一致 |
| 护栏 | 出现 `cut_skipped` 且无紧跟的 `cut_begin` | 命中被放弃后未排队补切 |
| 报告 | 归因表 | 分成功率 / 设备失败 / 非设备未终结三列 |

**`phase_seen.fired` 不是「已触发断电」**，实际语义是「该终端当时是 master」。
真正触发过的唯一证据是 **`phase_fire`** 事件。复核数据时按此判定。

## 停止条件

出现以下情况时停止并报告事实与所需决定，不得猜测继续：

- 继电器不可达 → 断电动作会全部失败，只能降级为只读探针或 `--relay-dry-run`。
- OBOS 代理或登录态不可用 → 所有轮次会 `submit_fail`。
- 设备离线超过 `--lost-threshold` → 记录异常并立即停止该台，保留现场等排查。
- 轮次长时间无终态且已收紧护栏仍复现 → 停止下发，等待收敛，不叠加任务。
- 工作区含可能被覆盖的无关修改，或目标路径写入会覆盖来源不明的文件。

## 交付

- 时间线 jsonl：`.obos_watch/rpower_<前缀>.jsonl`
- 报告三件套：同前缀的 `.md`（总览）、`.csv`（逐轮明细）、`_cuts.csv`（断电明细）
- 结论必须回答四问：各设备成功/失败/未终结各多少且未终结的归因；断电归因表；
  命中样本与连带样本；**哪些阶段零覆盖**（警惕把「零覆盖」当成「零风险」）

## 仓库与发布门禁

本技能源码位于 AZ-SKILLS 仓库的 `workflow/az-obos-upgrade-stress-workflow/`。

- 修改源码必须在**独立 AI 任务分支**（`jpz_ai-*@<from>`）提交并推送，
  再创建以来源分支为合入目标的 MR；**不得直接向 `master` 等目标分支推送、快进或合并**。
- 创建或更新 MR 前先同步远端来源分支的最新提交；出现冲突必须先解决并验证。
- 远端确认 MR 已合并前不得清理本地分支。
- 登记 `manifest.yaml` 与运行 `sync_skills.py` 正式同步到各 AI 平台目录，
  均需用户明确要求；验证阶段只用 `--dry-run`。
