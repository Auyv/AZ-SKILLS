# bsc_hse_runtime 测试工程

## 测试目的

本工程是 BSC HSE 业务层的主机单元测试，不连接 SPC 服务，也不访问 CIU98 硬件。

主要验证：

- `BscHseInit`、`BscHseDone`、对象创建和销毁生命周期；
- 重复初始化和初始化结束后的错误处理；
- 属性 ID 到算法的映射；
- 新建对象的全部 46 个属性具备确定的默认状态，其中 40 个 Int 属性逐项校验；
- CIU98 槽位 1～14 按算法顺序固定分配，所有 KEY_ID 属性只读；
- 客户端不能通过公钥导入接口覆盖固定槽位；
- 密钥槽位、加密模式、填充方式、摘要算法、签名输入模式和公钥格式的参数校验；
- SM2 公钥槽位、私钥槽位、USER_ID 和签名输出长度向厂商适配层的传递；
- SM2 `DIGEST` 模式当前返回不支持；
- 8 个线程并发调用时，BSC 对厂商接口保持串行访问；
- 对象池和运行时资源的基本管理。

测试使用假的厂商接口和 SFC 互斥锁适配，不依赖真实设备。

## 编译环境

在 HSED 项目构建 Docker 的 Linux 环境中使用本机编译器编译，不要指定 AG35 交叉工具链。

## 编译命令

```bash
cd /home/sei/code/task-jpz_dev/mpu/app/hsed/test/bsc_hse_runtime

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
./build/bsc_hse_runtime_test
```

成功时输出：

```text
[PASS] runtime lifecycle and max_active_calls=1
```

## 编译产物

```text
build/bsc_hse_runtime_test
```
