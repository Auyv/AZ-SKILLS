# rte_hse_loader 测试工程

## 测试目的

本工程是 RTE HSE 厂商动态库加载器的主机单元测试，不访问真实 CIU98 芯片。

构建时会同时生成三种假的厂商动态库：

| 动态库 | 用途 |
|---|---|
| `rte_hse_valid_vendor` | 符号和 ABI 均有效 |
| `rte_hse_missing_symbol_vendor` | 缺少必需导出符号 |
| `rte_hse_invalid_abi_vendor` | 可加载，但 ABI 版本不兼容 |

主要验证：

- 动态库文件不存在时返回设备不可用；
- 动态库缺少必需符号时返回不支持；
- 动态库能够加载但 ABI 不兼容时，由 BSC 初始化拒绝；
- 有效厂商动态库可以完成 RTE 加载和 BSC 初始化；
- `RteHseDone` 正确卸载动态库并清理绑定；
- `RteHseSelfTest` 能通过有效厂商库完成自检调用。

## 编译环境

在 HSED 项目构建 Docker 的 Linux 环境中使用本机编译器编译，不要指定 AG35 交叉工具链。

## 编译命令

```bash
cd /home/sei/code/task-jpz_dev/mpu/app/hsed/test/rte_hse_loader

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel "$(nproc)"
```

显示完整编译命令：

```bash
cmake --build build --verbose --parallel "$(nproc)"
```

## 运行测试

推荐通过 CTest 运行。CTest 会自动把三个测试动态库的实际路径传给测试程序：

```bash
ctest --test-dir build --output-on-failure
```

需要直接运行时：

```bash
./build/rte_hse_loader_test \
  ./build/librte_hse_valid_vendor.so \
  ./build/librte_hse_missing_symbol_vendor.so \
  ./build/librte_hse_invalid_abi_vendor.so
```

退出码为 `0` 表示全部加载场景符合预期。

## 编译产物

```text
build/rte_hse_loader_test
build/librte_hse_valid_vendor.so
build/librte_hse_missing_symbol_vendor.so
build/librte_hse_invalid_abi_vendor.so
```
