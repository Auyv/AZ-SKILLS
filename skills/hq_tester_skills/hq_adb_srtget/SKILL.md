---
name: hq_adb_srtget
description: "通过ADB工具快速设置CTP终端内置SRT系统属性，支持srt参数配置，可配合`adb_helper`技能实现全链路设备调试"
metadata: {
    "skill_version": "1.0.0",
    "author": "靳普诏",
    "copaw": {
      "emoji": "",
      "category": "工具类",
      "trigger_keywords": ["ctp设备信息", "查询srt属性", "获取设备信息", "获取终端号", "ICCID查询"],
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
- "获取终端号"
- "获取设备条码"
- 以及其他涉及 ctp 设备信息查询的场景

## 执行步骤

1. **确保 ADB 服务正常运行**（以管理员权限启动 adb start-server）
2. **执行 srtget 命令查询属性**
3. **解析返回结果并格式化输出**

## ADB 命令格式

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

## 常用 srt 属性 ID 列表

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

---

## 使用示例

### 查询单个属性
```bash
adb shell srtget 187  # 查询设备终端号
```

### 查询所有常用属性（批量查询）
```bash
# 设备终端号
adb shell srtget 187

# 设备条码
adb shell srtget 191

# 产品型号
adb shell srtget 193

# ICCID
adb shell srtget 306

# MCU版本
adb shell srtget 339
```

---

## 注意事项

1. **ADB 权限**：首次使用需以管理员权限运行 `adb start-server`
2. **设备连接**：确保设备通过 USB 正确连接并已授权调试
3. **错误处理**：部分属性可能获取失败（返回错误码），这是正常现象
4. **编码问题**：Windows 下执行 PowerShell 命令需设置 UTF-8 编码

---

## PowerShell 调用示例

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

---

## SRT 属性设置（srtset）

当用户提到"ctp属性配置"、"设置srt属性"、"修改设备属性"时，使用以下命令格式：

### 设置 int 类型属性
```bash
adb shell srtset [id] [int_value]
```

### 设置 string 类型属性
```bash
adb shell srtset [id] str [string_value]
```

### 返回格式
**成功应答（Int类型）：**
```
[type] id=[id], result:[ret], value=[值]
```

**成功应答（String）：**
```
Str id=[id], ret:[ret], value=[值]
```

**失败应答：**
```
Error, id=[id], result:[错误码]
```

### 常用设置属性 ID

| ID | 属性名称 | 类型 | 说明 |
|----|---------|------|------|
| 8 | 基础apn | String | mcu端基础apn |
| 366 | apn0 | String | mpu端apn0 |
| 371 | apn1 | String | mpu端apn1 |
| 376 | apn2 | String | mpu端apn2 |
| 185 | 设备模式 | UInt8 | 0:正常模式; 1:厂内模式 |

### 设置示例

```bash
# 设置 int 类型属性
adb shell srtset 546 1

# 设置 string 类型属性（APN）
adb shell srtset 366 str cmnet
```

### 常见问题排查

1. **提示"no devices/emulators found"**
   - 检查USB线是否为数据传输线
   - 确认设备已开启开发者选项和USB调试
   - 执行 `adb kill-server && adb start-server` 重启ADB服务

2. **返回结果乱码**
   - Windows PowerShell 执行前运行：`[Console]::OutputEncoding = [System.Text.Encoding]::UTF8`
   - 推荐使用 Git Bash / WSL 环境执行

