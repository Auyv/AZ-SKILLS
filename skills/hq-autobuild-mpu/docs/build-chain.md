# 通用 MPU 自动构建链路

```text
Windows: <ci-repository>/build/autobuild-mpu.bat
  ├─ where docker / docker info
  ├─ docker inspect <config.json 中的容器名>
  ├─ 必要时 docker start <容器名>
  └─ docker exec -i -w <容器内构建根目录> <容器名>
       bash ./<project>/build/mpu/build.sh <project> <product> <model>
```

参数来源：

| 参数 | 权威来源 |
|---|---|
| 容器名 | `tools/docker_cmd/config.json` 的 `containers[].name` |
| 容器挂载点 | `containers[].volumes` 中当前工作区对应的目标路径 |
| 工程名 | `<ci-repository>/build/mpu/build.sh` 所属持续集成仓库目录 |
| 产品名 | `<project>/config/<product>/list.env` 的目录名 |
| 构建模型 | `list.env` 中的 `G_PRODUCT_BUILD_MPU_MODEL` |

生成器只负责到 `build.sh` 返回。具体 lib/app、SDK、kernel、rootfs、firmware 和 FOTA 链路属于项目实现，不固化在通用技能中。

## 为什么不使用容器 ID

容器重建后 ID 会变化，配置中的容器名才是稳定接口。生成脚本直接把容器名传给 `docker inspect`、`docker start` 和 `docker exec`。

## 为什么生成和构建分离

修改脚本是快速、可静态验证的操作；完整 MPU 编译通常耗时数分钟且会生成大量产物。只有用户明确要求构建时才运行生成的入口。
