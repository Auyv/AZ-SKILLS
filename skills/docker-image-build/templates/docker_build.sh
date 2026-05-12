#!/bin/bash
# ==========================================
#   Docker 镜像构建脚本
#   使用说明：修改下面的变量值后运行
# ==========================================

# 镜像名称和标签
IMAGE_NAME="{{镜像仓库}}/{{项目名}}/{{功能}}/{{平台}}/{{内容}}"
IMAGE_TAG="{{版本号}}"

# Docker 构建上下文路径（可选，默认当前目录）
BUILD_CONTEXT="."

# 显示配置信息
echo "=========================================="
echo "  Docker 镜像构建脚本"
echo "=========================================="
echo ""
echo "配置信息："
echo "  镜像名称: $IMAGE_NAME"
echo "  镜像标签: $IMAGE_TAG"
echo "  构建上下文: $BUILD_CONTEXT"
echo ""

# 询问是否继续
read -p "确认构建？(Y/N): " CONFIRM
if [[ ! "$CONFIRM" =~ ^[Yy]$ ]]; then
    echo "取消构建。"
    exit 0
fi

echo ""
echo "[$(date '+%Y-%m-%d %H:%M:%S')] 开始构建镜像..."
docker build -t ${IMAGE_NAME}:${IMAGE_TAG} ${BUILD_CONTEXT}

# 检查构建结果
if [ $? -eq 0 ]; then
    echo ""
    echo "=========================================="
    echo "  构建成功！"
    echo "  镜像: ${IMAGE_NAME}:${IMAGE_TAG}"
    echo "=========================================="
    
    # 显示镜像信息
    echo ""
    echo "镜像信息："
    docker images ${IMAGE_NAME} --format "table {{.Repository}}:{{.Tag}}\t{{.Size}}\t{{.CreatedAt}}"
else
    echo ""
    echo "=========================================="
    echo "  构建失败！"
    echo "=========================================="
    exit 1
fi

echo ""
# 可选：推送到仓库
read -p "是否推送到仓库？(Y/N): " PUSH
if [[ "$PUSH" =~ ^[Yy]$ ]]; then
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] 推送镜像到仓库..."
    docker push ${IMAGE_NAME}:${IMAGE_TAG}
    if [ $? -eq 0 ]; then
        echo ""
        echo "推送成功！"
    else
        echo ""
        echo "推送失败！"
    fi
fi
