---
name: hq_adb_srtget
description: "通过ADB工具快速设置CTP终端内置SRT系统属性，支持srt参数配置，可配合`adb_helper`技能实现全链路设备调试"
version: 1.1.0
metadata: {
    "skill_version": "1.1.0",
    "author": "靳普诏",
    "copaw": {
      "emoji": "",
      "category": "工具类",
      "trigger_keywords": ["ctp设备信息", "查询srt属性", "获取设备信息", "获取终端号", "ICCID查询", "通过名称查询SRT属性", "OBOS查srt", "OBOS查询属性", "远程查SRT", "obos参数id"],
      "requires": {"adb_helper": ">=1.0.0"},
      "scope": "private",
      "auto_trigger": true
    }
}
---


# CTP设备SRT属性查询

## 触发条件

当用户询问以下内容时触发：
- "ctp设备基本信息"
- "查询设备 srt 属性"
- "获取终端号 / 获取设备条码 / 查询 ICCID / IMEI / SIM卡状态 / 网络信号"
- **"OBOS 查 SRT 属性" / "OBOS 远程查" / "OBOS 参数 ID"** → 走第 5 节「OBOS 远程查询」分支
- 以及其他涉及 ctp 设备信息查询的场景

## 查询路径选择（重要！先看这里再动手）

| 场景 | 选哪种方式 |
|------|----------|
| 设备在手上，现场调试 | **ADB 本地查**（走第 1-4 节，`adb shell srtget`） |
| 设备不在身边，需要远程查 | **OBOS 网页**（走第 5 节，远程查最近一次上报值） |
| 跨设备批量核对 | **OBOS 网页**（支持「批量查询」+「导出」CSV/Excel） |
| 怀疑终端未上报/上报延迟 | OBOS + 本地 `srtget` 交叉验证 |

---

# 一、ADB 本地查询

## 1. 执行步骤

1. **确保 ADB 服务正常运行**（以管理员权限启动 adb start-server）
2. **确定 SRT 属性 ID**
   - 如果用户已提供明确 ID，直接使用该 ID。
   - 如果用户只提供属性名称或自然语言描述，先查阅下方常用列表。
   - 常用列表没有覆盖时，读取 `C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\鸿泉经验\自研类\SRT常用属性.md`，按"名称 / 说明 / 数据类型"匹配最相关的 `srt id`。
   - 匹配到 ID 后，先向用户说明匹配依据，例如：`ICCID 对应 kSidBinSimIccid，srt id 为 306`。
3. **执行 srtget 命令查询属性**
4. **解析返回结果并格式化输出**

## 2. 自然语言属性查询规则

- 优先精确匹配用户给出的关键词，如 `ICCID`、`IMEI`、`SIM卡状态`、`信号强度`、`终端号`。
- 同一关键词匹配到多个候选 ID 时，不要猜测；列出候选项并请用户确认。
- 本技能只负责查询或设置 SRT 属性，不扩展为通用文档检索技能；查表仅用于确定 `srtget` / `srtset` 所需 ID。
- 示例：用户要求"通过 srt 系统查 iccid 的值"时，查表确认 `ICCID` 对应 `srt id 306`，然后执行 `adb shell srtget 306`。

## 3. ADB 命令格式

```bash
adb shell srtget [id]
```

### 返回格式

**成功应答（Int类型）：**
```
type=[type] id=[id], len=[length]
[dtype] id=[id], ret:0, value=[值]
```

**成功应答（String/Bin类型）：**
```
type=[type] id=[id], len=[length]
Bin id=[id], ret:[长度]
    String: [字符串]
    Mem: [16进制]
```

**失败应答：**
```
error, is not srt_id=[id] get_result:[错误码]
```

---

## 4. 常用 srt 属性 ID 列表

| ID | 属性名称 | 类型 | 说明 |
|----|---------|------|------|
| 187 | 设备终端号 | UInt32 | 设备唯一标识 |
| 191 | 设备13位条码 | Bin | 产品条码 |
| 193 | 产品型号 | Bin | 如 HQT420-S01-01 |
| 306 | ICCID号 | Bin | SIM卡集成电路卡识别码 |
| 339 | MCU软件版本号 | Bin | 主控固件版本 |
| 340 | MCU Boot版本号 | Bin | 启动程序版本 |
| 192 | MCU程序编译日期 | Bin | 固件编译时间 |
| 341 | MPU软件版本号 | Bin | 协处理器固件版本 |
| 344 | 程序烧写时间 | UInt32 | 固件烧录时间戳 |
| 345 | 硬件版本号 | UInt32 | 硬件版本 |

### 使用示例

```bash
# 查询单个属性
adb shell srtget 187  # 设备终端号

# 批量查询
adb shell srtget 187   # 终端号
adb shell srtget 191   # 设备条码
adb shell srtget 193   # 产品型号
adb shell srtget 306   # ICCID
adb shell srtget 339   # MCU版本
```

### 注意事项

1. **ADB 权限**：首次使用需以管理员权限运行 `adb start-server`
2. **设备连接**：确保设备通过 USB 正确连接并已授权调试
3. **错误处理**：部分属性可能获取失败（返回错误码），这是正常现象
4. **编码问题**：Windows 下执行 PowerShell 命令需设置 UTF-8 编码

### PowerShell 调用示例

```powershell
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

# 启动 ADB 服务（管理员权限）
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = "adb.exe"
$psi.Arguments = "start-server"
$psi.Verb = "RunAs"
$psi.UseShellExecute = $true
$process = [System.Diagnostics.Process]::Start($psi)
$process.WaitForExit()

# 查询属性
& adb shell "srtget 187"
```

### 常见问题排查

1. **"no devices/emulators found"**
   - 检查USB线是否为数据传输线
   - 确认设备已开启开发者选项和USB调试
   - 执行 `adb kill-server && adb start-server` 重启ADB服务
2. **返回结果乱码**
   - Windows PowerShell 执行前运行：`[Console]::OutputEncoding = [System.Text.Encoding]::UTF8`
   - 推荐使用 Git Bash / WSL 环境执行

---

# 二、SRT 属性设置（srtset）

当用户提到"ctp属性配置"、"设置srt属性"、"修改设备属性"时，使用以下命令格式：

```bash
# int 类型
adb shell srtset [id] [int_value]
# string 类型
adb shell srtset [id] str [string_value]
```

### 返回格式

**Int 类型成功**：`[type] id=[id], result:[ret], value:[值]`
**String 成功**：`Str id=[id], ret:[ret], value:[值]`
**失败**：`Error, id=[id], result:[错误码]`

### 常用设置属性 ID

| ID | 属性名称 | 类型 | 说明 |
|----|---------|------|------|
| 8 | 基础apn | String | mcu端基础apn |
| 366 | apn0 | String | mpu端apn0 |
| 371 | apn1 | String | mpu端apn1 |
| 376 | apn2 | String | mpu端apn2 |
| 185 | 设备模式 | UInt8 | 0:正常模式; 1:厂内模式 |

```bash
adb shell srtset 546 1            # int 类型
adb shell srtset 366 str cmnet    # string 类型（APN）
```

---

# 三、OBOS 远程查询（设备不在身边时使用）

> 完整操作文档：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\鸿泉经验\Obos\obos查看srt属性.md`

## 3.1 入口

```
URL:   http://obos.hopechart.com/#/terminalData/terminalaparm
导航:  终端数据 → 参数信息
```

> ⚠️ **菜单路径易错**：是**「终端数据」**下面的**「参数信息」**，不是「终端参数」子页面。
> 这两个一级菜单名字相近，新人 100% 会迷路。

## 3.2 SRT ID → OBOS 参数 ID 转换规则（核心技巧）

OBOS 网页的「参数 ID」输入框只接受 **OBOS 专用参数 ID**，必须按下面公式把 SRT ID 转换过来：

```
OBOS参数ID = SRT_ID（十六进制） | 0x40000
```

即在 SRT ID 的十六进制基础上**强制置位第 18 位**（bit18，0x40000），形成 OBOS 专用参数空间。

**反向公式**（核对用）：
```
SRT_ID = OBOS参数ID & 0xFFFF
```

### 示例验证

| SRT ID | 属性名 | SRT 十六进制 | OBOS 参数 ID |
|-------:|-------|------------|----------:|
| 191 | 设备序列号（13位条码） | 0xBF | **0x400BF** |
| 187 | 设备终端号 | 0xBB | **0x400BB** |
| 310 | VIN 码 | 0x136 | **0x40136** |
| 306 | ICCID | 0x132 | **0x40132** |

## 3.3 查询操作步骤

1. **条件选项**（必填）：下拉选「10位终端编号」
2. **数据类型**（必填）：下拉选「最新数据」
3. **参数ID**（可选）：填 `0x400BF` 这类 OBOS ID，用于精确定位某一个属性；不填则返回该终端全部属性
4. **10位终端编号**（必填）：填入终端号
5. 点 **「查询」** 按钮
6. 在下方表格看返回结果

## 3.4 常用 SRT ID → OBOS 参数 ID 速查

| SRT ID | OBOS 参数 ID | 属性名 | 用途 |
|-------:|----------:|-------|------|
| 187 | 0x400BB | 设备终端号 | 核对终端号 |
| 191 | 0x400BF | 设备序列号（13位条码） | 核对产品条码 |
| 193 | 0x400C1 | 内部产品型号 | 核对型号 |
| 306 | 0x40132 | SIM卡ICCID | 核对SIM卡 |
| 310 | 0x40136 | VIN码 | 核对车架号 |
| 339 | 0x40153 | MCU软件版本号 | 核对固件版本 |
| 340 | 0x40154 | MCU Boot版本号 | 核对启动版本 |
| 341 | 0x40155 | MPU软件版本号 | 核对协处理版本 |
| 344 | 0x40158 | 程序烧写时间 | 核对烧录时间 |
| 345 | 0x40159 | 设备硬件版本 | 核对硬件版本 |
| 355 | 0x40163 | 远程日志使能 | 远程日志 |
| 390 | 0x40186 | GNSS日志使能 | GNSS日志 |
| 391 | 0x40187 | CAN日志使能 | CAN日志 |
| 488 | 0x401E8 | 自动化测试通道使能 | 自动化测试 |

## 3.5 常见坑

- **直接填 SRT 十进制数字 → 查不到任何结果。** 必须是 `0x4xxxx` 形式。
- **OBOS 下发到终端有约 1 分钟时延。** 刚下发完参数不要立刻查，等 1 分钟。
- **「最新数据」是最近一次上报值**，不一定等于设备当前实际值。和 `srtget` 查的不一致时，以设备本地为准。
- **查不到数据**排查顺序：① 终端号对不对（用 0x400BB 反查）→ ② OBOS ID 拼错没（用 `& 0xFFFF` 校核）→ ③ 终端在线状态（看「终端监控」）。
- **批量查多个属性**：用页面「批量查询」按钮 + 「导出」按钮可导出 CSV/Excel。

## 3.6 表格返回字段

| 字段 | 含义 | 示例 |
|------|------|------|
| 10位终端编号 | 设备终端号 | 2508133932 |
| 参数ID | OBOS 参数 ID（含 0x40000） | 0x000400BF |
| 参数值 | 属性实际值 | 485G258D23932 |
| 数据类型 | String / Int / Bin | String |
| 数据时间 | 终端上报时间 | 2026-06-29 10:33:37 |

> Bin 类型以十六进制存储，String 类型已自动转码可读。
> Bin 转字符串的常用 ASCII 范围：`30-39` = 数字 0-9，`41-5A` = 大写字母 A-Z。

---

# 四、相关文档

- **OBOS 完整操作文档**：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\鸿泉经验\Obos\obos查看srt属性.md`
- **SRT 完整属性表**：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\鸿泉经验\自研类\SRT常用属性.md`
- **SRT 数据源**：`C:\HQ_Workspace\AR590\code\srt_mpu\srt_pid_type.h`
- **OBOS 兄弟文档**：`C:\JPZ_Workspace\笔记仓库-jpz\hq-jpz\鸿泉经验\Obos\obos清除队列数据.md`（参数下发路径不同，参考用）
