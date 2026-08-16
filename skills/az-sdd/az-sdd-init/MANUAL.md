# az-sdd-init 手动指南

## 输入

- 项目根目录；
- 明确的 AZ-SDD 初始化意图；
- 项目名称和目标；不需要选择模式。

## 步骤

1. 确认当前目录是目标项目，不在 AZ-SKILLS 源码目录中误建档案。
2. 新项目先运行 `python az_sdd.py init --project <path> --title "项目名" --dry-run`。
3. 确认计划后改用 `--apply`，创建 v4 baseline、changes、archive 和 feedback。
4. v3 项目运行 `python az_sdd.py migrate --from v3 --to v4 --project <path> --dry-run`。
5. 核对复制清单和功能编号，再改用 `--apply`；不得删除 v3 文件。
6. 运行 validate/status，确认 format=v4。
7. 填写 PROJECT.md 的长期目标，下一步进入 `/az-sdd-spec`。

## 验证

- v4 核心文件和目录存在且可读；
- STATUS 指向 `/az-sdd-spec`；
- dry-run 时不创建任何文件。

## 常见错误

- 已初始化：返回 `ERR_ALREADY_INITIALIZED`。
- 路径冲突：返回 `ERR_ARTIFACT_CONFLICT`。
- 不可写：返回 `ERR_WRITE_PERMISSION`。
