# AZ-SDD 公共错误码

所有技能复用本表中的语义。技能可以只声明自己可能返回的子集。

| 错误码 | 含义 | 推荐恢复动作 |
|---|---|---|
| `ERR_INVALID_INPUT` | 输入为空、类型错误、格式错误或意图不明确 | 指出字段和合法格式，只询问必要信息 |
| `ERR_PROJECT_NOT_INITIALIZED` | 项目没有 `az-sdd/` 档案 | 显式调用 `az-sdd-init` |
| `ERR_ALREADY_INITIALIZED` | 初始化目标已经存在 | 停止写入，读取现有档案或由用户决定迁移 |
| `ERR_ARTIFACT_NOT_FOUND` | 需要的 spec、arch、状态或证据不存在 | 返回缺失路径和应先执行的阶段 |
| `ERR_ARTIFACT_CONFLICT` | 多个制品给出的状态或结论冲突 | 输出冲突双方，禁止自动覆盖 |
| `ERR_INVALID_STATE` | 状态不在 `states.yaml` 中 | 修复状态字段或规则配置 |
| `ERR_TRANSITION_NOT_ALLOWED` | 请求的状态转换不在允许边中 | 返回合法后继状态 |
| `ERR_SPEC_NOT_APPROVED` | coder 没有可消费的已检查 proposal/delta | 完成 change 规范检查 |
| `ERR_ARCH_REQUIRED` | 风险门禁要求 design 但设计缺失 | 完成 change/design.md |
| `ERR_TEST_PREREQUISITE` | 应用无法启动、关闭或进入验收环境 | 修复环境或启动脚本后重试 |
| `ERR_TEST_FAILED` | 自动化测试或人工验收失败 | 保持/回退开发状态，记录失败证据 |
| `ERR_TEMPLATE_MISSING` | 必需模板不存在 | 使用内置最小模板并报告缺失路径 |
| `ERR_EXTERNAL_TOOL_MISSING` | 构建、测试或报告工具不存在 | 提供项目已有替代入口，不主动安装依赖 |
| `ERR_RULE_CONFIG` | YAML 规则缺失或无法解析 | 报告原始解析错误，不使用硬编码规则绕过 |
| `ERR_WRITE_PERMISSION` | 目标路径不可写 | 保持 dry-run 结果并提示检查权限 |
| `ERR_V4_NOT_INITIALIZED` | 请求 change 操作但项目未启用 v4 | 新项目执行 init，v3 项目执行 migrate |
| `ERR_INVALID_CHANGE_ID` | change ID 不符合约定 | 使用 `CHG-0001` 等大写字母、数字和连字符格式 |
| `ERR_CHANGE_EXISTS` | change ID 已存在 | 继续现有 change 或选择新 ID |
| `ERR_CHANGE_NOT_FOUND` | 指定 change 不存在 | 检查 ID 和 `changes/` 路径 |
| `ERR_CHANGE_NOT_READY` | 任务、证据、验收或 delta 未通过归档门禁 | 根据结构化 issues 补齐制品 |
| `ERR_BASELINE_CONFLICT` | baseline 与 change 创建时的哈希不一致 | 人工重放或重新基于最新 baseline 创建 delta |
| `ERR_DELTA_INVALID` | delta 标记、路径、哈希或内容格式无效 | 按 delta-format.md 修复 |
| `ERR_ARCHIVE_EXISTS` | 同名归档已存在 | 检查是否已经归档 |
| `ERR_ARCHIVE_FAILED` | 归档写入失败且已自动回滚 | 检查原始错误和返回的备份路径 |
| `ERR_INVALID_RISK_FACTOR` | change 使用未知风险因子 | 使用 flow.yaml 中声明的 risk_factors |
| `ERR_RISK_GATE_REQUIRED` | 风险触发的条件门禁未满足 | 根据 issues 补齐设计、测试、人工验收、回滚或范围澄清 |

错误输出必须包含：`code`、`message`、`recovery`。不要只显示“失败”。
