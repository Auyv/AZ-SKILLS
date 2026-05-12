---
name: docker-image-build
description: Docker 镜像制作技能。当用户提到"制作 Docker 镜像"、"构建镜像"、"编写 Dockerfile"、"创建 docker 镜像"、"制作私有镜像"、"构建开发环境镜像"、"SDK 镜像制作"、"交叉编译镜像"、"迁移镜像到仓库"、"镜像版本管理"等场景时，必须激活此技能。此外，当用户提供基础镜像名称、SDK 包、工具链等信息并要求生成镜像时，也应使用此技能。
---

# Docker 镜像制作技能

## 工作流程

```
收集需求 → 生成文件 → 验证构建 → 推送仓库
```

## 第一步：收集需求

制作镜像前，必须收集以下信息：

| 项目 | 说明 | 示例 |
|------|------|------|
| **基础镜像** | 父镜像地址 | `10.10.151.101:80/library/build/jenkins/2004` |
| **镜像名称** | 完整镜像名（不含标签） | `<仓库>/<项目>/<功能>/<平台>/<内容>` |
| **标签版本** | 镜像版本号 | `1.0`, `r01a02` |
| **作者** | 制作人员 | 张三 |
| **SDK包** | SDK或源码包名称 | `ql-ol-sdk.tar.gz` |
| **SDK路径** | 容器内安装路径 | `/home/ql-ol-sdk` |
| **工具链** | 交叉编译工具链(如有) | `ql-ag35-toolchain.tar.gz` |
| **工具链路径** | 工具链安装路径 | `/opt/ql_crosstools` |
| **构建命令** | 需要执行的编译命令 | `make`, `bash build.sh` |
| **额外文件** | 其他需要复制的文件 | 配置文件、脚本等 |

## 第二步：生成文件

每次镜像制作会生成以下文件：

```
<项目目录>/
├── Dockerfile           # 镜像构建文件
├── docker_build.bat     # Windows 构建脚本
├── docker_build.sh      # Linux 构建脚本（可选）
├── README.md            # 镜像说明文档
└── <依赖文件>...         # SDK、工具链等
```

使用模板：
- **基础镜像**：使用 `templates/Dockerfile-basic.md`
- **SDK+工具链镜像**：使用 `templates/Dockerfile-sdk.md`
- **Windows 构建脚本**：使用 `templates/docker_build.bat`
- **Linux 构建脚本**：使用 `templates/docker_build.sh`
- **镜像说明文档**：使用 `templates/README-template.md`

## 第三步：验证与推送

```bash
# 本地构建测试
docker build -t <镜像名>:<标签> .

# 推送仓库
docker push <镜像名>:<标签>
```

## 命名规范

### 镜像命名

```
镜像名: 功能 / 平台 / 内容 : 标签
```

**示例：**
- `build/rv1126/base:1.0`
- `build/ag35/sdk:r01a02`
- `build/eg800g/toolchain:v2.0`

### 仓库命名

```
仓库标签: IP[:PORT] / 项目名 / 镜像名 : 标签
```

**示例：**
- `10.10.151.101:80/library/build/rv1126/base:1.0`
- `10.10.151.101:80/library/build/ag35/sdk:r01a02`

### 容器命名

```
容器名: 功能 - 平台 - 内容 - 标签
```

**示例：**
- `build-rv1126-base-1.0`
- `build-ag35-sdk-r01a02`

## Dockerfile 常用指令

| 指令 | 说明 | 示例 |
|------|------|------|
| `FROM` | 指定基础镜像 | `FROM 10.10.151.101:80/library/build/jenkins/2004` |
| `LABEL` | 镜像元数据 | `LABEL Author="姓名" Version="1.0"` |
| `COPY` | 复制文件到容器 | `COPY sdk /home/sdk` |
| `RUN` | 执行构建命令 | `RUN apt update && apt install -y cmake` |
| `WORKDIR` | 设置工作目录 | `WORKDIR /home/project` |
| `ENV` | 设置环境变量 | `ENV PATH="/opt/tools:$PATH"` |
| `USER` | 设置用户 | `USER root` |
| `ARG` | 构建参数 | `ARG VERSION=1.0` |

## 常用命令速查

### 构建相关

| 命令 | 说明 |
|------|------|
| `docker build -t <镜像名>:<标签> .` | 构建镜像 |
| `docker images` | 查看本地镜像 |
| `docker rmi <镜像ID>` | 删除镜像 |

### 容器相关

| 命令 | 说明 |
|------|------|
| `docker run -it <镜像> /bin/bash` | 创建并进入容器 |
| `docker exec -it <容器> /bin/bash` | 进入运行中的容器 |
| `docker ps -a` | 查看所有容器 |
| `docker start <容器>` | 启动容器 |
| `docker stop <容器>` | 停止容器 |

### 仓库相关

| 命令 | 说明 |
|------|------|
| `docker login <IP:PORT>` | 登录镜像仓库 |
| `docker tag <镜像> <仓库镜像>` | 打标签 |
| `docker push <镜像>` | 推送镜像 |
| `docker pull <镜像>` | 拉取镜像 |

---

*Docker 镜像制作技能 - 使用模板时请参考 templates/ 目录*
