# 0200 N725A MPU 升级适配测试报告

## 基本信息

| 项目 | 内容 |
|---|---|
| 日期 | 2026-08-11 |
| 功能 | 0200 - MPU 固件升级与分区恢复 |
| 环境 | Docker `8249f084089f` / Neoway OpenWrt GCC 13.3.0 |
| 结论 | 适配器自动测试与正式 SDK 交叉链接通过；整应用构建因目标 OpenSSL 缺失阻塞 |

## 测试结果

| 编号 | 场景 | 结果 |
|---|---|---|
| AT001 | SDK 初始化及成功/失败/无记录/同步中状态映射 | 通过 |
| AT002 | 合法路径、厂商返回成功、100% 与等待重启 | 通过 |
| AT003 | 厂商返回失败、统一失败状态和错误回调 | 通过 |
| AT004 | 空路径、超长路径、不存在文件 | 通过 |
| AT005 | 备份、回滚 no-op 成功和 100% 回调 | 通过 |
| AT006 | 活动槽位、损坏状态、SDK 错误与非法槽位 | 通过 |
| AT007 | `BUILD_PLATFORM_NAME=n725a` 映射型号 4 并生成适配器目标文件 | 通过 |

## 执行证据

### 主机桩测试

```text
bash -lc '.../mpu_upgrade/tests/n725a/run_tests.sh'
PASS: N725A MPU upgrade adapter tests
```

### 正式 SDK 交叉链接

`run_cross_compile.sh` 使用正式 `nwy_fota.h`、`libnwy_service`、`libnwy_common` 及系统库链接成功。输出产物：

```text
Class: ELF32
Machine: ARM
Flags: Version5 EABI, hard-float ABI
```

### 整应用构建

`build/build.sh Debug` 已识别 `HQ_CMAKE_BUILD_MODEL_TYPE=4`，N725A 适配器目标文件生成成功，构建到 96% 后失败：

```text
ofc_ase256.h:28:10: fatal error: openssl/evp.h: No such file or directory
```

该失败来自既有安全解包模块。当前 N725A 工具链和 SDK 中未提供目标 ABI 的 OpenSSL 开发头文件/库，不能使用宿主 x86 OpenSSL 替代。

## 遗留验证

- 补齐 N725A ABI 匹配的 OpenSSL 开发包后重跑整应用构建。
- 在目标设备执行 MT004、MT005，确认自动重启、A/B 切换和同步。
