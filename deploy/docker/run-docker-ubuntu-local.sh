#!/usr/bin/env bash

set -e

DOCKERFILE_PATH="./deploy/docker/Dockerfile-build-ubuntu-local"
IMAGE_NAME="qgc-ubuntu-docker"
SOURCE_DIR="$(pwd)"
BUILD_DIR="${SOURCE_DIR}/build"

# 确保构建目录存在
mkdir -p "${BUILD_DIR}"

# 构建镜像（传递用户 ID）
docker build \
  --build-arg USER_ID=$(id -u) \
  --build-arg GROUP_ID=$(id -g) \
  --file "${DOCKERFILE_PATH}" \
  -t "${IMAGE_NAME}" \
  "${SOURCE_DIR}"

# 运行容器
docker run \
  --rm \
  --cap-add SYS_ADMIN \
  --device /dev/fuse \
  --security-opt apparmor:unconfined \
  -v "${SOURCE_DIR}:/project/source" \
  -v "${BUILD_DIR}:/project/build" \
  "${IMAGE_NAME}"