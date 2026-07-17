# 构建链路详解

本文档描述 `local_build_mpu.bat` 触发的完整 MPU 构建链路，
仅供 AI Agent 在排查深层构建问题时按需查阅。

## 调用链路

```
Windows: local_build_mpu.bat
  │
  ├─ 参数: (无) = Release 构建 | "clean" = 清理构建
  │
  └─ docker exec -i sim7805-wit803_001 bash -c
       "cd /home/sei/autobuild/k3_sim7805 &&
        ./k3_sim7805/build/mpu/build.sh k3_sim7805 WIT803-001"
       │
       └─ build.sh (容器内)
            ├─ 读取 config/WIT803-001/list.env (产品配置)
            ├─ 读取 sdk/sim7805.env (工具链环境)
            ├─ source k3_sim7805/build/mpu/build_sim7805.sh
            │    ├─ 编译 lib/ 公共库 (cbb_srt, project_config, platform_common_utils...)
            │    ├─ 编译 app/ 应用 (hqtoolbox, hqinit, canstorage_app, hq_fota_app...)
            │    ├─ 编译 kernel (boot.img)
            │    ├─ 编译 rootfs (system.img, userdata.img, oemdata.ubi)
            │    ├─ 打包 firmware
            │    └─ 打包 fota (update_ota.zip)
            └─ 打包 WIT803-001_YYYYMMDDHHmmss.zip → k3_sim7805/package/package.zip
```

## 本技能作用域

本技能只覆盖到 `build.sh` 入口执行完毕并确认成功返回。

**不覆盖**：
- `build_sim7805.sh` 内部的具体编译命令
- 各 lib/app 的 CMake 配置和编译参数
- kernel/rootfs 的具体制作流程
- firmware 分区表配置

原因：这些细节随项目和 SDK 版本变化，不应固化到技能中。

## 关键环境变量

| 变量 | 来源 | 示例值 |
|------|------|--------|
| `G_PROJECT_NAME` | build.sh 参数 $1 | `k3_sim7805` |
| `G_PRODUCT_NAME` | build.sh 参数 $2 | `WIT803-001` |
| `G_BUILD_MODEL` | 可选的 $3 或 list.env | `sim7805` |
| `T_PROJ_TOP_PATH` | $(pwd) | `/home/sei/autobuild/k3_sim7805` |
| `SDK_PATH` | sdk.sim7805.env | SDK 工具链路径 |
| `BUILD_PRODUCT_NAME` | 继承 G_PRODUCT_NAME | `WIT803-001` |
| `BUILD_PLATFORM_NAME` | 继承 G_BUILD_MODEL | `sim7805` |

## 构建产物目录

```
autobuild/k3_sim7805/
├── build_output/
│   ├── lib/release/       # 编译生成的 .so / .a
│   ├── app/release/       # 编译生成的可执行文件 + .dbg + .map
│   └── include/           # 导出头文件
├── k3_sim7805/
│   ├── package/
│   │   └── package.zip    # 最终构建包 (≈260MB)
│   └── config/WIT803-001/
│       └── list.env       # 产品配置文件
└── WIT803-001_*.zip       # 构建输出 ZIP（与 package.zip 内容相同）
```

## SDK 依赖路径

```
autobuild/local/k3_sim7805/sdk_WIT803-001/sdk/sim7805/
├── include/               # SDK 头文件
├── lib/release/           # SDK 预编译库
└── env/                   # 工具链环境脚本
```
