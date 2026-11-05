---
name: hc-ctp-work-ready-workflow
description: |
  当用户提供本地产品配置目录/list.env，或 GitLab 分支中的配置目录链接，并要求识别配置、拉取
  MPU/MCU manifest 与 HUBS SDK、准备工作目录和可开发基线时使用。配置 profile 名可能与 manifest
  产品名不同；需要按仓库分支和 manifest 记录核实映射，不得仅凭目录名猜测。
  统一完成工作目录隔离、manifest 拉取、HUBS SDK 下载、仓库状态核验与后续开发交付门禁；
  在用户给出具体任务前不修改业务代码。
version: 2.8.0
---

> **授权边界（先于一切流程）**
>
> 本 skill 是**流程指引**，不预先授权任何 git commit / push / MR / `reset --hard` /
> force push / 远端合并等操作。所有此类操作严格遵循项目全局 Git 规则（`未获同意不得
> git commit / git push / git reset --hard / 强制推送`）以及 `az-gitlab-merge-request`
> 等既有 skill 的授权约定。
>
> - 每一个可能产生对外副作用的动作（创建或切换开发任务分支、推送分支、创建/更新 MR、commit、reset、
>   强制推送、删除 workspace 等）都必须**在本次会话中取得用户明确同意**后才能执行。
> - 即使获得一次同意，也**仅对该次会话内的同一次动作有效**；不形成持久默认、不构成
>   后续操作的预授权、不允许在不同任务间复用。
> - 用户明确要求在指定目录部署该产品开发环境，视为授权该次准备所必需的分支探测、manifest
>   拉取和 SDK 下载；执行前说明范围。此授权不包括忽略失败、覆盖、清理或其他破坏性操作。
>   若用户只要求分析或预检，则不得据此写盘或下载。
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
- `source_branch`：GitLab URL 中 `/-/tree/` 或 `/-/blob/` 后的分支；blob URL 可指向 `config/<profile>/list.env`。
- `project_root`：项目根，即当前登记的 CTP 产品工程根；`autobuild/` 与 `workspace/` 都挂在它下面。
- `workspace_root`：`<project_root>/workspace`，工作空间父目录。
- `workspace`：**本次任务要拉取代码的目标目录名**，识别优先级如下（关键：不要从配置路径反推）：
  1. 用户在本次会话显式指定的目录名或绝对路径；
  2. 当前会话的工作目录（若它位于 `<workspace_root>/` 之下），取其相对于 `workspace_root` 的路径段；
  3. 用户提供的本地配置路径中相对于 `workspace_root` 的最末目录段（仅作为兜底，且必须在报告中显式提示"该目录已存在内容，代码将拉入其中"）。GitLab URL 不能反推出 workspace；前两项都不可用时询问用户。
  > 配置路径所在的 workspace 与代码要拉入的 workspace **可以不是同一个**。常见情形是用户提供
  > 另一个任务 workspace 中已拉好的 `config/<config_profile>/list.env` 作为配置参考，而代码要拉进
  > 当前任务的新 workspace。两者不得混淆。
- 用户明确指定当前目录或其他绝对路径时，目标可在产品工程的 `workspace/` 之外；将其直接作为
  `workspace`，不得移动或反推目标。配置仓库来源与 manifest 来源单独定位，不因目录布局不符猜测产品。
- `platform`：URL 场景由已核验的分支 manifest 中配置仓库的 project path 确定；本地场景从适用的
  `<project_root>/autobuild/*/manifests/` 扫描。若 `<config_profile>` 没有同名 manifest，按“GitLab 分支与 manifest 映射”核对配置仓库项目及 revision，只有唯一匹配时才能确定 `manifest_product` 和 `platform`。

> ⚠️ `autobuild` 是 `<workspace_root>` 的**兄弟**目录，不是它的子目录。基准路径必须写成
> `<project_root>/autobuild/...`；写成 `<workspace_root>/autobuild/...` 会因目录不存在而失败。

执行前自动校验以下要素；任一缺失或不匹配时停止并报告：

1. 配置路径可读；若为 GitLab 链接且本机无对应副本，必须先按授权流程临时检出分支，再验证 profile 下的 `list.env`。
2. `<config_repo_dir>/config/<config_profile>/list.env` 含 HUBS 产品、PCB、BOOM、软件版本键及五个 `G_PROJ_LIST_HUBS_*` 清单键。
3. 已核验的配置仓库分支或 `<project_root>/autobuild/<platform>/manifests/` 中对应的 MPU、MCU manifest 均存在且可解析；
   `manifest_product` 与 `config_profile` 不同的情况必须有唯一、可复核的分支映射证据。
4. 本 Skill 随附的 `scripts/hcCtpWorkReady.py` 存在；运行环境提供 Python 3.9+ 和原生 Git CLI（含 SSH 凭据）。不依赖 `git_pull_xml.py`、curl、unzip 或第三方 Python 包。

完整读取当前环境提供的全局规则、工作区与目标仓库的 `AGENTS.md`。涉及分支、提交或 MR 时，再读取适用的 Git 规则。

## 准备流程

### 1. 解析路径与识别产品型号

- 对本地路径，提取末段为 `config_profile`。如果它与 manifest 后缀不一致，不得直接停止或改名；先看是否有本次会话提供的 GitLab URL 可用于分支映射。
- 对 GitLab URL，提取 GitLab 主机、项目路径、`source_branch` 和 `config_profile`。若 URL 不含完整项目/分支信息，停止并询问缺少的精确部分。
- 对本地路径，确认它确实位于 `<project_root>` 之下（`workspace/` 或 `autobuild/`）；不在则停止，避免误读别处同名目录。对 URL，核对 GitLab 主机与项目路径，不将 URL 当作本地文件路径。
- 按"输入与边界"中的优先级确定 `workspace`，并在报告中显式区分：`config_profile` / `manifest_product` / `platform` / `workspace` / 配置来源 / 目标拉取目录。
- 对 URL，先核验来源分支的新鲜度并取得该分支的 manifest 和配置，再判断是否存在同名清单或映射歧义；不得仅因旧本地副本缺少同名清单而提前询问。对本地路径则扫描适用的 `autobuild/*/manifests/`。同一来源必须同时命中一对 MPU/MCU 清单，且其配置仓库项目及有效 revision 与 URL 一致，才可把 profile 作为 `manifest_product`。

#### GitLab 分支与 manifest 映射

当用户给出 GitLab `/-/tree/` 目录链接或 `/-/blob/` 的 `list.env` 链接时：

1. 解析 URL 和精确分支名；分支含 `/` 时按 URL 编码或 GitLab 的分支信息确认边界，不猜测。只读查询远端分支 HEAD，核对本地副本的 origin、分支及 commit；仅分支名相同不足以证明副本是最新的。远端不可访问或 HEAD 无法核实时报告，不把旧副本当作最新版本。
2. 本地没有与远端 HEAD 一致的可读副本时，在获授权后仅将 URL 对应仓库的指定分支检出到 `<workspace>/.ai_tmp/branch_probe/<config_repo_dir>/`，读取其中的 `config/<config_profile>/list.env`、`manifests/` 和所需 build 脚本。先确认目标不存在；不得更改共享副本或为了探测克隆所有 manifest 项目。记录探测 commit；目标仓库最终拉取的 commit 与探测 commit 不同时暂停并重新核验输入。
3. 优先检查该分支的 `mpu_<config_profile>.xml` 与 `mcu_<config_profile>.xml`。两份均可解析且都指向 URL 对应配置仓库与精确分支时使用这对同名 manifest，即使同一分支还有其他产品清单也不因此产生歧义。按配置仓库 project 的 path 确认 platform 与落位。
4. 同名清单缺失或不匹配时，才在该分支的 manifest 中按配置仓库项目及有效 revision 查找成对候选。有效 revision 按项目 `revision` → 对应 remote 的 `revision` → manifest `<default>` 顺序解析；只有恰好一对匹配时采用其后缀。否则报告候选并询问，不得创建或改写 manifest，也不得根据型号相似猜测。
5. 仅在完整拉取、SDK 下载和基线检查成功后，且用户明确授权清理时，删除精确的 `branch_probe` 目录；流程失败时保留它以便诊断。不得连带删除 `.ai_tmp/hubs-sdk/`。
6. 拉取目标 workspace 的配置仓库后，改用其中的 `list.env` 作为权威来源；其他项目失败时必须按步骤 5 取得忽略批准。核对目标配置仓库 commit 和两份 manifest、`list.env` 的哈希与步骤 1～4 采用的已核验来源一致；任何一项不一致均暂停 SDK 下载并报告，避免分支在准备期间前移造成混合基线。

若本地路径不在 `<project_root>` 下，或不符合 `config/<profile>` 形态，且无法由给定 GitLab URL 明确定位时，停止并请用户确认目录布局。

### 2. 隔离工作空间

- 使用步骤 1 确定的目标目录作为本次任务独立工作目录；它可以是用户明确指定的绝对路径，**不是**配置参考副本所在目录。
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
| 拉入副本 | `<workspace>/<platform>/<config_repo_dir>/config/<config_profile>/list.env` | **权威来源**，随代码版本走 |
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
全部为空返回 1。返回 0 才按开头授权边界执行步骤 6。

注意：`list.env` 在 Windows 上可能带 CRLF；按行解析时需规范化换行并去除值两侧空白。

### 4. 定位 manifest 与专用工具

按 `manifest_product` 与步骤 1 确定的来源定位；URL 场景使用已核验的分支副本中的 `manifests/`，本地场景通常使用：

- MPU manifest：`<project_root>/autobuild/<platform>/manifests/mpu_<manifest_product>.xml`
- MCU manifest：`<project_root>/autobuild/<platform>/manifests/mcu_<manifest_product>.xml`

读取 manifest 的 `<default>` 节点记录默认 `<remote>` 与 `<revision>`；逐项扫描 `<project>` 节点的
`path`、`name`、`remote`、`revision`、`upstream`，作为后续核验基线。

使用本 Skill 自带的 `<skill_root>/scripts/hcCtpWorkReady.py`。若文件缺失，停止并报告 Skill 安装不完整；不得回退到外部拉取脚本或临时拼接 curl/unzip 命令。执行环境只需 Python 3.9+ 标准库和原生 Git CLI；Git/SSH 凭据沿用用户现有配置。

### 5. 按 manifest 拉取 MPU / MCU 仓库

按开头授权边界确认目标与动作；明确部署请求已覆盖常规拉取。专用工具以父路径优先、单线程方式处理项目，避免 MCU 父仓库与其嵌套子仓库并发创建造成目录竞争：

```powershell
python <skill_root>/scripts/hcCtpWorkReady.py pull `
  --workspace <workspace> `
  --manifest <已核验来源>/manifests/mpu_<manifest_product>.xml `
  --manifest <已核验来源>/manifests/mcu_<manifest_product>.xml
# 每个本次已获用户批准忽略的项目增加一个精确标识：
# --ignore "<remote>|<project-name>"
```

工具行为与约束：

- 同一 `(remote, name, path)` 项目跨 manifest 去重；同一仓库身份出现在不同 path 时分别核验落位。结果 JSON 保留每项 path、name、remote、revision、HEAD/branch 或原始错误，并统计成功、失败、忽略数及不同仓库数。
- 按 manifest 路径深度从浅到深串行执行，父项目先于嵌套子项目；不启用并发参数。
- 目标路径不存在时执行 `git clone --no-checkout`；目标已存在时必须是 Git 仓库、origin 与 manifest URL 相符且工作区干净，否则该仓库失败并保留现场。
- 分支 revision 用 fetch + checkout / `pull --ff-only`；SHA 或 tag revision 用 detached checkout。脚本不执行 reset、清理目录或强制覆盖。
- manifest project 的 URL 由 remote fetch 与 project name 拼接；如企业 manifest 使用特殊 URL 规则，应先核实并修正输入，不要猜测。

拉取过程中，逐仓库检查 JSON 结果：

- 执行前为本次运行在 `<workspace>/.ai_tmp/` 准备不覆盖已有文件的唯一结果路径；将完整 JSON stdout 和原始 stderr 分别保存，记录进程退出码，再读取 JSON 统计与每个失败项，避免工具显示截断导致丢失错误。报告中不要泄露凭据；若原始输出可能包含凭据，限制访问且不要直接展示敏感内容。
- 任一项目失败时停止 SDK 等后续阶段，保留原始错误、已创建目录和已完成仓库范围；不删除 workspace 掩盖失败。报告完整精确 `(remote, name)`、路径及错误。只有用户在本次会话批准具体失败项或明确批准已展示的整组项目后，才可将对应精确身份传给 `--ignore`；不将批准扩展到尚未展示的新失败。
- GitLab 的 `project could not be found` / `Could not read from remote repository` 可能是无权限、仓库不存在或路径错误，不能断定是权限问题；若用户只批准“无权限项目”而原因未区分，应展示这些不确定项目并请用户明确是否包含它们。网络、凭据、分支不存在等其他错误不得归入权限失败。
- 本工具会继续处理后续项目，失败的父项目目录可能被成功的嵌套子项目创建为非 Git 目录。重跑前检查每个父路径、子仓库和未提交内容；不得删除、覆盖或把非 Git 父目录当作干净仓库。修复访问后如遇此冲突，保留现场并与用户确定安全迁移或重建方案。
- 对已批准忽略且无需重拉的项目，可直接运行 `baseline --ignore` 审核已有结果，再在配置仓库、manifest 和 SDK 条件均满足时继续；不必为登记忽略项重跑所有成功项目。
- `--ignore` 只接受引号包裹的 `"REMOTE|PROJECT_NAME"` 精确身份（PowerShell 中竖线需引用）；忽略项不会扩展到新失败项目，且在 MPU/MCU 两份清单间按身份一致生效。
- **不要**为了"统一分支"而对已存在的本地仓库执行 `git reset --hard`、`git checkout -B`、强制推送等破坏性操作；任何此类操作都需用户额外明确同意。

### 6. 参考 build 脚本拉取 HUBS SDK

按开头授权边界确认下载已获授权，并在执行前向用户说明 URL、目标目录与解压路径。

先从 workspace 权威 `list.env` 读取产品模型并检查对应 build 脚本，计算实际 URL、package mode、MPU/MCU build model、缓存目录和两个落位目录。把这些具体值复述给用户后再下载；若 `list.env` / build 脚本不完整，或下载脚本无法处理该模型、版本模式或凭据来源，先排除输入问题再重试，不得把一次可恢复的 `Error_CredMissing` 直接当成网络失败。

仅当步骤 3 中 `NeedHubsSdk()` 返回 0 且 MPU `<workspace>/<platform>/sdk/` 与 MCU `<workspace>/<platform>/mcu/sdk/` 两个目标都不存在时执行；若目标已存在，分别核验布局并报告，不得默认覆盖。专用工具没有覆盖选项；替换需另行设计并获得用户明确同意。

#### 目录布局

下载与解压的中间产物落到本次 workspace 的 `.ai_tmp/hubs-sdk/` 缓存区。专用 Python 工具使用标准库 `urllib.request` 下载、`zipfile` 解压，并对 ZIP 成员做路径穿越与符号链接校验；无需 curl/unzip。**最终 SDK 副本按产品实际 build 脚本语义铺平到两个目标**（MPU 一个、MCU 一个）：

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

`--cache-dir <path>` 可覆盖默认缓存根，便于把缓存放到其它盘 / 外部存储。所有中间目录（含 zip 与 staging）**默认保留**，由用户人工手动清理 `.ai_tmp/` 目录；脚本**不**自动删除。

#### 包路径与凭据

HUBS 包路径规则与该产品实际使用的 `build/mpu/build_<model>.sh` 保持一致：

- 默认（latest）包名 `<hubs_product>_<pcb><boom>.zip`，URL `<base>/<hubs_product>/<pcb><boom>/<name>`。
- 固定（pinned）包名 `<hubs_product>_<pcb><boom>_<softver>.zip`，URL `<base>/<hubs_product>/<pcb><boom>/history/<name>`，其中版本取 `list.env` 的 `G_PROJ_HUBS_SOFT_VERSION`。
- 下载前检查所选 `build/mpu/build_<G_PRODUCT_BUILD_MPU_MODEL>.sh` 的包命名逻辑：若脚本明确使用 `history/<softver>`，调用 `hcCtpWorkReady.py download-sdk` 时传 `--soft-version <softver>`；若脚本取 latest，则不传。不得只因 `SOFT_VERSION` 非空就推断下载模式。
- 默认 base URL `http://172.16.1.99:8081/artifactory/sei/project`；build model 以配置和对应 build 脚本为准，`ag35_ceraa` / `s32k312` 只是 downloader 默认值。

凭据来源（仅记录来源，不在脚本中硬编码）：

1. `<list-env>` 中的 `G_PROJ_HUBS_ARTIFACTORY_CRED` 等候选键；
2. 环境变量 `HUBS_ARTIFACTORY_CRED`；
3. workspace 内配置仓库 `build/mpu/build_<model>.sh` 中 `curl -u...` 段；
4. 命令行 `--cred user:password`。

专用工具按 `--config-repo` 与实际 build model 自动检查对应脚本中的静态凭据。若脚本使用动态 shell 变量、凭据来源不可解析或需要命令行凭据，先由用户选择已配置的环境变量或传 `--cred`；不得将凭据写入 skill、命令文件、日志或最终报告，也不得打印原始 `curl -u...` 行。

#### 调用模板

```powershell
python <skill_root>/scripts/hcCtpWorkReady.py download-sdk `
  --list-env <workspace>/<platform>/<config_repo_dir>/config/<config_profile>/list.env `
  --workspace <workspace> --platform <platform> --config-repo <config_repo_dir> `
  --profile <config_profile> `
  --build-model <G_PRODUCT_BUILD_MPU_MODEL> `
  --mcu-build-model <G_PRODUCT_BUILD_MCU_MODEL>
# 可选：--cache-dir <path>          覆盖默认 <workspace>/.ai_tmp/hubs-sdk
# 可选：--mpu-target <dir>          MPU 落地目录，默认 <workspace>/<platform>/sdk
# 可选：--mcu-target <dir>          MCU 落地目录，默认 <workspace>/<platform>/mcu/sdk
# 可选：--base-url <url>            默认 http://172.16.1.99:8081/artifactory/sei/project
# 可选：--soft-version <version>    选择 history 下的 pinned 包；latest 不传此参数
# 可选：--cred <user:password>      仅在自动凭据来源均失效时使用，工具不会回显
```

#### 工具职责

1. 解析 `<list-env>` 并校验 HUBS 关键字段；五个 `G_PROJ_LIST_HUBS_*` 全空时拒绝下载。
2. 根据 build model 检查产品对应的 `build/mpu/build_<model>.sh`；latest/pinned 模式由执行者先按构建脚本确认，pinned 时传入 `--soft-version`。
3. 凭据按 list.env 候选键、`HUBS_ARTIFACTORY_CRED`、对应模型 build 脚本中的静态 `curl -u`、`--cred` 顺序读取；不执行 shell、不输出凭据内容。
4. ZIP 与 staging 已存在时仅复用可读的完整产物；空缓存、损坏或不完整 staging 保留并报错，不覆盖或清理。
5. 两个来源模型均存在且 MPU/MCU 目标均不存在时才复制。任一目标已存在即停止，不提供自动覆盖选项。
6. 返回 JSON，包含 URL、包模式、凭据来源、缓存路径、模型、落位目录和目录项；不包含凭据。

#### 退出码

| 码 | 含义 |
|:---|:---|
| `0` | 成功；JSON 输出含 manifest 逐仓库结果或 SDK 下载摘要 |
| `1` | 输入错误、项目拉取失败、已有目标、缺凭据、网络/解压错误等；以 JSON `error` 或逐项 `failed` 结果为准 |

### 7. 交付可开发基线

对本次相关仓库检查 `git status --short`、当前 HEAD / 分支与 manifest 指定 `revision`；报告任何未提交改动、拉取遗漏或分支偏差。这些检查为只读操作，可直接进行。

可先用工具生成机器可读的清单，再由执行者按下列规则复核：

```powershell
python <skill_root>/scripts/hcCtpWorkReady.py baseline `
  --workspace <workspace> `
  --manifest <已核验来源>/manifests/mpu_<manifest_product>.xml `
  --manifest <已核验来源>/manifests/mcu_<manifest_product>.xml
# 本次获授权忽略项可重复传入：--ignore "<remote>|<project-name>"
```

`baseline` 只读并输出 JSON；脚本不会代替执行者读取 `AGENTS.md`、判断嵌套目录是否为预期内容或决定基线是否完整。

- 对 MPU/MCU 两份 manifest 的每个 `project path` 检查仓库是否存在、HEAD、当前分支和 `git status --short`；显式 `revision` 优先，否则按项目 remote revision，再回退到 default revision。SHA revision 与 HEAD 比较；分支 revision 与当前分支名比较。
- 分开统计 manifest 项目条目数、成功、失败、用户批准忽略数和不同仓库数；同一 `(remote, name)` 在 MPU/MCU 两份清单重复时按一个仓库报告，但保留两侧 path/revision 差异。
- 聚合父仓库可能因 manifest 管理的嵌套仓库或 SDK 落位显示 `?? app/`、`?? common/`、`?? sdk/`。逐个检查相应子仓库：如果子仓库已单独匹配 manifest 且干净，将父仓库状态说明为预期嵌套目录；其他未提交改动仍需单独报告。

- 找到并读取各仓库的 `AGENTS.md`，确认构建入口、测试方式和额外约束。
- 校验 HUBS SDK 是否成功铺平到 MPU 目标 `<workspace>/<platform>/sdk/` 与 MCU 目标 `<workspace>/<platform>/mcu/sdk/` 下；两侧布局不同，按步骤 6 的"预期布局"表分别核对，不要用同一套判据。
- 汇总 workspace、manifest、已拉取仓库、HUBS SDK 状态和异常项。
- 若仍有用户批准忽略的仓库，标为“环境已准备但仓库不完整”，准确列出失败/跳过仓库；能否开发取决于具体任务与缺失仓库的关系，未执行构建时不得宣称构建通过。
- 若本次使用临时分支探测，只有在仓库拉取、SDK 下载和基线检查均完成且用户已明确授权删除后，才删除 `.ai_tmp/branch_probe/<repo>/`；保留 `.ai_tmp/hubs-sdk/` 下载缓存与 staging。

只有拉取阶段和基线检查完成、所有失败均按上述规则处理后，才向用户报告准备结果并等待具体工作内容；有忽略项时必须说明仓库不完整及适用限制。

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
- 已核验的来源分支没有同名 manifest，且不能通过唯一匹配“配置仓库项目 + source_branch revision”确定 `manifest_product/platform`；不得猜测。
- `<config_repo_dir>/config/<config_profile>/list.env` 缺失或不含 HUBS 关键键（`G_PROJ_HUBS_PRODUCT_NAME / PCB_VERSION / BOOM_VERSION / SOFT_VERSION`）。
- 已核验来源中无法找到唯一适用的 MPU/MCU manifest 对或无法唯一确定 `platform`。
- 只能通过"配置路径反推"确定 `workspace`（优先级 3），且该目录已有内容——此时必须让用户确认目标目录，不得默认拉入。
- Skill 自带 `scripts/hcCtpWorkReady.py` 缺失、Python 版本低于 3.9，或系统找不到原生 Git CLI。
- 目标工作目录存在可能被覆盖的数据（无关文件、未提交修改或来源不明的仓库）。
- `hcCtpWorkReady.py pull` 拉取失败且包含任何尚未由用户明确批准忽略的仓库；失败原文必须保留，且仅将批准范围转换为精确身份跳过。
- 工具报告 origin 不匹配、目标路径是非 Git 目录、仓库工作区不干净，或某目标路径无法安全落在 workspace 中。
- MPU/MCU manifest XML 不可解析，或其配置仓库项目/分支与临时探测结果不一致。
- `hcCtpWorkReady.py download-sdk` 返回错误、HTTP 下载失败、ZIP 校验/解压失败或 SDK 来源 build_model 缺失；保留缓存和原始报错，修正输入后再决定是否重试。
- HUBS SDK 缓存中同名 ZIP 为空、损坏，或 staging 不完整；不得自动覆盖或清理，由用户决定如何处理。
- 准备流程或后续开发任务中需要执行超出本次请求授权范围的操作。对明确授权忽略的仓库及已授权删除的精确临时探测目录，仅按本次授权范围继续。

不得删除目录、重置仓库或猜测分支来继续。
