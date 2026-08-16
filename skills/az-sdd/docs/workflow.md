# AZ-SDD v4 开发流程

## 主流程

```text
显式进入 AZ-SDD
  → init 新项目 / migrate v3 项目
  → change-init
  → proposal + spec.delta
  → clarify + checklist
  → design（风险触发时）
  → analyze
  → tasks + coder implement
  → evidence
  → tester + acceptance
  → change-validate
  → archive
  → merge baseline + 保留历史
```

## 三个空间

| 空间 | 表达内容 | 写入时机 |
|---|---|---|
| `baseline/` | 已经生效的当前行为 | 仅 archive 成功时 |
| `changes/` | 尚未生效的一次变更 | 提议、设计、实现、验收期间 |
| `archive/` | 已合入变更的完整历史 | archive 门禁通过后 |

## 质量门禁

### 进入 coder 前

- proposal 的原因、范围和非目标明确；
- delta 内容完整、目标安全、基线哈希一致；
- design_required 变更的设计已完成；
- analyze 没有 critical；
- tasks 可追溯到 delta 验收条件。

### 进入 tester 前

- 必要任务全部完成；
- 自动化测试和实现证据已写入 evidence；
- 应用可启动、关闭并进入验收环境。

### archive 前

- delta 无 TODO 或格式错误；
- tasks 没有未完成项；
- evidence 非占位；
- acceptance 明确为 `passed` 或 `通过`；
- baseline 哈希与 change 创建时一致；
- 用户确认执行 `--apply`。

## 归档事务

```text
预校验全部 delta
  → 备份 baseline + change
  → 应用 ADDED/MODIFIED/REMOVED
  → 复制完整 change 到 archive
  → 移除开放 change
  → 成功

任一步骤异常 → 自动恢复 baseline 和 change → 返回 ERR_ARCHIVE_FAILED
```

## v3 兼容

未迁移的 v3 项目继续使用 spec/arch/coding/test 状态流。迁移只复制 spec 到 baseline，不删除旧制品；迁移后的状态和分析读取 v4 baseline，旧文件仍可回看。
