# hq-autobuild-mcu 手动操作指南

## 前置条件

- Windows 主机可运行 Python 3。
- CI 仓库内存在 `<项目名>\build\mcu\pack.bat`。
- 若选择 `build-pack`，同目录还需存在 `build.bat`。

## 生成脚本

在 `YLB04_A6` 项目根目录执行：

```powershell
python .\skills\hq-autobuild-mcu\scripts\create_autobuild_mcu.py --repository-root "C:\目标\CI仓库" --product-name "YLB04-A6" --project-name "项目目录名" --build-model "ac78428" --mode pack
```

参数说明：

- `--repository-root`：放置 `autobuild-mcu.bat` 的 CI 仓库根目录。
- `--product-name`：产品配置目录名。
- `--project-name`：可省略；仅当仓库中唯一存在 `build\mcu` 时才能自动探测。
- `--build-model`：可省略，默认 `mcu`。
- `--mode pack`：只调用 `pack.bat`。
- `--mode build-pack`：先调用 `build.bat`，成功后调用 `pack.bat`。
- `--output-path`：可修改文件名，但文件必须位于 CI 仓库根目录。
- `--force`：备份并覆盖已有输出；使用前先审阅原文件。

## 验证

先读取生成文件，确认 `G_PROJECT_NAME`、`G_PRODUCT_NAME`、`G_BUILD_MODEL` 和调用模式正确：

```powershell
Get-Content -LiteralPath "C:\目标\CI仓库\autobuild-mcu.bat"
```

用户明确要求执行时，优先直接运行生成脚本：

```powershell
& "C:\目标\CI仓库\autobuild-mcu.bat"
```

## 常见错误

- `ERR_PROJECT_NOT_FOUND`：补充 `--project-name`，或检查项目是否包含 `build\mcu`。
- `ERR_BUILD_ENTRY_NOT_FOUND`：确认目标工程的 `pack.bat`/`build.bat` 入口。
- `ERR_OUTPUT_EXISTS`：审阅现有文件；确认允许覆盖后加 `--force`。
- `ERR_PERMISSION_DENIED`：更换可写目录或修正权限，不要绕过权限保护。
