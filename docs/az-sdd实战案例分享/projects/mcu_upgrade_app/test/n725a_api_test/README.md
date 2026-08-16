# N725A FOTA API 手动测试工具

该目录是独立的 N725A/Neoway FOTA API 手动测试工程，不参与
`mcu_upgrade_app` 的业务构建。交互形式参考 `spc_mcud_clt_test`，通过数字菜单逐项调用
`/opt/neoway-arm-oe-linux/include/nwy_fota.h` 中的公开接口。

## 接口菜单

| 选项 | 接口 | 风险 |
| --- | --- | --- |
| 1 | `nwy_get_upgrade_result` | 只读 |
| 2 | `nwy_get_fota_result` | 只读 |
| 3 | `nwy_get_active_system_slot` | 只读 |
| 4 | `nwy_get_damaged_state` | 只读 |
| 5 | `nwy_fota_ua` | 启动完整包升级，执行前二次确认 |
| 6 | `nwy_fota_update` | 启动拼接包升级，执行前二次确认 |
| 7 | `nwy_set_system_sync` | 修改 A/B 同步状态，执行前二次确认 |
| 8 | `nwy_set_active_system_slot` | 修改下次启动分区，执行前二次确认 |
| 0 | 退出 | 无 |

程序启动时调用 `nwy_sdk_init()`，退出时调用 `nwy_sdk_deinit()`。设备端输出仅使用
ASCII，避免串口或 ADB shell 的中文编码差异。

## 参数输入约定

- 枚举值只通过数字菜单选择。当前 `nwy_set_active_system_slot` 使用 `1` 选择
  `NWY_SYSTEM_A`、`2` 选择 `NWY_SYSTEM_B`，程序内部再转换为 SDK 要求的 `'a'`/`'b'`。
- 字符串参数会显示提示符并等待用户输入。`nwy_fota_ua` 和 `nwy_fota_update` 的升级包
  路径均按完整字符串读取，不使用预设路径。
- 普通数值参数（超时、包偏移和包长度）等待用户输入数字，支持十进制和 `0x` 前缀
  十六进制。
- 危险接口在参数输入完成后仍需输入数字 `1` 二次确认；输入 `0` 或其他值取消调用。

## 在 N725A SDK 容器中构建

将本目录复制或挂载进含 Neoway SDK 的容器后执行：

```sh
cd /path/to/n725a_api_test
chmod +x build.sh
./build.sh Debug
```

默认 SDK 根目录是 `/opt/neoway-arm-oe-linux`，也可通过
`NEOWAY_APP_SDK_PATH` 和 `CROSS_CC` 覆盖。产物为：

```text
build/n725a_fota_api_test
```

## 手动部署和运行

以下命令仅作示例，应在确认测试设备、升级包和测试窗口后由测试人员执行：

```sh
adb push build/n725a_fota_api_test /tmp/
adb shell chmod +x /tmp/n725a_fota_api_test
adb shell /tmp/n725a_fota_api_test
```

建议先执行选项 1-4。选项 5-8 会触发升级或改变 A/B 启动状态，必须使用与目标
N725A 固件匹配的升级包，并确保设备供电稳定。工具不会自动下载升级包，也不会自动
刷机、重启或切换分区。
