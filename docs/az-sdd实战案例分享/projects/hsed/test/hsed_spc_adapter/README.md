# hsed_spc_adapter 测试工程

## 测试目的

本工程是 HSED SPC 适配层的主机单元测试，不启动 SPC 服务，也不访问 CIU98 硬件。

主要验证：

- SPC 适配层初始化、重复初始化和结束清理；
- 客户端连接接入及空连接拒绝；
- HSE 对象创建和销毁；
- 对象句柄只能由创建它的连接使用；
- 整数属性和字节属性的设置、读取；
- 加密、解密、签名、公钥导出和公钥导入接口转发；
- 连接断开时自动销毁该连接拥有的全部对象；
- 断开后的旧句柄被拒绝；
- 对象池耗尽时返回 `NO_RESOURCE`；
- 服务结束时清理剩余对象。

测试使用假的 BSC HSE 实现和互斥锁适配，不依赖真实 SPC 连接。

## 编译环境

在 HSED 项目构建 Docker 的 Linux 环境中使用本机编译器编译，不要指定 AG35 交叉工具链。

## 编译命令

```bash
cd /home/sei/code/task-jpz_dev/mpu/app/hsed/test/hsed_spc_adapter

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel "$(nproc)"
```

显示完整编译命令：

```bash
cmake --build build --verbose --parallel "$(nproc)"
```

## 运行测试

当前 `CMakeLists.txt` 未注册 CTest 用例，直接运行测试程序：

```bash
./build/hsed_spc_adapter_test
```

成功时输出：

```text
[PASS] all SPC adapter operations, ownership, disconnect cleanup, and pool exhaustion
```

## 编译产物

```text
build/hsed_spc_adapter_test
```
