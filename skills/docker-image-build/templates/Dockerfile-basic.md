# 基础开发环境镜像 Dockerfile 模板

```dockerfile
# Dockerfile 模板 - 基础开发环境镜像
# 使用说明：将以下占位符替换为实际值

FROM {{基础镜像地址}}
LABEL Author="{{作者姓名}}" \
      Version="{{版本号}}" \
      Description="{{镜像描述}}"

# 创建必要的目录
RUN mkdir -p {{目标目录}}

# 复制文件到容器
# 示例：COPY sdk /home/sdk
COPY {{源文件1}} {{目标路径1}}
COPY {{源文件2}} {{目标路径2}}

# 设置环境变量（可选）
ENV {{变量名}}={{变量值}}

# 安装系统依赖（可选）
RUN apt-get update && apt-get install -y \
    {{需要的包}} \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR {{工作目录}}

# 执行初始化命令（可选）
RUN {{初始化命令}}

# 设置默认工作用户（可选）
# USER {{用户名}}
```

## 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| `{{基础镜像地址}}` | 父镜像地址 | `10.10.151.101:80/library/build/jenkins/2004` |
| `{{作者姓名}}` | 制作人员姓名 | 张三 |
| `{{版本号}}` | 镜像版本 | `1.0`, `r01a02` |
| `{{镜像描述}}` | 镜像功能描述 | 开发环境镜像 |
| `{{目标目录}}` | 需要创建的目录 | `/opt/app`, `/home/sdk` |
| `{{源文件}}` | 要复制到容器的文件 | `app.tar.gz`, `config.json` |
| `{{目标路径}}` | 容器内的目标路径 | `/opt/app`, `/home/config` |
| `{{变量名}}` | 环境变量名 | `PATH`, `JAVA_HOME` |
| `{{变量值}}` | 环境变量值 | `/opt/tools:$PATH` |
| `{{工作目录}}` | 默认工作目录 | `/home/project` |
