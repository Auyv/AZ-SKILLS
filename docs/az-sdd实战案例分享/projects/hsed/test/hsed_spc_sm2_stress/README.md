# hsed_spc_sm2_stress 测试工程

## 测试目的

本工程是在 AG35 目标机运行的 HSED SPC 多通道 SM2 签名压力测试客户端。

主要验证：

- 单进程创建多个工作线程；
- 每个线程建立独立 SPC 连接并创建独立 HSE 对象；
- 每个通道生成不同的随机 16 字节 USER_ID；
- 所有通道对相同原文执行 SM2 带 ZA 签名；
- 不同 USER_ID 对应不同 ZA，验证通道数据隔离；
- 记录每次 `Sign` RPC 的传输返回值、业务返回值、微秒延迟和每秒调用次数；
- 汇总每个通道及全部通道的成功数、失败数、最小/最大/平均延迟和吞吐率；
- 使用固定 USER_ID 额外生成公钥、原文和签名样本，供外部带 Z 验签。

默认参数：

| 参数 | 默认值 | 范围 |
|---|---:|---:|
| 通道数 | 4 | 1～8 |
| 每通道签名次数 | 10 | 1～1000000 |
| 原文长度 | 32 字节 | 1～4096 |
| SM2 公钥槽位 | 固定 `11` | 只读 |
| SM2 私钥槽位 | 固定 `12` | 只读 |

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
cd /home/sei/code/task-jpz_dev/mpu/app/hsed/test/hsed_spc_sm2_stress

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
build/hsed_spc_sm2_stress
```

该程序是 ARM Linux 可执行文件，不能直接在构建容器的 x86 环境运行。

## 目标机运行

运行前必须启动正常模式的 HSED 服务，并确保公私钥槽位是一对有效 SM2 密钥。

使用默认参数：

```bash
./hsed_spc_sm2_stress
```

指定测试参数：

```bash
./hsed_spc_sm2_stress \
  --channels 4 \
  --iterations 100 \
  --message-size 32
```

SM2 固定读取公钥槽位 `11`、私钥槽位 `12`。测试启动时会读取并校验这两个只读属性，不提供修改 KEY_ID 的命令行参数。

当设备运行库不在系统默认搜索路径时：

```bash
LD_LIBRARY_PATH=/mnt/card/back/app/hq/lib:$LD_LIBRARY_PATH \
./hsed_spc_sm2_stress --channels 4 --iterations 100
```

退出码为 `0` 表示所有通道签名成功、固定验签样本生成成功且通道隔离检查通过。
