---
name: hq_adb_srtset
description: "通过ADB工具快速查询CTP终端内置SRT系统属性，支持单个/批量获取设备唯一标识、软硬件版本、SIM卡信息等核心参数，可配合`adb_helper`技能实现全链路设备调试"
metadata: {
    "skill_version": "1.0.0",
    "author": "靳普诏",
    "copaw": {
      "emoji": "",
      "category": "工具类",
      "trigger_keywords": ["ctp属性配置", "设置srt属性", "修改设备属性"],
      "requires": {"adb_helper": ">=1.0.0"},
      "scope": "private",
      "auto_trigger": true
    }
}
---


# CTP设备SRT属性查询

## 🔑 触发条件
当用户执行以下操作时自动触发本技能：
1. **关键词触发**：
   - "ctp属性配置"、"设置设备 srt 属性"、"修改设备属性"、"修改设备apn信息"
2. **场景触发**：
   - 用户需要修改设备属性值
3. **命令触发**：
   - `skill run hq_adb_srtset [属性ID/属性名称] [int_value]`
   - `skill run hq_adb_srtset [属性ID/属性名称] str [string_value]`
4. **边界说明**：
   - ADB安装应用、拉取日志、屏幕操作、系统升级等通用操作请使用`adb_helper`技能，本技能仅处理SRT属性设置\修改相关场景

---

## 📋 前置依赖
| 依赖项 | 要求 | 验证方式 |
|--------|------|----------|
| 工具依赖 | ADB工具已安装并加入系统PATH | 执行`adb --version`返回版本号≥1.0.41 |
| 权限要求 | 设备已开启USB调试并授权当前电脑 | 执行`adb devices`返回设备序列号 |
| 环境变量 | 无特殊要求，Windows环境建议配置UTF-8编码 | 执行`chcp`返回65001 |
| 其他技能依赖 | 可选依赖`adb_helper`自动完成设备连接检查 | 无需额外配置 |

---

## 🎯 核心功能
1. **单个属性设置**：支持8/366/371/376等常用SRT属性ID设置

---


## 📝 执行流程
1. **确保 ADB 服务正常运行**（以管理员权限启动 adb start-server）
2. **执行 srtset 命令设置属性**
3. **解析返回结果并格式化输出**



## ADB 命令格式

### 设置int类型属性
```bash
adb shell srtset [id] [int_value]
```

### 设置string类型属性
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
---

## 常用 srt 属性 ID 列表

| ID | 属性名称 | 类型 | 说明 |
|----|---------|------|------|
| 8 | 基础apn | String | mcu端基础apn |
| 366 | apn0 | String | mpu端apn0 |
| 371 | apn1 | String | mpu端apn1 |
| 376 | apn2 | String | mpu端apn2 |
| 185 | 设备模式 | UInt8 | 设备模式; 0:正常模式;  1:厂内模式 |

---

## 示例
## 示例1：设置int类型属性
```bash
# 命令
adb shell srtset 546 1

# 输出结果
UInt8 id=546, result:0, value=1
```

### 示例2：设置string类型属性
```bash
# 命令
adb shell srtset 366 str cmnet

# 输出结果
Str id=366, ret:0, value=cmnet
```

---

## 常用 srt 属性 ID 列表
| ID | 属性名称 | 类型 | 说明 |
|----|---------|------|------|
| 187 | 设备终端号 | UInt32 | 设备唯一标识，业务系统核心关联ID |
| 191 | 设备13位条码 | Bin | 产品SN码，贴于设备背部 |
| 193 | 产品型号 | Bin | 设备硬件型号，如 HQT420-S01-01 |
| 306 | ICCID号 | Bin | SIM卡集成电路卡识别码，用于流量卡查询 |
| 339 | MCU软件版本号 | Bin | 主控固件版本，功能迭代标识 |
| 340 | MCU Boot版本号 | Bin | 启动程序版本 |
| 192 | MCU程序编译日期 | Bin | 固件编译时间，格式如`2026-03-21 12:00:00` |
| 341 | MPU软件版本号 | Bin | 协处理器固件版本 |
| 344 | 程序烧写时间 | UInt32 | 固件烧录时间戳，可转换为可读时间 |
| 345 | 硬件版本号 | UInt32 | 硬件迭代版本，如`100`对应V1.0.0 |

---

## ❓ 常见问题排查
1. **问题：提示"no devices/emulators found"**
   - 排查步骤：
     1. 检查USB线是否为数据传输线，仅充电线无法识别
     2. 确认设备已开启开发者选项和USB调试（设置-关于设备-连续点击版本号开启开发者选项）
     3. 重新插拔设备，在设备弹出的调试授权窗口勾选"始终允许"并确认
     4. 执行`adb kill-server && adb start-server`重启ADB服务

2. **问题：返回结果乱码**
   - 排查步骤：
     1. Windows PowerShell环境执行前运行：`[Console]::OutputEncoding = [System.Text.Encoding]::UTF8`
     2. 命令行执行`chcp 65001`切换到UTF-8编码
     3. 推荐使用Git Bash/WSL环境执行命令，避免编码问题
