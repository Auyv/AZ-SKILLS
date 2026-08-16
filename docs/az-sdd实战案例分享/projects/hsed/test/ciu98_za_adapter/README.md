# ciu98_za_adapter 测试工程

## 测试目的

本工程是 CIU98 厂商适配层的 SM2 ZA 主机单元测试，不访问真实 CIU98 芯片。

主要验证：

- SM2 原文签名前先从指定公钥槽位导出 64 字节公钥；
- 使用 16 字节 USER_ID、公钥和 SM2 曲线参数计算 ZA；
- 厂商签名输入按 `ZA || 原文` 组织；
- 私钥槽位、公钥槽位、SM3 算法和 USER_ID 正确传递；
- SM2 签名输出为 64 字节，`r_size` 和 `s_size` 各为 32 字节；
- `DIGEST` 输入模式当前返回不支持；
- 公钥导出失败、公钥长度异常和 ZA 计算失败时，返回值及调用链正确停止。

测试通过桩函数模拟 CIU98 SDK，不依赖 SPC 服务和真实硬件。

## 编译环境

在 HSED 项目构建 Docker 的 Linux 环境中使用本机编译器编译，不要指定 AG35 交叉工具链。

工程需要以下源码目录存在：

```text
/home/sei/code/task-jpz_dev/mpu/lib/bsc_hse_plug/vendor/ciu98-ylb04_a6
```

## 编译命令

```bash
cd /home/sei/code/task-jpz_dev/mpu/app/hsed/test/ciu98_za_adapter

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel "$(nproc)"
```

显示完整编译命令：

```bash
cmake --build build --verbose --parallel "$(nproc)"
```

## 运行测试

通过 CTest 运行：

```bash
ctest --test-dir build --output-on-failure
```

或直接运行：

```bash
./build/ciu98_za_adapter_test
```

成功时输出：

```text
PASS: CIU98 ZA adapter tests
```

## 编译产物

```text
build/ciu98_za_adapter_test
```
