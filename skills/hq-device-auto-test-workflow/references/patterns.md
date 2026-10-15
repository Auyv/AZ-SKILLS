# 模式与接口详解

## 1. cases.json schema

```json
{
  "cases": [
    {
      "id": "report_types_twc",
      "kind": "types",
      "duration": 60,
      "driver_args": ["--set-int", "7=8", "--set-int", "8=1", "--set-int", "9=1",
                       "--set-int", "10=1", "--set-int", "17=0"],
      "types": [3],
      "allow_warmup": true,
      "warmup_reason": "采集掩码通道 init 时快照，首个采集周期仍用默认值；vendor 禁改故豁免并记录"
    }
  ]
}
```

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| id | 是 | 用例标识，同时是证据目录名 |
| kind | 是 | 判定规则类型，见 §2 |
| duration | 是 | 用例窗口秒数；测试进程运行 duration-3s 后停止驱动 |
| driver_args | 是 | 注入给 serve 进程的激励参数（`--set-int/--set-str/--relogin-at/--diag-trace-ms` 等） |
| expected / types / expected_records / min_count ... | 按 kind | 断言参数 |
| allow_warmup + warmup_reason | 否 | 预热包豁免，必须给理由 |
| baseline_note | 否 | 基准来源/观察项记录（如中间件行为阻塞说明） |

## 2. 判定规则库（kind）

引擎维护一个 `evaluate(case, frames, ...) -> (status, detail)` 纯函数，可离线回放。

| kind | 断言 | 关键参数 | 备注 |
| --- | --- | --- | --- |
| types | 数据项类型集合符合预期 | `types`, `exact`, `allow_warmup` | 预热窗口=首个登入帧 monotonic+1500ms；窗口内非预期类型单独记录 warmup 列表 |
| consistency | 数据项字段值与基准逐字段一致 | `expected: {"2.speed": 80, ...}` | 基准取自驱动构造的样本；用第 2 个有效包核对基准本身 |
| integrity | 数据条数符合 | `expected_records`, `record_tolerance` | 基准实测校准 |
| login_sequence | 登入流水号按步长递增 | `min_count`, `step`, `modulus` | 激励用显式 Logout→Login（`--relogin-at SEC`），勿依赖链路断连感知 |
| history | 补发数据符合断点续传 | `baseline` | 需独立 baseline，未建立时 BLOCKED |
| activation | 备案/激活流程 | `expected_commands`, `expected_ack_status` | 通道级命令统计 |

判定分类语义：
- `PASS`：证据满足全部断言。
- `FAIL`：证据明确违反断言（detail 引用具体值）。
- `INCOMPLETE`：窗口内证据不足以判定（如登入次数不足）。
- `BLOCKED`：用例前置不成立（baseline 缺失、配置缺失）——用例自身问题，非被测对象问题。
- `ERROR`：环境/执行故障（设备掉线、超时、启动失败）——不得计入 FAIL。

## 3. 证据格式（frames.jsonl）

每帧一行 JSON：

```json
{"event":"frame","channel":"standard","cmd":2,"time":"2026-09-29T10:42:25",
 "monotonic_ms":18234,"seq":5,"structure":"PASS","signature":"PASS",
 "items":[{"type":2,"raw":{"speed":80,"rpm":1500,"air_mass_intake":500}}]}
```

要点：
- `monotonic_ms` 用于时序判定（预热窗口、断连时序），`time` 用于人读。
- `raw` 保留**解码后的原始字段**，不做归一化——判定层与基准直接比对。
- 除 frame 外的事件（`connect`/`injected_disconnect`/`offline_begin` 等）也写入同一文件，构成完整时间线。
- 伴生 `collector.txt`：被测服务与驱动的周期日志（会话状态、队列深度、送数计数），只做归因不做判定。

## 4. run_all 骨架要点

参考实现：`hqt420_s11` 任务仓 `s32k312_asr1803/lib/hc_pm_msg_hj1239/tester/run_all.py`。

```
main(opts):
  env_check()                      # adb devices 有目标序号；失败 exit 2
  [opts.build] build_all()         # 调 tools/*.sh（docker 容器内交叉编译）
  [opts.deploy] deploy()           # adb push 产物 + 重启被测服务 + pid 就绪等待 + 沉降
  for suite in opts.suites:        # 默认 02, 01
      run_suite(suite, timeout)    # subprocess 流式打印（逐行、秒级前缀）；超时 kill → ERROR
      parse_report(suite)          # 读套件 JSON
  write_summary()                  # 顶层 run_all-<ts>.md/.json
  exit(0 if all PASS else 1)
```

- 任何 adb 交互的子进程超时 ≥120s（高负载下被测服务重启 >50s）。
- 部署与重启的关键步骤打印 `[OK]/[ERR]` 标记。
- `--select id1,id2` 支持补跑失败用例；补跑结果与原报告分文件存放（时间戳命名），不做原地合并。

## 5. 新项目落地步骤

1. 复制参考实现的 tester 目录骨架（00_common 引擎 + NN_<suite>/cases.json + run_all.py + tools/）。
2. 改 driver 的激励注入点（对接新被测库的输入接口）与 TSP 的帧解析/校验（新协议格式）。
3. 按阶段 1 拆功能点 → 填 cases.json → 离线回放判定函数验证规则 → 真机单用例 → 全量。
4. 引擎与判定函数先在主机上用伪造证据自测（不依赖设备），再上真机。
