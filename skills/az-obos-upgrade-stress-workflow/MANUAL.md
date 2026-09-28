# MANUAL：OBOS 升级压测快速上手

## 三条命令跑完一次验证

### 1. 部署到工作区 + 预检

```bash
python scripts/bootstrap_workspace.py --workspace <工作区路径>
```

输出 JSON，重点看 `checks.relay.ok` 与 `checks.obos_proxy.ok` 是否都为 true。
`warnings` 非空时先修环境，不要带着问题开跑。

### 2. 只读探针（约 12 分钟，不动继电器）

单台、单轮、5 秒采样，拿到真实的阶段时间线：

```bash
set HQ_OBOS_WS=<工作区路径>
python phase_probe.py -t <终端号> --firmware <包> --version <版本> -i 5
python analyze_phase_probe.py --latest --terminal <终端号>
```

分析结果里必须确认两件事：

- `progress` 只有 0 和 100（若是，则一切按百分比的策略都不可行）
- 下载窗口时长（决定压测用几秒轮询；**≤5s 才打得到**，窗口实测仅约 13.5s）

### 3. 节点定向压测

```bash
python obos_upgrade_stress_randpower.py ^
  -t <t1,t2,t3> --acc 3 --main 4 ^
  --firmware 261.img --version 261 ^
  --pkgB-firmware 262.img --pkgB-version 262 --ab-alternate-by device ^
  --rounds 40 --cut-mode phase --cut-nodes download,filecheck ^
  --node-alternate 1 --min-cut-gap 600 --trigger-rotate 1 ^
  --fast-poll-interval 5 --fast-poll-from 15 --poll-interval 20 ^
  --round-timeout 2700 --stuck-timeout 1800 --fail-backoff 900 ^
  --lost-threshold 1200 --max-hours 12 --plan-prefix AT_PH1_20260922
```

`--acc 3 --main 4` 是本机当前环境的实测值，换环境必须先读 `/api/status` 确认。

先跑一轮确认触发时序正常，再放量。急停：在工作目录放 `STOP` 文件。

## 随时重建报告

```bash
python rpower_report.py --prefix AT_PH1_20260922 ^
  -t <t1,t2,t3> --firmware 261.img --version 261
```

产出三件套：`rpower_<前缀>.md`（总览）、`.csv`（逐轮明细）、`_cuts.csv`（断电明细）。

## 开跑前的硬检查

| 检查项 | 怎么做 | 不通过的后果 |
| --- | --- | --- |
| 继电器通道定义 | `GET /api/status` | ACC/主电接反，上下电顺序失效 |
| 继电器可达 | 同上 | 断电全部失败 |
| OBOS 代理与登录态 | 探针试一次状态查询 | 轮次大面积 `submit_fail` |
| 固件是 full 包 | 核对包名与上传记录 | 设备侧 `file check faild` |
| 无残留未终结任务 | 查各设备最近计划 | 平台排队堆积，压测停滞 |
| `--max-hours` 够用 | 轮数 × 单轮均耗（约 11.5min） | 到点收尾，轮次跑不满 |

## 常见误判（最容易搞错的四条）

1. **`progress=100` 不等于完成**。设备还要连续供电 10~15 分钟才收敛终态。
2. **`progress` 没有中间值**。它只有 0 和 100，别按百分比设计策略。
3. **`phase_seen.fired=true` 不是「触发了断电」**。它表示该终端当时是 trigger master。
   真正触发看 `phase_fire` 事件。
4. **「卡住停测」不是设备失败**。可能只是没等够，或 `--max-hours` 到点收尾。
   必须事后复查平台终态再下结论。

## 断电设计的取舍顺序

覆盖率不是第一优先级。按这个顺序守：

```
设备能收敛（连续供电 10~15min） > 归因不污染（只认 master 命中） > 覆盖率
```

具体体现为：`--min-cut-gap` 护栏内命中就放弃并记 `cut_skipped`，**绝不排队补切**；
非 master 的节点见闻只记 `phase_seen` 作连带样本，不驱动断电。

## 本仓库内的维护约定

- 源码位置：`skills/az-obos-upgrade-stress-workflow/`（工作流型 Skill，命名带 `-workflow` 后缀）。
- 改动走独立 AI 任务分支 + MR，不得直接推 `master`。
- `manifest.yaml` 登记与 `sync_skills.py` 正式同步需用户明确要求。
