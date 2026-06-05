#!/usr/bin/env bash
set -ex

echo "===== STEP 0: Install dependencies ====="

apt-get update && apt-get -y install \
  bc automake clang curl findutils git \
  hostname libtool libkrb5-dev ninja-build \
  llvm make python3 liblttng-ust-dev \
  tar wget jq lld build-essential \
  zlib1g-dev libssl-dev libbrotli-dev \
  ca-certificates libicu-dev locales tzdata

update-ca-certificates

echo "===== STEP 1: Clone runtime repository ====="

git clone --recurse-submodules https://github.com/alhad-deshpande/runtime.git
cd runtime
git checkout ppc64le_coreclr_jit

echo "===== STEP 2: Read .NET SDK version ====="

SDK_VERSION=$(jq -r '.sdk.version' global.json)
echo "Using SDK_VERSION=$SDK_VERSION"

echo "===== STEP 3: Setup .NET SDK ====="

export DOTNET_DIR=$(pwd)/dotnet-sdk-$(uname -m)
mkdir -p "$DOTNET_DIR"

pushd "$DOTNET_DIR"

wget https://github.com/IBM/dotnet-s390x/releases/download/v$SDK_VERSION/dotnet-sdk-$SDK_VERSION-linux-$(uname -m).tar.gz

mkdir -p .dotnet
tar xvf dotnet-sdk-$SDK_VERSION-linux-$(uname -m).tar.gz -C .dotnet > /dev/null

export DOTNET_ROOT=$(pwd)/.dotnet
export PATH=$DOTNET_ROOT:$PATH

dotnet --info

popd

echo "===== STEP 4: HARD FIX for NuGet (CRITICAL) ====="

# Reset ALL NuGet sources globally
mkdir -p ~/.nuget/NuGet

cat > ~/.nuget/NuGet/NuGet.Config <<EOF
<?xml version="1.0" encoding="utf-8"?>
<configuration>
  <packageSources>
    <clear />
    <add key="nuget.org" value="https://api.nuget.org/v3/index.json" />
  </packageSources>
</configuration>
EOF

cat ~/.nuget/NuGet/NuGet.Config

# Environment hardening
export NUGET_PACKAGES=$HOME/.nuget/packages
export DOTNET_NOLOGO=1
export DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1

echo "===== STEP 5: Build CoreCLR ====="

./build.sh clr+clr.hosts \
  /p:PrimaryRuntimeFlavor=CoreCLR \
  /p:PublishAot=false \
  /p:SupportsNativeAotComponents=false | tee build.log

echo "===== STEP 6: Build libraries ====="

./build.sh libs

echo "===== STEP 7: Build tests ====="

./src/tests/build.sh /p:LibrariesConfiguration=Debug

echo "===== STEP 8: Replace System.Private.CoreLib.dll ====="

CORE_ROOT=./artifacts/tests/coreclr/linux.ppc64le.Debug/Tests/Core_Root

if [ -f "$CORE_ROOT/IL/System.Private.CoreLib.dll" ]; then
    cp "$CORE_ROOT/IL/System.Private.CoreLib.dll" \
       "$CORE_ROOT/System.Private.CoreLib.dll"
else
    echo " CoreLib not found → test build failed earlier"
    exit 1
fi

echo "===== STEP 9: Clone JIT Testing repo ====="

cd ..
git clone https://github.com/alhad-deshpande/JIT_Testing
cd JIT_Testing
git checkout ppc64le_coreclr_jit_testing

echo "===== STEP 10: Run JIT tests ====="

BASE_DIR="$(pwd)/.."
RUNTIME_PATH="$BASE_DIR/runtime"
DOTNET_PATH="$RUNTIME_PATH/dotnet-sdk-$(uname -m)/.dotnet"

echo "Using DOTNET_PATH=$DOTNET_PATH"
echo "Using RUNTIME_PATH=$RUNTIME_PATH"

if [ ! -f "$DOTNET_PATH/dotnet" ]; then
    echo " ERROR: dotnet not found"
    exit 1
fi

chmod +x run_test.sh
./run_test.sh "$DOTNET_PATH" "$RUNTIME_PATH"

echo "=====  DONE ====="
