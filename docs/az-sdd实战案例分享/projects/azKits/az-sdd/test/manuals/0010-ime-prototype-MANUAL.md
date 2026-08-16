# 0010 中文 IME 原型门禁手册

## 目的

在完整 TUI 页面开发前，确认 Bubble Tea v2 在 Windows Terminal 和传统 Console Host 中能够区分中文输入法上屏与脚本执行，满足 0012 MT005 和统一技术规范的 IME 门禁。

## 前置条件

1. Windows 10/11，启用“微软拼音”。
2. 已执行 `build_azKits.bat`，存在 `dist\azkits.exe`。
3. 使用 `examples\ime-toolbox.yaml`，其中包含“设备初始化”和“设备连接检查”。

## MT-IME-001 Windows Terminal

1. 在 Windows Terminal 的 PowerShell 中进入项目根目录。
2. 执行 `.\verify_ime.bat`。
3. 输入 `/`，确认显示两个候选。
4. 切换到微软拼音，输入 `shebei`，分别测试组词、候选翻页和选字。
5. 使用空格上屏“设备”，确认候选收敛且脚本没有启动。
6. 清空后重复输入，使用输入法的 `Enter` 完成上屏，确认该次上屏没有同时启动脚本。
7. 上屏完成后，再单独按一次 `Enter`，确认只启动当前高亮脚本。
8. 重复测试 `Tab`，确认只补全为 `/device-init` 或 `/device-check`，不运行脚本。
9. 按 `Esc`，确认关闭候选且不执行；按 `Ctrl+C` 退出并恢复终端。

## MT-IME-002 传统 Console Host

1. 按 `Win+R`，输入 `cmd.exe` 并打开传统命令提示符。
2. `cd /d C:\JPZ_Workspace\AZ_TOOLS\azKits`。
3. 执行 `verify_ime.bat`。
4. 完整重复 MT-IME-001 的第 3-9 步。

## 判定标准

| 检查项 | Windows Terminal | Console Host |
|--------|------------------|--------------|
| 中文组词、翻页、选字无乱码 | 待填写 | 待填写 |
| 空格上屏只更新候选 | 待填写 | 待填写 |
| 输入法 Enter 上屏不执行脚本 | 待填写 | 待填写 |
| 独立 Enter 只执行高亮脚本 | 待填写 | 待填写 |
| Tab 只补全 | 待填写 | 待填写 |
| Esc 不执行 | 待填写 | 待填写 |
| Ctrl+C 恢复终端 | 待填写 | 待填写 |

所有项目在两个终端均通过后，GATE-IME-001 才能关闭并进入完整 TUI 开发。任一终端发生“上屏同时执行”，请记录终端类型、输入法操作、实际输入文本和日志，并保持开发状态不变。
