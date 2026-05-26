---
name: az-sdd-init
description: |-
  基于 SDD（规范驱动开发）流程，初始化项目档案目录结构。
  **触发场景**：当用户提到"初始化项目"、"创建项目档案"、"建立规范目录"、
  "开始新项目"、"给项目建档"、"az-sdd init"、"sdd init"、
  "初始化SDD"、"初始化规范目录"时激活。
  **不触发**：如果用户已经在 az-sdd/ 目录下工作，或明确说"不需要初始化"。
---

# AZ-SDD-INIT

项目初始化 - 创建 az-sdd 档案目录结构。

## 执行步骤

### Step 1: 检查项目环境

```
1. 检查当前工作目录
2. 检查 az-sdd/ 目录是否已存在
3. 检测项目类型（package.json, requirements.txt, go.mod 等）
```

### Step 2: 根据环境执行

**情况A: az-sdd/ 已存在**
```
→ 输出提示：项目已初始化
→ 显示当前项目信息
→ 询问用户：是否需要重新初始化？(是/否)
→ 如果选择"是"，执行重建流程
→ 如果选择"否"，结束
```

**情况B: az-sdd/ 不存在**
```
→ 继续执行初始化流程
```

### Step 3: 分析项目

如果有工程文件，自动提取：

| 文件类型 | 检测路径 | 提取字段 |
|---------|---------|---------|
| npm/前端 | package.json | name, description, dependencies |
| Python | requirements.txt, pyproject.toml | 依赖列表 |
| Go | go.mod | module, go version |
| Rust | Cargo.toml | name, description |
| Java/Maven | pom.xml | artifactId, description |

### Step 4: 收集项目信息

**自动提取的字段**（无需询问）：
- {PROJECT_NAME}: 从工程文件或目录名
- {TECH_STACK}: 从依赖文件
- {CURRENT_DATE}: 自动获取 (YYYY-MM-DD)

**需要询问的字段**：
- 项目描述（可选）
- 项目定位（可选）
- 目标用户（可选）

### Step 5: 创建目录结构

```
az-sdd/
├── init.md          # 项目基本信息
├── track.md         # 工作状态记录
├── SPEC.md          # 功能规范索引
├── ARCH.md          # 技术规范索引
├── FEEDBACK.md      # 用户反馈索引
├── spec/            # 功能规范目录
├── arch/            # 技术规范目录
├── test/            # 测试目录
│   ├── cases/       # 测试用例
│   ├── reports/     # 测试报告
│   └── manuals/     # 操作手册
└── feedback/        # 用户反馈目录
```

### Step 6: 初始化文件

使用 templates/init.md 和 templates/track.md 初始化文件。

### Step 7: 输出结果

```
✅ az-sdd 项目初始化完成！

📁 目录结构：
az-sdd/
├── init.md           项目基本信息
├── track.md          工作状态记录
├── SPEC.md           功能规范索引
├── ARCH.md           技术规范索引
├── FEEDBACK.md       用户反馈索引
├── spec/             功能规范目录
├── arch/             技术规范目录
├── test/             测试目录
└── feedback/         用户反馈目录

📊 项目信息：
- 项目名称：{PROJECT_NAME}
- 项目类型：{PROJECT_TYPE}
- 技术栈：{TECH_STACK}

💡 下一步建议：
1. 使用 /az-sdd-spec 编写功能规范
2. 使用 /az-sdd-arch 补充技术规范（可选）
```

## 错误处理

| 情况 | 处理方式 |
|------|---------|
| az-sdd/ 已存在 | 询问是否重建或更新 |
| 权限不足 | 提示错误，结束 |
| 磁盘空间不足 | 提示错误，结束 |
