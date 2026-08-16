# AZ-SDD v4 Delta 格式

`spec.delta.md` 使用可读 Markdown 和机器标记组合。一个文件可包含多个目标，但同一目标只能出现一次。

## MODIFIED

```markdown
## MODIFIED: specs/0010-example.spec.md

base_sha256: <64 位 SHA-256>

<!-- AZ-SDD:CONTENT:START -->
# 修改后的完整当前行为规范
<!-- AZ-SDD:CONTENT:END -->
```

## ADDED

`base_sha256` 使用 `-`，内容区提供新增后的完整规范。

## REMOVED

目标必须存在，`base_sha256` 必须与当前 baseline 一致，不包含内容区。

## 冲突规则

- ADDED 目标已存在：`ERR_BASELINE_CONFLICT`。
- MODIFIED/REMOVED 目标不存在：`ERR_ARTIFACT_NOT_FOUND`。
- 当前 baseline 哈希与 change 创建时不同：`ERR_BASELINE_CONFLICT`。
- 路径越出 `baseline/`：`ERR_INVALID_INPUT`。
- 同一目标重复或标记不完整：`ERR_DELTA_INVALID`。
