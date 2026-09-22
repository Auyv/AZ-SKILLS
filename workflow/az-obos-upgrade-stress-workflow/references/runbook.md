# Runbook：参数表 / 时间线事件 / 报告字段 / 排障

配套 `SKILL.md` 与 `MANUAL.md` 使用。参数默认值取自
`scripts/obos_upgrade_stress_randpower.py` 的 argparse，改动脚本后请以 `--help` 为准。

## 1. 主脚本参数

### 目标与包

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `-t/--terminals` | 必填 | 终端号，逗号分隔 |
| `--product` | `HQT420-S01-01` | 固件登记的产品型号（跨型号登记实测可升级成功） |
| `--firmware`/`--pkgA-firmware` | `259.img` | A 包固件名 |
| `--version`/`--pkgA-version` | `259` | A 包版本号 |
| `--pkgB-firmware` | 空 | B 包固件名，空则只跑 A 包 |
| `--pkgB-version` | 空 | B 包版本号 |
| `--ab-alternate-by` | `device` | A/B 交替维度：`device`（按设备奇偶）或 `round`（按轮奇偶） |
| `--mode` | `HTTP` | 传输方式 |
| `--rounds` | `100` | 每台设备轮次 |
| `--plan-prefix` | 空 | 计划名前缀，最终名为 `<prefix>_<terminal>_R<round>` |

### 继电器

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--relay-url` | `http://127.0.0.1:8080` | 继电器总服务 |
| `--acc` | 3 | ACC 通道号（须以 `/api/status` 实测确认为准） |
| `--main` | 4 | 主电通道号（同上） |
| `--switch-gap` | 0.4 | 两通道动作间隔秒 |
| `--relay-dry-run` | off | 真实升级但不动继电器 |
| `--dry-run` | off | 全程演练：不升级也不动继电器 |
| `--dry-phase-sleep` | 0.5 | dry-run 每阶段模拟停留秒 |

### 断电调度（random 模式）

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--cut-mode` | `random` | `random` 或 `phase` |
| `--cut-interval-min`/`--cut-interval-max` | 300 / 1200 | 距上次**复电**的随机间隔秒 |
| `--first-cut-after` | -1 | 首次断电延迟秒；-1 表示走同一随机分布 |
| `--cut-duration` | 10 | 断电时长秒 |
| `--max-cuts-per-round` | 1 | 每轮断电次数配额 |
| `--cut-min-round-elapsed` | 120 | 本轮至少跑满多少秒才允许切 |
| `--cut-recheck-interval` | 30 | 闸门全不满足时的复检间隔秒 |
| `--cut-only-when-active` | 1 | 仅 active 轮次可切 |
| `--max-cuts` | 0 | 断电总次数上限，0=不限 |

### 断电调度（phase 定向模式）

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--cut-nodes` | `download,filecheck` | 目标节点集合 |
| `--node-alternate` | 1 | 按轮交替节点（R1 打 download、R2 打 filecheck） |
| `--min-cut-gap` | 600 | 两次断电最小间隔秒（护栏，优先于覆盖率） |
| `--trigger-rotate` | 1 | 每次断电后 trigger master 顺延到下一台 |
| `--fast-poll-interval` | 5 | master 在下载窗口的提速轮询间隔 |
| `--fast-poll-from` | 15 | 下发后多少秒开始提速 |

### 轮次与容错

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--poll-interval` | 20 | 常规状态轮询间隔 |
| `--round-cooldown` | 15 | 单轮成功后到下一轮间隔 |
| `--fail-backoff` | 900 | 单轮失败后退让秒 |
| `--mcp-retry` | 3 | 单次 MCP 查询重试次数 |
| `--auth-retry-interval` | 60 | 认证抖动后重试间隔 |
| `--auth-fail-max` | 12 | 认证失败累计上限，超过全局中止 |
| `--online-interval` | 30 | 在线状态采样间隔 |
| `--round-timeout` | 1800 | 单轮预期上限，超时后不再下发新任务、转挂起等待 |
| `--stuck-timeout` | 3600 | 超此值记「卡住停测」并停该设备 |
| `--online-timeout` | 900 | 等待设备在线上限 |
| `--recover-timeout` | 900 | 断电后等待回线上限 |
| `--lost-threshold` | 1200 | 持续离线多久记失联异常 |
| `--abort-on-brick` | off | 设备失联时全局中止（默认仅停该台） |
| `--skip-online-check` | off | 开轮前不检查在线（仅调试） |
| `--max-hours` | 30 | 总时长上限小时 |
| `--stop-file` | `STOP` | 急停文件名 |

### 探针

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `-t/--terminal` | `0049519998` | 单台终端号 |
| `--firmware`/`--version` | `261.img` / `261` | 探针使用的包 |
| `-i/--interval` | 5.0 | 采样间隔秒 |
| `--max-wait` | 3600 | 最长等待秒 |
| `--no-submit` | off | 跳过下发，仅监控已存在计划 |

### 报告工具

| 参数 | 说明 |
| --- | --- |
| `--prefix` | 计划前缀，用于定位 `.obos_watch/rpower_<prefix>.jsonl` |
| `--jsonl` | 直接指定时间线路径 |
| `-t/--terminals` | 设备列表（补全用） |
| `--firmware`/`--version` | 报告中标注的包信息 |
| `--top` | md 中逐轮明细显示最近 N 条，0=全量 |

### 部署预检

| 参数 | 说明 |
| --- | --- |
| `--workspace` | 目标工作区绝对路径（必填） |
| `--force` | 覆盖已存在的同名脚本 |
| `--check` | 只预检，不复制 |
| `--relay-url` | 继电器总服务地址 |
| `--obos-proxy-host`/`--obos-proxy-port` | OBOS 代理端口，默认 `127.0.0.1:7897` |

## 2. 时间线事件类型（jsonl）

报告与事后复查都基于这些事件，`ts` 为毫秒级本地时间。

| 事件 | 关键字段 | 含义 |
| --- | --- | --- |
| `scheduler_start` | `cut_interval` | 调度器启动（random 模式） |
| `phase_mode_start` | `nodes`/`alternate`/`min_gap`/`fast_interval` | 定向模式启动 |
| `round_start` | `round`/`plan`/`firmware`/`version` | 单轮开始 |
| `submit` / `start` | `round`/`plan` | 提交/下发 |
| `status` | `state`/`state_name`/`progress`/`fail_reason`/`phase` | 每次轮询快照 |
| `phase_seen` | `node`/`master`/`fired` | 节点见证。**`fired` = 该终端当时是 master，不是「已触发」** |
| `phase_fire` | `node`/`seq` | 真正触发了一次定向断电 |
| `cut_begin` | `seq`/`trigger_node`/`trigger_master`/`devices` | 切电前快照（唯一的阶段证据） |
| `power_off` / `power_on` | `ch_states`/`cost_s`/`actual_off_s` | 实际上下电 |
| `cut_skipped` | `reason`/`gap_s`/`need_s` | 护栏生效放弃本次 |
| `cut_postponed` | — | 准入闸门全不过，整流推迟 |
| `online_lost` / `online_gain` | `offline_s` | 离线/回线 |
| `device_lost` | `reason` | 记录失联异常 |
| `plan_stuck_abort` | `waited_s`/`state`/`state_name` | 卡住停测 |
| `mcp_auth_fail` | `count` | 认证抖动累计 |
| `round_end` | `result`/`duration_s`/`note` | 单轮结果 |
| `post_review_correction` | `from`/`to`/`evidence` | 事后复查回填 |

## 3. `round_end.result` 取值口径

| 取值 | 性质 | 是否计入设备失败 |
| --- | --- | --- |
| `成功` | 终态成功 | 否 |
| `失败` | 平台判失败 | **是** |
| `卡住停测` | 长时间无终态 | 否（需事后复查真实终态） |
| `下发失败` / `提交失败` | MCP/代理层失败 | 否 |
| `进度结束` | `--max-hours` 到点收尾 | 否（需事后复查） |
| `进行中` | 尚未收敛 | 否 |

报告里的失败率只统计 `失败`。混算会让断电归因表得出「某阶段失败率 33%」这类假结论。

## 4. 排障

### 报告生成报 `NameError: lost_reason`
`render()` 引用了作用域外的变量，只有出现失联设备时才触发。修 `rpower_report.py` 后重生成。

### 报告里断电归因表被污染
检查 `round_end.result` 是否混入了「到点收尾」和「非设备未终结」。归因表必须拆成
成功 / 设备失败 / 非设备未终结 三列。

### 任务卡在「执行中」不下发新任务
这是**设计行为**，不是 bug。平台侧任务没终结就下发会排队堆积。
先确认是否因断电过频导致，必要时收紧 `--min-cut-gap`；已卡住的任务等它自行收敛
（实测最长 2h20m 后转成功）。

### 探针采样到大量 `query_fail`
MCP 或代理异常。单次查询中位 2.4s、最长 6.67s；超过 10s 基本就是代理抖动。
先确认 OBOS 代理端口可连接、登录态有效。

### 三台并行时轮询明显变慢
MCP 单实例串行的必然结果。按 SKILL.md 第 3 步的表核算预算，
先把三台全速 5s 降下来，改成只给 master 提速。

### 计划名冲突
计划名必须唯一且含终端号。平台**没有删除接口**，重名只能改名重传，
所以固件与计划命名要带足够区分度（如 `261_full.img`）。

## 5. 复核清单（收工前逐条过）

- [ ] 时间线 jsonl 已备份（`.jsonl.bak*`）
- [ ] 所有「卡住停测 / 到点收尾」的轮次已复查平台终态并回填
- [ ] 未终结的每一笔都归到三类原因之一（设备卡住 / 代理抖动 / 到点收尾）
- [ ] 断电归因表三列齐全，失败率只算设备失败
- [ ] 命中样本与连带样本分开统计
- [ ] 列出零覆盖阶段
- [ ] 继电器确认已复电（`GET /api/status` 看通道为 on 或 always-on）
- [ ] 无残留未终结任务
