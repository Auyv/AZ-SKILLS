---
name: hc-ctp-work-ready-workflow
description: |
  当用户提供本地产品配置目录/list.env，或 GitLab 分支中的配置目录链接，并要求识别配置、拉取
  MPU/MCU manifest 与 HUBS SDK、准备工作目录和可开发基线时使用。配置 profile 名可能与 manifest
  产品名不同；需要按仓库分支和 manifest 记录核实映射，不得仅凭目录名猜测。
  统一完成工作目录隔离、manifest 拉取、HUBS SDK 下载、仓库状态核验与后续开发交付门禁；
  在用户给出具体任务前不修改业务代码。
version: 2.6.0
---

> **授权边界（先于一切流程）**
>
> 本 skill 是**流程指引**，不预先授权任何 git commit / push / MR / `reset --hard` /
> force push / 远端合并等操作。所有此类操作严格遵循项目全局 Git 规则（`未获同意不得
> git commit / git push / git reset --hard / 强制推送`）以及 `az-gitlab-merge-request`
> 等既有 skill 的授权约定。
>
> - 每一个可能产生对外副作用的动作（创建/切换/推送分支、创建/更新 MR、commit、reset、
>   强制推送、删除 workspace 等）都必须**在本次会话中取得用户明确同意**后才能执行。
> - 即使获得一次同意，也**仅对该次会话内的同一次动作有效**；不形成持久默认、不构成
>   后续操作的预授权、不允许在不同任务间复用。
> - "准备流程"中所有 manifest 拉取、HUBS SDK 下载、目录清理动作同样需要用户授权；
>   仅路径校验、文件读取、变量解析等纯只读操作可直接进行。
> - 与全局 Git 规则冲突时，以全局 Git 规则为准；本 skill 不得绕过。

# 鸿泉 MPU 工程准备与交付工作流（路径驱动版）

适用于以产品 `list.env` + XML manifest 管理多个 MPU 仓库的开发任务。用户在自然语言中给出
产品配置目录绝对路径，skill 自动识别产品型号、解析 `hub_sdk` 信息、按 manifest 拉取
MPU/MCU 源码、并参考既有 build 脚本下载 HUBS SDK 预编译包，交付可验证的代码基线后等待
具体任务安排。

## 输入与边界

必要输入：**产品配置目录的本地绝对路径，或 GitLab 配置目录链接**；同时需要明确代码拉取目标
workspace（可由当前工作目录按既定优先级推导）。本地路径通常指向形如

```
<workspace_root>/<workspace>/<config_repo_dir>/config/<config_profile>
```

的目录。

路径和标识识别规则：

- `config_profile`：`config/<profile>` 中的目录名。它只是配置 profile，**不保证**是 manifest 文件名后缀。
- `manifest_product`：manifest 文件名 `<platform>/manifests/{mpu,mcu}_<manifest_product>.xml` 的产品前缀。
  仅当本地配置目录名与唯一匹配的 manifest 后缀一致时，才可把两者视为同一标识。
- `config_repo_dir`：配置 Git 仓库在 workspace 中的目录名；从 manifest 中 `name="RDprojects/<repo>"`
  的项目及其 `path` 识别，不要假定它与 `<platform>` 同名。
- `source_branch`：GitLab URL 中 `/-/tree/<branch>/config/<profile>` 的 `<branch>`。
- `project_root`：项目根，即当前登记的 CTP 产品工程根；`autobuild/` 与 `workspace/` 都挂在它下面。
- `workspace_root`：`<project_root>/workspace`，工作空间父目录。
- `workspace`：**本次任务要拉取代码的目标目录名**，识别优先级如下（关键：不要从配置路径反推）：
  1. 用户在本次会话显式指定的目录名或绝对路径；
  2. 当前会话的工作目录（若它位于 `<workspace_root>/` 之下），取其相对于 `workspace_root` 的路径段；
  3. 用户提供的本地配置路径中相对于 `workspace_root` 的最末目录段（仅作为兜底，且必须在报告中显式提示"该目录已存在内容，代码将拉入其中"）。GitLab URL 不能反推出 workspace；前两项都不可用时询问用户。
  > 配置路径所在的 workspace 与代码要拉入的 workspace **可以不是同一个**。常见情形是用户提供
  > 另一个任务 workspace 中已拉好的 `config/<config_profile>/list.env` 作为配置参考，而代码要拉进
  > 当前任务的新 workspace。两者不得混淆。
- `platform`：不依赖路径结构假设，优先由扫描 `<project_root>/autobuild/*/manifests/{mpu,mcu}_<manifest_product>.xml`
  命中得到。若 `<config_profile>` 没有同名 manifest，按“GitLab 分支与 manifest 映射”核对配置仓库项目及
  revision；只有唯一匹配时才能确定 `manifest_product` 和 `platform`。

> ⚠️ `autobuild` 是 `<workspace_root>` 的**兄弟**目录，不是它的子目录。基准路径必须写成
> `<project_root>/autobuild/...`；写成 `<workspace_root>/autobuild/...` 会因目录不存在而失败。

执行前自动校验以下要素；任一缺失或不匹配时停止并报告：

1. 配置路径可读；若为 GitLab 链接且本机无对应副本，必须先按授权流程临时检出分支，再验证 profile 下的 `list.env`。
2. `<config_repo_dir>/config/<config_profile>/list.env` 含 HUBS 产品、PCB、BOOM、软件版本键及五个 `G_PROJ_LIST_HUBS_*` 清单键。
3. `<project_root>/autobuild/<platform>/manifests/{mpu,mcu}_<manifest_product>.xml` 两份都存在且可解析；
   `manifest_product` 与 `config_profile` 不同的情况必须有唯一、可复核的分支映射证据。
4. `git_pull_xml.py` 工具可定位（优先级：本次 workspace 内 `.ai_tmp/git_pull_xml/git_pull_xml.py` → 历史 workspace 中同名路径 → 用户在本次会话提供的绝对路径 → 询问）。

完整读取当前环境提供的全局规则、工作区与目标仓库的 `AGENTS.md`。涉及分支、提交或 MR 时，再读取适用的 Git 规则。

## 准备流程

### 1. 解析路径与识别产品型号

- 对本地路径，提取末段为 `config_profile`。如果它与 manifest 后缀不一致，不得直接停止或改名；先看是否有本次会话提供的 GitLab URL 可用于分支映射。
- 对 GitLab URL，提取 GitLab 主机、项目路径、`source_branch` 和 `config_profile`。若 URL 不含完整项目/分支信息，停止并询问缺少的精确部分。
- 对本地路径，确认它确实位于 `<project_root>` 之下（`workspace/` 或 `autobuild/`）；不在则停止，避免误读别处同名目录。对 URL，核对 GitLab 主机与项目路径，不将 URL 当作本地文件路径。
- 按"输入与边界"中的优先级确定 `workspace`，并在报告中显式区分：`config_profile` / `manifest_product` / `platform` / `workspace` / 配置来源 / 目标拉取目录。
- 先尝试直接扫描 `<project_root>/autobuild/*/manifests/{mpu,mcu}_<config_profile>.xml`。同一平台必须同时命中 MPU 与 MCU 清单且无其他平台候选，才可把 profile 作为 `manifest_product`。若用户还提供了 `source_branch`，即使 profile 同名也必须核对配置仓库项目的有效 revision；不匹配时按 GitLab 分支映射规则重新唯一匹配或停止询问。

#### GitLab 分支与 manifest 映射

当用户给出 GitLab `/-/tree/<source_branch>/config/<config_profile>` 链接，且 profile 没有同名 manifest 时：

1. 在 `<project_root>/autobuild/*/manifests/` 中解析 MPU/MCU XML，查找配置仓库项目（`name` 对应 URL 的项目路径，通常为 `RDprojects/<config_repo_dir>`），并比较其有效 revision 与 `<source_branch>`。有效 revision 按项目 `revision` → 对应 remote 的 `revision` → manifest `<default>` 顺序解析。解析 URL 时按 URL 规则解码 branch segment（例如 `%2F`），但保留精确分支名。
2. 只有恰好一组 MPU/MCU manifest 同时指向该配置仓库和分支时，才将该组文件名后缀记为 `manifest_product`；配置目录名仍保留为 `config_profile`。不得只根据型号相似、旧共享 `list.env`、相邻目录或用户口头描述猜测映射。
3. 若本地 manifest 没有唯一映射，停止并报告候选项；不得自行创建 manifest 或改写现有 manifest。
4. 若该分支的仓库副本尚未在可读位置，且用户已明确授权临时检出该指定分支，可仅用 URL 对应的 GitLab 项目 clone URL 检出该仓库的指定分支到 `<workspace>/.ai_tmp/branch_probe/<config_repo_dir>/`，读取 `config/<config_profile>/list.env` 和所需 build 脚本。临时检出前确认目标路径不存在；禁止为探测而克隆整个 MPU/MCU manifest 或改用其他分支。
5. 临时检出也是写盘动作，未获授权时先询问。仅在完整拉取、SDK 下载和基线检查成功后，且用户明确授权清理时，删除精确的 `branch_probe` 目录；流程失败时保留它以便诊断。不得连带删除 `.ai_tmp/hubs-sdk/`。
6. 完成 manifest 拉取后，改用目标 workspace 内配置仓库中的 `list.env` 作为权威来源。确认它与临时检出读取的 profile 一致（可比较关键键或文件哈希）；不一致时暂停 SDK 下载并报告差异。

若本地路径不在 `<project_root>` 下，或不符合 `config/<profile>` 形态，且无法由给定 GitLab URL 明确定位时，停止并请用户确认目录布局。

### 2. 隔离工作空间

- 使用 `<workspace_root>/<workspace>/` 作为本次任务独立工作目录（即步骤 1 确定的 `workspace`，**不是**配置路径所在的那个 workspace）。
- 不复用其他任务目录，不将生成物写入共享父目录。
- 目标目录已存在时，先检查其内容与 Git 状态；存在无关文件、未提交修改或来源不明的仓库时停止并请用户决定。**任何目录覆盖、清理或重置动作都需用户在本次会话中明确同意**，禁止 skill 自动执行。
- 若目录内仅有本流程此前创建且获授权的 `.ai_tmp/branch_probe/`，可以在其余目标路径无冲突的前提下继续；不得将探测仓库误当成完整 manifest 拉取结果。
- 用户明确指定目录规则时，将其记录到适用范围内的 `AGENTS.md`；未获要求不自行创建协作文档。

### 3. 解析 list.env 与 hub_sdk 信息

#### 配置归属规则（重要）

产品配置仓库（通常为 `RDprojects/<config_repo_dir>`）本身也是 manifest 的一个 project，会被拉入
`<workspace>/<platform>/<config_repo_dir>/`。`config_profile` 是仓库中的配置目录，未必等于 `manifest_product`。
因此**同一配置 profile 可能同时存在两份 `list.env`**：

| 副本 | 路径 | 角色 |
|:---|:---|:---|
| 拉入副本 | `<workspace_root>/<workspace>/<platform>/<config_repo_dir>/config/<config_profile>/list.env` | **权威来源**，随代码版本走 |
| 共享工作区副本 | `<project_root>/autobuild/<platform>/config/<config_profile>/list.env` | 仅用于定位候选 manifest，不作为下载来源 |

规则：

- 若目标 workspace 中已存在拉入副本，**一律以它为准**读取 HUBS 信息。
- 用户提供的路径若指向共享工作区副本，仅用于预检；代码尚未拉取时可先读取它，但下载前必须复核 workspace 内副本。
- 两份副本的 `G_PROJ_HUBS_SOFT_VERSION` 或 `G_PROJ_LIST_*` 不一致时，**以拉入副本为准**，
  并在报告中显式列出差异（这会直接改变 HUBS 包名与下载 URL）。
- 绝不用共享副本的 HUBS 版本号去下载 SDK，除非已确认目标 workspace 内的副本与之相同。

#### 解析字段

读取权威 `<config_repo_dir>/config/<config_profile>/list.env`，确认以下键并记录：

- `G_PROJ_HUBS_PRODUCT_NAME`：HUBS SDK 在制品库中的产品名（例：`hqt420_s11`）。
- `G_PROJ_HUBS_PCB_VERSION`：硬件 PCB 版本（例：`r01`）。
- `G_PROJ_HUBS_BOOM_VERSION`：硬件 BOOM 版本（例：`v02`）。
- `G_PROJ_HUBS_SOFT_VERSION`：HUBS SDK 软件版本（例：`1.0.0.7`）。
- `G_PROJ_LIST_HUBS_LIB / SO / PLUG / BIN / DRV`：HUBS SDK 中各类产物清单。
- `G_PROJ_LIST_HUBS_BIN_MAP`：HUBS SDK 预编译二进制重命名映射（用于核对发布目录）。
- `G_PRODUCT_BUILD_MPU_MODEL` 与 `G_PRODUCT_BUILD_MCU_MODEL`：产品实际使用的 MPU/MCU build model。若存在且非空，下载时必须覆盖 downloader 的默认值；不能假定所有产品都是 `ag35_ceraa` / `s32k312`。

**是否需要下载 HUBS SDK** 由 `NeedHubsSdk()` 函数判定（参考该产品实际使用的 `build/mpu/build_<model>.sh` 中的同名函数；优先按 `G_PRODUCT_BUILD_MPU_MODEL` 选择）：上述五个 `G_PROJ_LIST_HUBS_*` 中任一非空即返回 0，
全部为空返回 1。返回 0 才在步骤 6 中**向用户申请**执行下载；skill 不自动开始下载。

注意：`list.env` 在 Windows 上可能带 CRLF；解析时需按行去除 `\r`，否则键值末尾残留 `\r` 会导致
HUBS URL 拼接失败、curl 请求失败。

### 4. 定位 manifest 与拉取工具

按 `manifest_product` 与步骤 1 得到的 `platform` 定位：

- MPU manifest：`<project_root>/autobuild/<platform>/manifests/mpu_<manifest_product>.xml`
- MCU manifest：`<project_root>/autobuild/<platform>/manifests/mcu_<manifest_product>.xml`

读取 manifest 的 `<default>` 节点记录默认 `<remote>` 与 `<revision>`；逐项扫描 `<project>` 节点的
`path`、`name`、`remote`、`revision`、`upstream`，作为后续核验基线。

`git_pull_xml.py` 工具按以下优先级定位：

1. 本次 workspace 内的 `.ai_tmp/git_pull_xml/git_pull_xml.py`。
2. 历史 workspace（同一 `workspace_root` 下其它任务）中同名路径。
3. 用户在本次会话提供的绝对路径。
4. 询问用户并禁止自行 clone / fetch。

定位与解析均为只读操作，可直接进行；调用 `git_pull_xml.py` 实际写盘前需获得用户授权。

### 5. 按 manifest 拉取 MPU / MCU 仓库

调用前需在本次会话取得用户对"按 manifest 拉取 MPU 与 MCU 仓库"以及"在哪个 workspace 中执行"的明确同意。建议的命令模板：

```bash
python <git_pull_xml.py> -b <workspace_root>/<workspace> -x <project_root>/autobuild/<platform>/manifests/mpu_<manifest_product>.xml  [-j <N>]
python <git_pull_xml.py> -b <workspace_root>/<workspace> -x <project_root>/autobuild/<platform>/manifests/mcu_<manifest_product>.xml  [-j <N>]
```

参数约定：

- `-b`：基础目录 = workspace 根（所有 manifest `path` 的拼接锚点）。
- `-x`：manifest 路径；优先使用本地绝对路径而非 URL，便于离线 / 未登录环境运行。
- `-j`：并发线程数，参考 `git_pull_xml.py` 内置上限 ≤ 16。
- MPU 与 MCU manifest 都包含的项目（典型为项目仓库自身），第二次调用命中已存在目录后由脚本自动按 update 分支继续；不会覆盖未提交改动。

拉取过程中：

- 任一项目失败时保留原始错误、已创建目录和已完成仓库范围；不删除 workspace 掩盖失败。
- 每次拉取完成后记录逐仓库结果（`path`、`name`、`remote`、`revision`、错误原文），分别统计成功、失败和跳过数量；不能只看脚本总退出码。
- `git_pull_xml.py` 返回非零时，默认停止并向用户报告失败清单。用户可以明确指定忽略其中哪些仓库；该授权仅覆盖清单中明确列出的仓库，不自动扩展到后续 manifest 新出现的失败。
- 用户明确忽略部分仓库后，继续流程前将这些仓库加入本次任务的忽略清单，并按 `(remote, name)` 识别跨 MPU/MCU manifest 的同一仓库，即使两份 XML 的 `path` 不同也不得再次拉取。若其余拉取结果全成功，可继续后续阶段，并在最终基线中标明不完整仓库。
- 如使用 `git_pull_xml.py --path` 过滤，必须确认工具打印的过滤后待拉取数量等于预期 allowlist 数量，并确认忽略项未被尝试。若过滤结果不符或仍拉取了忽略项，停止并报告；不得把全量重试冒充为已跳过指定仓库。
- 若 manifest 中某仓库已 pin 到 commit SHA，按 `git_pull_xml.py` 的 SHA 分支处理（fetch + checkout 到 detached HEAD），不执行 `git pull`。
- 目标工作目录已存在且为合法 Git 仓库时，脚本按 update 分支（fetch + checkout + pull）继续；不会覆盖未提交改动。
- **不要**为了"统一分支"而对已存在的本地仓库执行 `git reset --hard`、`git checkout -B`、强制推送等破坏性操作；任何此类操作都需用户额外明确同意。

### 6. 参考 build 脚本拉取 HUBS SDK

**执行前需在本次会话取得用户对"拉取 HUBS SDK 预编译包"的明确同意**，并向用户复述即将下载的 URL、目标目录与解压路径。

先从 workspace 权威 `list.env` 读取产品模型并检查对应 build 脚本，计算实际 URL、package mode、MPU/MCU build model、缓存目录和两个落位目录。把这些具体值复述给用户后再下载；若 `list.env` / build 脚本不完整，或下载脚本无法处理该模型、版本模式或凭据来源，先排除输入问题再重试，不得把一次可恢复的 `Error_CredMissing` 直接当成网络失败。

仅当步骤 3 中 `NeedHubsSdk()` 返回 0 且 MPU `<workspace>/<platform>/sdk/` 与 MCU `<workspace>/<platform>/mcu/sdk/` 两个目标都不存在时执行；若目标已存在，分别核验布局并报告，不得默认覆盖。`--force` 只在用户明确同意替换时使用。

#### 目录布局

下载与解压的中间产物落到本次 workspace 的 `.ai_tmp/hubs-sdk/` 缓存区。**最终 SDK 副本按 `build_ag35_ceraa.sh` 第 214 行同款语义铺平到两个目标**（MPU 一个、MCU 一个）：

| 角色 | 路径 | 备注 |
|:---|:---|:---|
| 缓存根 | `<workspace>/.ai_tmp/hubs-sdk/` | 本次任务独占；可手动整目录删除清理 |
| 下载缓存 | `<workspace>/.ai_tmp/hubs-sdk/downloads/` | 存放 `<package_name>.zip`，下次重复任务可复用或人工删除 |
| 解压缓存 | `<workspace>/.ai_tmp/hubs-sdk/staging_<config_profile>/sdk/` | 解压后含多个 build_model 子目录，从产品实际使用的模型中挑选铺平 |
| **MPU 落地** | `<workspace>/<platform>/sdk/` | MPU build_model 来自 `G_PRODUCT_BUILD_MPU_MODEL`；仅缺失时采用经核对的默认值 |
| **MCU 落地** | `<workspace>/<platform>/mcu/sdk/` | MCU build_model 来自 `G_PRODUCT_BUILD_MCU_MODEL`；仅缺失时采用经核对的默认值 |

staging 中的其它 build_model（如 `s32k312_boot`）**不参与铺平**，按需留在 staging 中供人工取用；脚本只处理 `--build-model` 与 `--mcu-build-model` 指定的两个。

两个落位目标的预期布局不同，校验时不要一律套用：

| 目标 | 预期存在 | 通常不存在 |
|:---|:---|:---|
| MPU `<platform>/sdk/` | `bin/` `lib/` `include/` `ko/` `env/` `tool/` `version.json` | `plug/`（除非 `G_PROJ_LIST_HUBS_PLUG` 非空） |
| MCU `<platform>/mcu/sdk/` | `bin/` `lib/` `include/` `env/` `tool/` | `ko/`、`plug/`（MCU 无内核模块概念） |

`--hubs-cache-dir <path>` 可覆盖默认缓存根，便于把缓存放到其它盘 / 外部存储。所有中间目录（含 zip 与 staging）**默认保留**，由用户人工手动清理 `.ai_tmp/` 目录；脚本**不**自动删除。

#### 包路径与凭据

HUBS 包路径规则与该产品实际使用的 `build/mpu/build_<model>.sh` 保持一致：

- 默认（latest）包名 `<hubs_product>_<pcb><boom>.zip`，URL `<base>/<hubs_product>/<pcb><boom>/<name>`。
- 固定（pinned）包名 `<hubs_product>_<pcb><boom>_<softver>.zip`，URL `<base>/<hubs_product>/<pcb><boom>/history/<name>`，其中版本取 `list.env` 的 `G_PROJ_HUBS_SOFT_VERSION`。
- 下载前检查所选 `build/mpu/build_<G_PRODUCT_BUILD_MPU_MODEL>.sh` 的包命名逻辑：若脚本明确使用 `history/<softver>`，调用 downloader 时传 `--soft-version <softver>`；若脚本取 latest，则不传。不得只因 `SOFT_VERSION` 非空就推断下载模式。
- 默认 base URL `http://172.16.1.99:8081/artifactory/sei/project`；build model 以配置和对应 build 脚本为准，`ag35_ceraa` / `s32k312` 只是 downloader 默认值。

凭据来源（仅记录来源，不在脚本中硬编码）：

1. `<list-env>` 中的 `G_PROJ_HUBS_ARTIFACTORY_CRED` 等候选键；
2. 环境变量 `HUBS_ARTIFACTORY_CRED`；
3. workspace 内配置仓库 `build/mpu/build_<model>.sh` 中 `curl -u...` 段；
4. 命令行 `--cred user:password`。

若 downloader 只会自动扫描固定脚本名（例如 `build_ag35_ceraa.sh`），而当前产品使用 `build_ag35_cetda.sh` 等变体：从与模型匹配的脚本读取凭据并仅在当前进程设置 `HUBS_ARTIFACTORY_CRED` 后重试；不得将凭据写入 skill、命令文件、日志或最终报告，也不得把原始 `curl -u...` 行打印到输出。

#### 调用模板

```bash
python <skill_root>/scripts/hcHubSdkDownload.py \
  --list-env <workspace>/<platform>/<config_repo_dir>/config/<config_profile>/list.env \
  --workspace <workspace> --platform <platform> \
  --build-model <G_PRODUCT_BUILD_MPU_MODEL> \
  --mcu-build-model <G_PRODUCT_BUILD_MCU_MODEL>
# 可选：--hubs-cache-dir <path>    覆盖默认 <workspace>/.ai_tmp/hubs-sdk
# 可选：--build-model <m>          MPU build_model；优先取 list.env 中 G_PRODUCT_BUILD_MPU_MODEL
# 可选：--mpu-target <dir>         MPU 落地目录，默认 <workspace>/<platform>/sdk
# 可选：--mcu-build-model <m>      MCU build_model；优先取 list.env 中 G_PRODUCT_BUILD_MCU_MODEL
# 可选：--mcu-target <dir>         MCU 落地目录，默认 <workspace>/<platform>/mcu/sdk
# 可选：--base-url <u>             默认 http://172.16.1.99:8081/artifactory/sei/project
# 可选：--soft-version <v>         固定到 history/ 下的 pinned 包
# 可选：--cred <user:password>     仅在其它三个来源都失效时使用
# 可选：--force                    同时替换已存在的 MPU / MCU 目标
```

#### 脚本职责（详见脚本内文档字符串）

1. 解析 `<list-env>`：要求 `G_PROJ_HUBS_PRODUCT_NAME/PCB_VERSION/BOOM_VERSION/SOFT_VERSION` 全部非空；
   复用 `NeedHubsSdk()`（五个 `G_PROJ_LIST_HUBS_*` 任一非空才下载），缺失则直接报错退出。
2. 计算 `<package_name>` / `<download_url>`；默认拉 latest；带 `--soft-version <v>` 时拉 history 下的 pinned。
3. 按上述顺序解析凭据并仅报告来源；全失败时报 `Error_CredMissing` 退出。
4. `curl -f -s -S -u <cred> -o <cache_root>/downloads/<package_name> <download_url>`；非零退出或空文件时报 `Error_Download` 退出。
5. 解压到 `<cache_root>/staging_<config_profile>/`，从 `sdk/<mpu-build-model>/` 铺平到 MPU 目标、从 `sdk/<mcu-build-model>/` 重命名为 `sdk` 后铺平到 MCU 目标；任一源子目录缺失时报 `Error_BuildModel`。
6. **不清理** `<cache_root>/downloads/<package_name>` 与 `<cache_root>/staging_<config_profile>/`；下次任务/用户手动决定是否复用或删除。

#### 退出码

| 码 | 含义 |
|:---|:---|
| `0` | 成功；打印 `credential_source`、`package_mode`、`download_url`、`mpu_target`、`mcu_target`、两侧 `sdk_layout` 等 |
| `1` | 输入错误（缺键、不需要 HUBS、MPU/MCU 目标已存在、找不到凭据、平台无法推断等） |
| `2` | 网络/解压错误 |

如脚本不可用或被用户禁用，回到内联 curl/unzip/cp 流程（同 `build_ag35_ceraa.sh`），但仍受同样的授权门禁、缓存目录布局与凭据约束。

### 7. 交付可开发基线

对本次相关仓库检查 `git status --short`、当前 HEAD / 分支与 manifest 指定 `revision`；报告任何未提交改动、拉取遗漏或分支偏差。这些检查为只读操作，可直接进行。

- 对 MPU/MCU 两份 manifest 的每个 `project path` 检查仓库是否存在、HEAD、当前分支和 `git status --short`；显式 `revision` 优先，否则按项目 remote revision，再回退到 default revision。SHA revision 与 HEAD 比较；分支 revision 与当前分支名比较。
- 分开统计 manifest 项目条目数、成功、失败、用户批准忽略数和不同仓库数；同一 `(remote, name)` 在 MPU/MCU 两份清单重复时按一个仓库报告，但保留两侧 path/revision 差异。
- 聚合父仓库可能因 manifest 管理的嵌套仓库或 SDK 落位显示 `?? app/`、`?? common/`、`?? sdk/`。逐个检查相应子仓库：如果子仓库已单独匹配 manifest 且干净，将父仓库状态说明为预期嵌套目录；其他未提交改动仍需单独报告。

- 找到并读取各仓库的 `AGENTS.md`，确认构建入口、测试方式和额外约束。
- 校验 HUBS SDK 是否成功铺平到 MPU 目标 `<workspace>/<platform>/sdk/` 与 MCU 目标 `<workspace>/<platform>/mcu/sdk/` 下；两侧布局不同，按步骤 6 的"预期布局"表分别核对，不要用同一套判据。
- 汇总 workspace、manifest、已拉取仓库、HUBS SDK 状态和异常项。
- 若仍有用户批准忽略的仓库，基线只能标为“可开发但仓库不完整”，准确列出失败/跳过仓库；不得无条件宣称所有 manifest 项目均已就绪。
- 若本次使用临时分支探测，只有在仓库拉取、SDK 下载和基线检查均完成且用户已明确授权删除后，才删除 `.ai_tmp/branch_probe/<repo>/`；保留 `.ai_tmp/hubs-sdk/` 下载缓存与 staging。

只有拉取与基线检查完成后，才向用户确认"准备好"并等待具体工作内容。

在用户给出具体任务前，不修改业务代码、产品配置、manifest、构建脚本或远端分支，也不启动无关构建。

## 收到具体开发任务后

> 每一步都需要用户在本次会话中**明确授权**；skill 不持有任何预授权；
> 授权随本次会话结束失效，不得在不同任务或不同会话之间复用。
> 与项目全局 Git 规则冲突时以全局规则为准。

1. 先识别具体文件所属仓库、原目标分支和相关 `AGENTS.md`；处理多仓库修改时维护清晰的仓库映射。所有修改动作需用户在本次会话明确授权后才能执行。
2. 优先复用用户指定的源分支或已有开放 MR；两者均不存在时，才在取得用户同意后从原目标分支创建独立任务分支实施最小修改。任务分支的创建、构建、测试或静态检查均需用户明确同意后才执行。
3. **提交与推送**：仅在获得用户本次明确同意（且明确指定 commit 内容或允许 stage 全部相关文件）后，将本仓库相关文件 commit 并推送到远端任务分支。**严禁**未经同意 commit / push / force push / `git reset --hard`。
4. **创建/更新 MR**：需要在本次会话中明确确认目标分支、源分支、标题与说明；**不得**直接向任何远端目标分支推送、快进或合并。用户说"同步"或"合并"时，**在获得本次明确同意后**调用 `az-gitlab-merge-request` 创建或更新 MR；CI 通过后仅报告状态，等待人工审核和合并。
5. 用户要求用测试分支验证 CI 时，可临时调整其明确指定的 manifest；验证 XML 与差异，除非用户明确要求，否则不提交该临时修改；任何 manifest 改动都需用户明确同意。

## 停止条件

在以下情况发生时停止并向用户报告事实与所需决定：

- 用户提供的本地配置路径不在 `<project_root>` 下或无法定位 `config/<config_profile>`；若是 GitLab URL，则 URL 缺少项目、分支或 profile 信息。
- `config_profile` 没有同名 manifest，且不能通过唯一匹配“配置仓库项目 + source_branch revision”确定 `manifest_product/platform`；不得猜测。
- `<config_repo_dir>/config/<config_profile>/list.env` 缺失或不含 HUBS 关键键（`G_PROJ_HUBS_PRODUCT_NAME / PCB_VERSION / BOOM_VERSION / SOFT_VERSION`）。
- 扫描 `<project_root>/autobuild/*/manifests/{mpu,mcu}_<manifest_product>.xml` 得到 0 个或 ≥2 个匹配；无法唯一确定 `platform`。
- 只能通过"配置路径反推"确定 `workspace`（优先级 3），且该目录已有内容——此时必须让用户确认目标目录，不得默认拉入。
- `git_pull_xml.py` 在优先级 1～3 中找不到，且用户拒绝提供路径。
- 目标工作目录存在可能被覆盖的数据（无关文件、未提交修改或来源不明的仓库）。
- `git_pull_xml.py` 拉取失败且包含任何尚未由用户明确批准忽略的仓库；若所有失败都在本次明确忽略清单中，可按步骤 5 继续，但必须保留失败原文并在最终基线标记不完整。
- `--path` 过滤未按预期生效、忽略仓库被再次尝试，或无法逐仓库区分已批准忽略项与新失败项。
- MPU/MCU manifest XML 不可解析，或其配置仓库项目/分支与临时探测结果不一致。
- curl 下载失败、unzip / copy 任一步骤返回非零，或 SDK 来源 build_model 缺失。
- `scripts/hcHubSdkDownload.py` 调用返回非零退出码且不能通过使用产品匹配的 build 脚本、正确模型参数或当前进程凭据变量修复；若是已有目标、缓存冲突或网络/解压错误，停止并保留原始报错。
- HUBS SDK 缓存 `<workspace>/.ai_tmp/hubs-sdk/` 已存在但携带不同 `<hubs_product>_<pcb><boom>[_<softver>]` 的旧产物，且本次不是显式 `--force`；需向用户报告并要求明确指示（保留/替换/复用）。
- 准备流程或后续开发任务中需要执行任何 manifest 拉取、SDK 下载、git / MR / commit / push、临时检出、目录清理或强制推送动作而未获得用户明确同意。对明确授权忽略的仓库及已授权删除的精确临时探测目录，仅按本次授权范围继续。

不得删除目录、重置仓库或猜测分支来继续。
