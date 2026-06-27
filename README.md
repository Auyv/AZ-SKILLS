# AZ-SKILLS 统一管理方案

> 跨 AI 工具的 Skill 管理与同步系统

---

## 📁 目录结构

```
AZ-SKILLS/
├── manifest.yaml       # 全局清单配置
├── sync_skills.py     # 同步脚本
├── README.md          # 说明文档
└── skills/            # Skill 源码（唯一真相来源，17 个）
    ├── az-sdd.v3/               # 技能族容器（含 9 个子技能，无顶层 SKILL.md）
    │   ├── az-sdd-init/         # SDD 初始化
    │   ├── az-sdd-spec/         # SDD 规范编写
    │   ├── az-sdd-arch/         # SDD 架构
    │   ├── az-sdd-coding/       # SDD 编码
    │   ├── az-sdd-test/         # SDD 人工测试
    │   ├── az-sdd-next/         # SDD 流程导航
    │   ├── az-sdd-rules/        # SDD 规范守卫
    │   ├── az-sdd-track/        # SDD 状态追踪
    │   └── az-sdd-feedback/     # SDD 反馈处理
    ├── issue-tracker/           # 问题排查记录（事后）
    ├── az-skill-creator/        # 技能创建（手动版）
    ├── auto-skill-creator/      # 技能创建（自动触发版）
    ├── az-pdf2md/               # PDF 转 Markdown
    ├── core-dump-analyzer/      # ARM core dump 分析
    ├── network-debug/           # 网络连接异常排查
    ├── docker-image-build/      # Docker 镜像制作
    ├── az-product-manager/      # 产品经理对话助手
    ├── az-knowledge-base/       # 知识库管理与检索
    ├── az-todo-tracker/         # 任务待办追踪（事前立项，2026-06-27 新增）
    ├── az-keil-build/           # Keil 嵌入式构建工具链（2026-06-23 新增）
    ├── az-c-macro2ini-config/   # C 宏定义转 ini 配置
    ├── az-mpu-local-build/      # MPU 本地构建
    ├── az-nmrj/                 # 牛马日志（nmrj 项目）
    ├── hq_tester_skills/        # 鸿泉测试技能族
    │   └── hq_adb_srtget/       # 通过 ADB 设置 SRT 属性
    ├── hq_ctp_autobuild_add_product/  # CTP 自动出包 - 新增产品
    ├── hq_ctp_autobuild_create/        # CTP 自动出包 - 创建
    └── hq_ctp_autobuild_doc/           # CTP 自动出包 - 文档
```

---

## 🎯 设计目标

1. **单一真相来源 (SSOT)**：所有 skill 源码统一存放在 `AZ-SKILLS/skills/`
2. **跨平台同步**：一键同步到 Hermes / Cline / Claude Code
3. **增量更新**：只同步有变更的 skill，避免重复复制
4. **状态追踪**：记录最后同步时间

---

## 🚀 快速开始

### 1. 查看状态

```bash
python3 sync_skills.py --list
```

### 2. 同步到所有平台

```bash
python3 sync_skills.py
```

### 3. 只同步到指定平台

```bash
python3 sync_skills.py --to cline      # 只同步到 Cline
python3 sync_skills.py --to hermes      # 只同步到 Hermes
python3 sync_skills.py --to claude_code # 只同步到 Claude Code
python3 sync_skills.py --to opencode    # 只同步到 OpenCode
python3 sync_skills.py --to openclaw    # 只同步到 OpenClaw
```

### 4. 全量同步

```bash
python3 sync_skills.py --full           # 同步所有 skills（忽略 manifest 中的平台配置）
```

### 5. 模拟运行（不实际复制）

```bash
python3 sync_skills.py --dry-run        # 先看看会同步什么
```

---

## ⚙️ manifest.yaml 配置说明

```yaml
skills:
  - name: az-sdd                    # Skill 名称
    description: 规范驱动开发流程    # 描述
    platforms:
      hermes: az-sdd               # Hermes 中使用的名称
      cline: az-sdd                # Cline 中使用的名称
      claude_code: az-sdd          # Claude Code 中使用的名称
      opencode: az-sdd             # OpenCode 中使用的名称

sync_targets:
  - name: hermes
    path: "C:/Users/jpz/.hermes/skills"
    enabled: true                   # 是否启用

sync_mode: "incremental"            # 同步模式: incremental / full / additive
```

---

## 📋 工作流程

### 新增一个 Skill

1. 在 `skills/` 下创建目录，如 `skills/my-new-skill/`
2. 在 `manifest.yaml` 的 `skills` 列表中添加配置
3. 运行 `python3 sync_skills.py` 同步到各平台

### 更新现有 Skill

1. 修改 `skills/<skill-name>/` 下的文件
2. 运行 `python3 sync_skills.py` 增量同步

### 停用一个 Skill

1. 在 `manifest.yaml` 中删除对应条目，或设置 `enabled: false`
2. 运行 `python3 sync_skills.py` 清理目标目录

---

## 🔧 依赖安装

```bash
pip install pyyaml
```

---

## 📝 注意事项

1. **Hermes 符号链接问题**：Hermes 使用符号链接，Windows 下脚本会自动用复制替代
2. **目录权限**：确保目标目录有写入权限
3. **Windows 路径**：脚本使用正斜杠 `/`，兼容 WSL 和 Windows

---

## 🤝 支持的平台

| 平台 | 路径 | 状态 |
|------|------|------|
| Hermes (奔霄) | `C:/Users/jpz/.hermes/skills` | ✓ |
| Cline | `C:/Users/jpz/.agents/skills` | ✓ |
| Claude Code | `C:/Users/jpz/.claude/skills` | ✓ |
| OpenCode | `C:/Users/jpz/.config/opencode/skills` | ✓ |
| OpenClaw | `C:/Users/jpz/.openclaw/skills` | ✓ |

---

## 🚀 在建项目

| 项目 | 说明 | 文档 |
|------|------|------|
| **Skill 商店** | 内部 Skill 上传/下载/审核/积分平台（部署在用户闲置锐龙 7 Win11 办公电脑 WSL2） | [[项目集/skill商店/01-规范/README]] |
|  └─ 功能规范 | spec-功能规范.md | [[项目集/skill商店/01-规范/spec-功能规范]] |
|  └─ 架构 | arch-技术架构.md | [[项目集/skill商店/01-规范/arch-技术架构]] |
|  └─ API 契约 | api-接口契约.md | [[项目集/skill商店/01-规范/api-接口契约]] |
|  └─ 任务拆单 | task-开发任务拆单.md | [[项目集/skill商店/01-规范/task-开发任务拆单]] |
|  └─ 验收用例 | test-验收用例.md | [[项目集/skill商店/01-规范/test-验收用例]] |

---

**文档版本**：v1.1
**更新日期**：2026-05-08
