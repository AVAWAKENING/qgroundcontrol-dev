#!/usr/bin/env bash

set -e

DOCKERFILE_PATH="./deploy/docker/Dockerfile-build-ubuntu-local"
IMAGE_NAME="px4-qgc:qgc-v5.0.8"
SOURCE_DIR="$(pwd)"
BUILD_DIR="${SOURCE_DIR}/build"

# 确保构建目录存在
mkdir -p "${BUILD_DIR}"

# 检查镜像是否存在，不存在时才构建
if ! docker image inspect "${IMAGE_NAME}" > /dev/null 2>&1; then
    echo "镜像 ${IMAGE_NAME} 不存在，开始构建..."
    docker build \
      --build-arg USER_ID=$(id -u) \
      --build-arg GROUP_ID=$(id -g) \
      --file "${DOCKERFILE_PATH}" \
      -t "${IMAGE_NAME}" \
      "${SOURCE_DIR}"
else
    echo "镜像 ${IMAGE_NAME} 已存在，跳过构建"
fi

# 运行容器
docker run \
  --rm \
  --cap-add SYS_ADMIN \
  --device /dev/fuse \
  --security-opt apparmor:unconfined \
  -v "${SOURCE_DIR}:/project/source" \
  -v "${BUILD_DIR}:/project/build" \
  "${IMAGE_NAME}"