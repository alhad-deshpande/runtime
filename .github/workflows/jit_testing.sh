#!/bin/bash

set -e

echo "=========================================="
echo "Installing dependencies"
echo "=========================================="

apt-get update

apt-get -y install \
    bc \
    automake \
    clang \
    curl \
    findutils \
    git \
    hostname \
    libtool \
    libkrb5-dev \
    ninja-build \
    llvm \
    make \
    python3 \
    liblttng-ust-dev \
    tar \
    wget \
    jq \
    lld \
    build-essential \
    zlib1g-dev \
    libssl-dev \
    libbrotli-dev \
    ca-certificates

WORKSPACE=$(pwd)

echo "=========================================="
echo "Cloning Runtime Repository"
echo "=========================================="

git clone --recurse-submodules https://github.com/alhad-deshpande/runtime.git
cd runtime

git checkout ppc64le_coreclr_jit

echo "=========================================="
echo "Installing .NET SDK"
echo "=========================================="

GLOBAL_JSON_PATH="global.json"
SDK_VERSION=$(jq -r '.sdk.version' "$GLOBAL_JSON_PATH")

export DOTNET_DIR=/dotnet-sdk-$(uname -m)

mkdir -p "$DOTNET_DIR"

pushd "$DOTNET_DIR"

wget https://github.com/IBM/dotnet-s390x/releases/download/v${SDK_VERSION}/dotnet-sdk-${SDK_VERSION}-linux-$(uname -m).tar.gz

mkdir -p .dotnet

tar xvf dotnet-sdk-${SDK_VERSION}-linux-$(uname -m).tar.gz \
    -C .dotnet > /dev/null

export DOTNET_ROOT=$(pwd)/.dotnet
export PATH=$DOTNET_ROOT:$PATH

popd

echo "DOTNET_ROOT=$DOTNET_ROOT"

dotnet --info

echo "=========================================="
echo "Building Runtime"
echo "=========================================="

./build.sh clr+clr.hosts \
    /p:PrimaryRuntimeFlavor=CoreCLR \
    /p:PublishAot=false \
    /p:SupportsNativeAotComponents=false \
    | tee build.log

echo "=========================================="
echo "Building Libraries"
echo "=========================================="

./build.sh libs

echo "=========================================="
echo "Building Tests"
echo "=========================================="

./src/tests/build.sh \
    /p:LibrariesConfiguration=Debug

echo "=========================================="
echo "Copying System.Private.CoreLib.dll"
echo "=========================================="

cp \
./artifacts/tests/coreclr/linux.ppc64le.Debug/Tests/Core_Root/IL/System.Private.CoreLib.dll \
./artifacts/tests/coreclr/linux.ppc64le.Debug/Tests/Core_Root/System.Private.CoreLib.dll

RUNTIME_PATH=$(pwd)

echo "=========================================="
echo "Cloning JIT Testing Repository"
echo "=========================================="

cd "$WORKSPACE"

git clone https://github.com/alhad-deshpande/JIT_Testing.git
cd JIT_Testing

git checkout ppc64le_coreclr_jit_testing

echo "=========================================="
echo "Running JIT Tests"
echo "=========================================="

DOTNET_PATH=$DOTNET_ROOT/dotnet

./run_test.sh "$DOTNET_PATH" "$RUNTIME_PATH"

echo "=========================================="
echo "Completed Successfully"
echo "=========================================="
