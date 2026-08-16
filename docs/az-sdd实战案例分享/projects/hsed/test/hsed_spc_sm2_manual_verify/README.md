# hsed_spc_sm2_manual_verify 测试工程

## 测试目的

本工程在 AG35 目标机通过生成的 `HcHsed_*` SPC 客户端接口执行三对象
SM2 带 ZA 验签测试：

1. 在同一 SPC 连接中创建 3 个 HSE 对象。
2. 为每个对象随机生成并设置不同的 16 字节 `USER_ID`。
3. 校验固定 SM2 公钥槽 11、私钥槽 12，并导出公钥。
4. 要求 3 个对象导出的公钥长度和内容完全相同。
5. 每组分别生成 1024 字节随机原文并执行 SM2 签名。
6. 输出 `USER_ID`、公钥、原文、签名以及 `r`、`s`。
7. 默认使用目标机 OpenSSL 3 逐组执行 SM2 ZA 验签，三组均通过才返回 0。

测试程序不修改 KEY_ID，也不生成、导入或覆盖密钥。

## 编译

优先在 MPU 根目录执行：

```text
az_build_linux_hsed.bat
```

该脚本会先编译 CIU98 插件，再编译 HSED 和本测试工程。

## 运行

正常模式启动 HSED 后，在 MPU 根目录执行：

```text
adb_exec_hsed_spc_sm2_manual_verify.bat
```

程序会打印网页人工验签所需的全部参数，并使用设备 OpenSSL 3 自动复核。
成功输出包括：

```text
[PASS] group=1 OpenSSL SM2 ZA verification
[PASS] group=2 OpenSSL SM2 ZA verification
[PASS] group=3 OpenSSL SM2 ZA verification
[PASS] all SM2 verifications passed
```

若需要网页逐组人工确认，在设备端给测试程序传入 `--manual`。

## 编译产物

```text
test/hsed_spc_sm2_manual_verify/build/hsed_spc_sm2_manual_verify
```
