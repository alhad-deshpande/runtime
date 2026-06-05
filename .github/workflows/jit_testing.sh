#!/usr/bin/env bash
set -euxo pipefail

echo "===== STEP 1: Install dependencies ====="

export DEBIAN_FRONTEND=noninteractive

apt-get update && apt-get install -y \
  bc automake clang curl findutils git \
  hostname libtool libkrb5-dev ninja-build \
  llvm make python3 liblttng-ust-dev \
  tar wget jq lld build-essential \
  zlib1g-dev libssl-dev libbrotli-dev \
  ca-certificates libicu-dev locales tzdata

# Fix timezone (avoid prompt)
ln -fs /usr/share/zoneinfo/Etc/UTC /etc/localtime
dpkg-reconfigure --frontend noninteractive tzdata

update-ca-certificates

echo "===== STEP 2: Clone runtime ====="

git clone --recurse-submodules https://github.com/alhad-deshpande/runtime.git
cd runtime
git checkout ppc64le_coreclr_jit

echo "===== STEP 3: Setup .NET SDK ====="

SDK_VERSION=$(jq -r '.sdk.version' global.json)

mkdir dotnet
cd dotnet

wget https://github.com/IBM/dotnet-s390x/releases/download/v${SDK_VERSION}/dotnet-sdk-${SDK_VERSION}-linux-$(uname -m).tar.gz

tar -xvf dotnet-sdk-${SDK_VERSION}-linux-$(uname -m).tar.gz > /dev/null

export DOTNET_ROOT=$(pwd)
export PATH=$DOTNET_ROOT:$PATH

dotnet --info
cd ..

echo "===== STEP 4: Fix NuGet (IMPORTANT) ====="


echo "===== STEP 4: Force NuGet override (NO command change) ====="

# Force all restores to use ONLY nuget.org
export NUGET_PACKAGES=$HOME/.nuget/packages
export NUGET_HTTP_CACHE_PATH=$HOME/.nuget/http-cache

# THIS is key (works without modifying build commands)
export DotNetRestoreSources="https://api.nuget.org/v3/index.json"

# Extra safety
export DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
export DOTNET_NOLOGO=1

echo "===== STEP 5: Build runtime ====="

./build.sh clr+clr.hosts /p:PrimaryRuntimeFlavor=CoreCLR /p:PublishAot=false /p:SupportsNativeAotComponents=false | tee build.log

echo "===== STEP 6: Build libs ====="

./build.sh libs

echo "===== STEP 7: Build tests ====="

./src/tests/build.sh /p:LibrariesConfiguration=Debug

echo "===== STEP 8: Fix CoreLib ====="

CORE_ROOT=./artifacts/tests/coreclr/linux.ppc64le.Debug/Tests/Core_Root

cp $CORE_ROOT/IL/System.Private.CoreLib.dll \
   $CORE_ROOT/System.Private.CoreLib.dll

echo "===== STEP 9: Clone JIT Testing ====="

cd ..
git clone https://github.com/alhad-deshpande/JIT_Testing
cd JIT_Testing
git checkout ppc64le_coreclr_jit_testing

echo "===== STEP 10: Run tests ====="

RUNTIME_PATH="$(pwd)/../runtime"
DOTNET_PATH="$RUNTIME_PATH/dotnet"

./run_test.sh "$DOTNET_PATH" "$RUNTIME_PATH"

echo "===== DONE ====="
