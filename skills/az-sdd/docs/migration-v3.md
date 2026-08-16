# AZ-SDD v3 → v4 兼容与迁移说明

## 结论

AZ-SDD v4 继续直接读取 v3 项目；迁移是显式、可预览、非破坏性的选择，不是强制步骤。

## 保持兼容的内容

- `az-sdd/INIT.md`、`STATUS.md`、`TRACK.md`；
- `SPEC.md`、`ARCH.md`、`FEEDBACK.md`；
- `spec/*.spec.md`；
- `arch/*.arch.md`；
- `coding/*.coding.md`；
- `test/`、`reports/`、`feedback/`。

中文状态名称和英文状态 ID 都能被状态脚本识别。

## 迁移步骤

1. 使用规则脚本只读检查：

       python az_sdd.py validate C:\path\to\project
       python az_sdd.py analyze C:\path\to\project

2. 先处理 critical，再评估 warning。
3. 预览迁移：

       python az_sdd.py migrate --from v3 --to v4 --project C:\path\to\project --dry-run

4. 核对复制清单、冲突和 `legacy_feature_ids`。
5. 用户确认后执行：

       python az_sdd.py migrate --from v3 --to v4 --project C:\path\to\project --apply

6. 再执行 validate/status，确认 `format=v4` 且 baseline 功能编号一致。

## 自动区块

v3 索引重建仍只维护以下标记之间的内容：

```text
<!-- AZ-SDD:AUTO:SPEC:START -->
...
<!-- AZ-SDD:AUTO:SPEC:END -->
```

标记外的人工内容不会被覆盖。写入前脚本会把原文件备份到项目 `.ai_tmp/bak/az-sdd/`。

## 迁移后的结构

- v3 `spec/*.spec.md` 被复制到 `baseline/specs/`；
- 新增 `PROJECT.md`、`.az-sdd-v4.json`、`changes/` 和 `archive/`；
- v3 原目录全部留在原位；
- validate/status/analyze 优先读取 v4 baseline。

## 不会执行的操作

- 不移动现有 spec、arch、test 或 feedback；
- 不重命名功能编号；
- 不自动改变功能状态；
- 不删除、移动或重命名 v3 制品；
- 不把未验收变更直接写入 baseline；
- 不安装 Python 依赖。

目标 baseline 已存在且内容不同会返回 `ERR_BASELINE_CONFLICT`，零写入退出。
