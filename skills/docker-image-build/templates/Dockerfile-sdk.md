# SDK + 工具链镜像 Dockerfile 模板

```dockerfile
# Dockerfile 模板 - SDK + 工具链镜像
# 使用说明：将以下占位符替换为实际值

FROM {{基础镜像地址}}
LABEL Author="{{作者姓名}}" \
      Version="{{版本号}}" \
      SdkPath="{{SDK安装路径}}" \
      ToolchainPath="{{工具链路径}}" \
      Description="SDK + 工具链开发环境镜像"

# 切换到 root 用户进行系统操作
USER root

# 创建 SDK 和工具链目录
RUN mkdir -p {{SDK安装路径}}
RUN mkdir -p {{工具链路径}}

# 复制 SDK 到容器
# 注意：使用 --chown 设置文件所有者
COPY --chown=sei:sei {{SDK包名}} {{SDK安装路径}}/

# 复制工具链到容器（如果有）
# COPY --chown=sei:sei {{工具链包名}} {{工具链路径}}/

# 设置环境变量
ENV SDK_PATH={{SDK安装路径}}
ENV PATH={{SDK安装路径}}/bin:{{工具链路径}}/bin:$PATH
ENV LD_LIBRARY_PATH={{SDK安装路径}}/lib:{{工具链路径}}/lib:$LD_LIBRARY_PATH

# 创建 cmake 软链接（如果需要）
RUN ln -s {{cmake路径}}/bin/cmake /usr/bin/cmake \
    && ln -s {{cmake路径}}/bin/ctest /usr/bin/ctest

# 设置文件权限
RUN chown -R root:root {{SDK安装路径}} \
    && chmod -R 775 {{SDK安装路径}}

# 切换回普通用户
USER sei

# 设置工作目录
WORKDIR {{工作目录}}

# 验证安装
RUN echo "SDK Path: $SDK_PATH" \
    && cmake --version \
    && echo "SDK + Toolchain installation completed!"
```

## 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| `{{基础镜像地址}}` | 父镜像地址 | `10.10.151.101:80/library/build/jenkins/2004` |
| `{{作者姓名}}` | 制作人员姓名 | 张三 |
| `{{版本号}}` | 镜像版本 | `1.0`, `r01a02` |
| `{{SDK安装路径}}` | SDK 在容器内的安装路径 | `/home/ql-ol-sdk`, `/opt/sdk` |
| `{{工具链路径}}` | 工具链安装路径 | `/opt/ql_crosstools`, `/opt/toolchain` |
| `{{SDK包名}}` | SDK 包文件名 | `ql-ol-sdk.tar.gz`, `sdk-v2.0.tar.gz` |
| `{{工具链包名}}` | 工具链包文件名 | `ql-ag35-toolchain.tar.gz` |
| `{{cmake路径}}` | cmake 安装路径 | `/opt/cmake`, `/usr` |
| `{{工作目录}}` | 默认工作目录 | `/home/project` |

## 常见 SDK 和工具链组合

### RV1126 平台

```dockerfile
FROM 10.10.151.101:80/library/build/jenkins/2004
LABEL Author="张三" \
      Version="1.0" \
      SdkPath="/home/rv1126-sdk" \
      ToolchainPath="/opt/rv1126_toolchain"

COPY --chown=sei:sei rv1126-sdk.tar.gz /home/rv1126-sdk/
COPY --chown=sei:sei rv1126-toolchain.tar.gz /opt/rv1126_toolchain/

ENV SDK_PATH=/home/rv1126-sdk
ENV PATH=/opt/rv1126_toolchain/bin:$PATH
```

### AG35 平台

```dockerfile
FROM 10.10.151.101:80/library/build/jenkins/2004
LABEL Author="李四" \
      Version="r01a02" \
      SdkPath="/home/ql-ol-sdk" \
      ToolchainPath="/opt/ql_crosstools"

COPY --chown=sei:sei ql-ol-sdk.tar.gz /home/ql-ol-sdk/
COPY --chown=sei:sei ql-ag35-toolchain.tar.gz /opt/ql_crosstools/

ENV SDK_PATH=/home/ql-ol-sdk
ENV PATH=/opt/ql_crosstools/bin:$PATH
```
