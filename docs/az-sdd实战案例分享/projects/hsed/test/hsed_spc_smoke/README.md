# hsed_spc_smoke 测试工程

## 测试目的

本工程是在 AG35 目标机运行的 HSED SPC 基础冒烟测试客户端。

默认测试内容：

- 初始化和结束 SPC 客户端运行环境；
- 打开和关闭 HSED SPC 连接；
- 创建和销毁 HSE 功能对象；
- 设置并读取 AES128 加密模式整数属性；
- 设置并读取 USER_ID 字节属性；
- 分别检查 SPC 传输返回值和 HSED 业务返回值；
- 验证对象和连接的完整生命周期。

传入 `--aes` 后额外验证：

- 使用 AES128 对一个 16 字节数据块加密；
- 对密文进行解密；
- 比较解密结果与原始数据是否一致。

`--aes` 会实际访问 CIU98 密钥槽位，必须确保服务端默认 AES128 槽位已配置有效密钥。

## 编译前提

- 在 HSED 项目构建 Docker 中编译；
- `mpu/build_output/include` 已生成；
- `mpu/build_output/lib/debug` 中已有 SPC 及基础库；
- 使用 AG35 `platform_ag35_cetda.cmake` 交叉工具链。

如果依赖尚未生成，先在 Windows 执行：

```bat
C:\HQ_Workspace\YLB04_A6\code\task-jpz_dev\mpu\az_build_linux_hsed.bat
```

## 编译命令

```bash
cd /home/sei/code/task-jpz_dev/mpu/app/hsed/test/hsed_spc_smoke

cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/home/sei/code/task-jpz_dev/mpu/sdk/env/platform_ag35_cetda.cmake \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel "$(nproc)"
```

显示完整编译命令：

```bash
cmake --build build --verbose --parallel "$(nproc)"
```

## 编译产物

```text
build/hsed_spc_smoke
```

该程序是 ARM Linux 可执行文件，不能直接在构建容器的 x86 环境运行。

## 目标机运行

运行前先启动正常模式的 HSED 服务：

```bash
./hsed --cfg ./hsed.ini
```

另开终端执行基础冒烟：

```bash
./hsed_spc_smoke
```

执行包含 AES128 往返的冒烟：

```bash
./hsed_spc_smoke --aes
```

当设备运行库不在系统默认搜索路径时：

```bash
LD_LIBRARY_PATH=/mnt/card/back/app/hq/lib:$LD_LIBRARY_PATH \
./hsed_spc_smoke
```

退出码为 `0` 且出现 `[PASS]` 表示测试通过。
