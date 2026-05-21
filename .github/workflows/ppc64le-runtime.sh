q  #!/bin/bash -e

  # Runtime build script for ubuntu:22.04, only for CI job

  set -uxo pipefail
  apt update && apt upgrade -y && apt-get install apt-utils sudo software-properties-common -y

  REPO=https://github.com/alhad-deshpande/runtime.git
  REF=main

  PORTABLE_BUILD=false
  BUILD=false
  TEST=false
  SDK_VERSION=""
  export ARCH=$(arch)
  export SCRIPT_DIR=$(pwd)
  export CONFIGURATION=Debug
  export WORK_DIR=/
  export DOTNET_DIR=/dotnet-sdk-$(uname -m)
  
  NUM=0
  sdk_versions=0
  REF=0
  RUNTIME_TOTAL_TESTCASES=0
  RUNTIME_PASSED_TESTCASES=0
  RUNTIME_PASS_AVG=0
  RUNTIME_FAILED_TESTCASES=0
  RUNTIME_FAIL_AVG=0
  RUNTIME_SKIPPED_TESTCASES=0
  RUNTIME_SKIP_AVG=0
  LIB_BUILD_EXIT_CODE=0
  common_args=()
  build_args=()
  test_args=()

  get_linux_platform_name()
  {
    . /etc/os-release
    echo "$ID.$VERSION_ID"
    return 0
  }

  export linux_platform=$(get_linux_platform_name)

  while [ $# -ne 0 ]
  do
    name="$1"
    case "$name" in
      --ref)
        shift
        REF="$1"
        ;;
      --build)
        shift
        BUILD="true"
        ;;
      --test)
        shift
        TEST="true"
        ;;
      --patch-file)
        shift
        PATCH_PATH="$1"
        ;;
      *)
        echo "Unknown argument \`$name\`"
        exit 1
        ;;
    esac
    shift
  done

  if [[ $(uname -m) = "ppc64le" ]]; then
    add-apt-repository -y ppa:dotnet/backports
    apt-get update && sudo apt-get upgrade -y
  fi

  apt-get update && DEBIAN_FRONTEND="noninteractive" TZ="Asia/Kolkata" apt-get install -y tzdata

  apt-get -y install bc automake clang curl findutils git \
                  hostname libtool libkrb5-dev ninja-build \
                  llvm make python3  liblttng-ust-dev \
                  tar wget jq lld build-essential zlib1g-dev libssl-dev libbrotli-dev curl ca-certificates

# Install cmake:3.26.6
  cd /tmp
  wget https://github.com/Kitware/CMake/releases/download/v3.26.6/cmake-3.26.6.tar.gz > /dev/null
  tar -xf cmake-3.26.6.tar.gz > /dev/null
  cd cmake-3.26.6
  ./bootstrap --prefix=/usr/local
  make -j$(nproc)
  make install
  export PATH=/usr/local/bin:$PATH
  cmake --version && which cmake
  cd $WORK_DIR

  runtime-build()
  {
    git clone "$REPO"
    cd "$(basename "$REPO" .git)"
    git checkout "$REF"
    COMMIT=$(git rev-parse HEAD)
    echo "$REPO is at $COMMIT"
    # git apply $PATCH_PATH
    # sed -i '/"sdk"[[:space:]]*:/, /}/ s/"version":[[:space:]]*"[^"]*"/"version": "10.0.100-rc.2.25502.107"/' global.json
    # sed -i '/"tools"[[:space:]]*:/, /}/ s/"dotnet":[[:space:]]*"[^"]*"/"dotnet": "10.0.100-rc.2.25502.107"/' global.json
    GLOBAL_JSON_PATH="global.json"
    SDK_VERSION=$(jq -r '.sdk.version' "$GLOBAL_JSON_PATH")

    # cd ../
    mkdir $DOTNET_DIR
    pushd dotnet-sdk-$(uname -m)
    wget https://github.com/IBM/dotnet-s390x/releases/download/v$SDK_VERSION/dotnet-sdk-$SDK_VERSION-linux-ppc64le.tar.gz
    mkdir .dotnet
    tar xvf dotnet-sdk-*linux-$(uname -m).tar.gz -C .dotnet > /dev/null
    export DOTNET_ROOT=$(pwd)/.dotnet
    export PATH=$DOTNET_ROOT:$PATH
    popd
    cd "$(basename "$REPO" .git)"

    sed -i '/<ItemGroup Condition="'"'"'$(TargetOS)'"'"' == '"'"'linux'"'"' and '"'"'$(TargetArchitecture)'"'"' == '"'"'ppc64le'"'"'">/{
    n
    a\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Diagnostics.Process\\tests\\System.Diagnostics.Process.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.NetworkInformation\\tests\\FunctionalTests\\System.Net.NetworkInformation.Functional.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Formats.Tar\\tests\\System.Formats.Tar.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.Sockets\\tests\\FunctionalTests\\System.Net.Sockets.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Runtime\\tests\\System.IO.Tests\\System.IO.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.WebSockets.Client\\tests\\System.Net.WebSockets.Client.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Threading.ThreadPool\\tests\\System.Threading.ThreadPool.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Threading\\tests\\System.Threading.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.Ping\\tests\\FunctionalTests\\System.Net.Ping.Functional.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.Requests\\tests\\System.Net.Requests.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Runtime\\tests\\System.IO.FileSystem.Tests\\System.IO.FileSystem.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.DirectoryServices.Protocols\\tests\\System.DirectoryServices.Protocols.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.IO.Compression\\tests\\System.IO.Compression.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.NameResolution\\src\\System.Net.NameResolution.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)Common\\tests\\Common.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Private.Xml\\tests\\System.Private.Xml.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.NameResolution\\tests\\FunctionalTests\\System.Net.NameResolution.Functional.Tests.csproj" />\
    <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Security.Cryptography\\tests\\System.Security.Cryptography.Tests.csproj" />
    }' src/libraries/tests.proj


    # sed -i '157i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Diagnostics.Process\\tests\\System.Diagnostics.Process.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '158i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.Ping\\tests\\FunctionalTests\\System.Net.Ping.Functional.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '159i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Runtime\\tests\\System.IO.FileSystem.Tests\\System.IO.FileSystem.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '158i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.NetworkInformation\\tests\\FunctionalTests\\System.Net.NetworkInformation.Functional.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '159i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Formats.Tar\\tests\\System.Formats.Tar.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '160i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.Sockets\\tests\\FunctionalTests\\System.Net.Sockets.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '161i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Runtime\\tests\\System.IO.Tests\\System.IO.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '162i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.WebSockets.Client\\tests\\System.Net.WebSockets.Client.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '163i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.WebSockets.Client\\tests\\System.Net.WebSockets.Client.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '164i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Threading.ThreadPool\\tests\\System.Threading.ThreadPool.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '165i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.Ping\\tests\\FunctionalTests\\System.Net.Ping.Functional.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '159i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Threading\\tests\\System.Threading.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '162i<ProjectExclusions Include="$(MSBuildThisFileDirectory)Common\\tests\\Common.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '163i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Runtime\\tests\\System.Runtime.Tests\\System.Runtime.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '163i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Threading.ThreadPool\\tests\\System.Threading.ThreadPool.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '166i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Runtime\\tests\\System.IO.FileSystem.Tests\\File\\System.IO.MemoryMappedFiles.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '167i<ProjectExclusions Include="$(MSBuildThisFileDirectory)Microsoft.Bcl.TimeProvider\\tests\\Microsoft.Bcl.TimeProvider.Tests.csproj" />' src/libraries/tests.proj
    # sed -i '168i<ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Runtime\\tests\\System.Reflection.Tests\\System.Reflection.Tests.csproj" />' src/libraries/tests.proj

    BUILD_DIR="$(pwd)"
    EXIT_CODE=256
    BUILD_EXIT_CODE=256

    common_args+=(/p:NoPgoOptimize=true --portablebuild "$PORTABLE_BUILD")
    common_args+=(/p:DotNetBuildFromSource=true)
    common_args+=(--runtimeconfiguration Debug --librariesConfiguration "$CONFIGURATION")
    common_args+=(/p:PrimaryRuntimeFlavor=Mono --warnAsError false --subset clr+mono+libs+host+packs+libs.tests)
    common_args+=(/p:UsingToolMicrosoftNetCompilers=false  /p:DotNetBuildSourceOnly=true /p:DotNetBuildTests=true --cmakeargs -DCLR_CMAKE_USE_SYSTEM_BROTLI=true --cmakeargs -DCLR_CMAKE_USE_SYSTEM_ZLIB=true /p:BaseOS=linux-ppc64le)

    BUILD_EXIT_CODE=0
    OPENSSL_ENABLE_SHA1_SIGNATURES=1
    ./build.sh ${common_args[@]+"${common_args[@]}"} ${build_args[@]+"${build_args[@]}"} || BUILD_EXIT_CODE=$?

    EXIT_CODE=$BUILD_EXIT_CODE
    if [ "$EXIT_CODE" -ne 0 ]; then
      exit 1
    else
      echo "Runtime build is successful. Below are the test results:" >> /Script-details
      exit 0
    fi
  }

  lib_test_build()
  {
    if [[ "$REF" == release* ]]; then
      export OPENSSL_ENABLE_SHA1_SIGNATURES=1
    fi
    export TERM=xterm-256color

    TEST_EXIT_CODE=0
    cd "$(basename "$REPO" .git)"

    cd $WORK_DIR
    pushd dotnet-sdk-$(uname -m)
    export DOTNET_ROOT=$(pwd)/.dotnet
    export PATH=$DOTNET_ROOT:$PATH
    popd
    cd "$(basename "$REPO" .git)"

    ./build.sh --subset libs.tests --test /p:WithoutCategories=IgnoreForCI ${common_args[@]+"${common_args[@]}"} ${test_args[@]+"${test_args[@]}"} || LIB_BUILD_EXIT_CODE=$?

    cd /runtime/artifacts/bin
    CUR_DIR=$(pwd)
    for dir in `ls . | grep Tests$ `
    do
      cd "$(find . -path "./${dir}/*/${dir}.dll" -exec dirname {} \;)"
      if [ ! -f testResults.xml ]; then
        echo Test No $RUNTIME_TOTAL_TESTCASES - $dir skipped.. \(testResults.xml not found\)
        ((RUNTIME_SKIPPED_TESTCASES++))
      elif grep -Eo 'failed="[1-9][0-9]*"' testResults.xml >/dev/null; then
        echo Test No $RUNTIME_TOTAL_TESTCASES - $dir failed..
        ((RUNTIME_FAILED_TESTCASES++))
      else
        echo Test No $RUNTIME_TOTAL_TESTCASES - $dir passed..
        ((RUNTIME_PASSED_TESTCASES++))
      fi
      ((RUNTIME_TOTAL_TESTCASES++))
      cd $CUR_DIR
    done

    RUNTIME_SKIPPED_TESTCASES=$((RUNTIME_TOTAL_TESTCASES - RUNTIME_PASSED_TESTCASES - RUNTIME_FAILED_TESTCASES))
    RUNTIME_PASS_AVG=$(echo "scale=2; ($RUNTIME_PASSED_TESTCASES / $RUNTIME_TOTAL_TESTCASES) * 100" | bc)
    RUNTIME_SKIP_AVG=$(echo "scale=2; ($RUNTIME_SKIPPED_TESTCASES / $RUNTIME_TOTAL_TESTCASES) * 100" | bc)
    RUNTIME_FAIL_AVG=$(echo "scale=2; ($RUNTIME_FAILED_TESTCASES / $RUNTIME_TOTAL_TESTCASES) * 100" | bc)

    #echo "LIB Test Result" >> /Script-details
    echo "----------------------" >> /Script-details
    echo "Total Test Cases : $RUNTIME_TOTAL_TESTCASES" >> /Script-details
    echo "Test Passed      : $RUNTIME_PASSED_TESTCASES ($RUNTIME_PASS_AVG%)" >> /Script-details
    echo "Test Failed      : $RUNTIME_FAILED_TESTCASES ($RUNTIME_FAIL_AVG%)" >> /Script-details
    echo "Test Skipped     : $RUNTIME_SKIPPED_TESTCASES ($RUNTIME_SKIP_AVG%)" >> /Script-details
    #echo "Average Test Passed  : $RUNTIME_PASS_AVG" >> /Script-details
    #echo "Average Test Skipped : $RUNTIME_SKIP_AVG" >> /Script-details
    #echo "Average Test Failed  : $RUNTIME_FAIL_AVG" >> /Script-details

    if [ "$LIB_BUILD_EXIT_CODE" -ne 0  ]; then
      exit 1
    else
      exit 0
    fi
  }

if [ "$BUILD" == "true" ]; then
  runtime-build
fi

if [ "$TEST" == "true" ]; then
  lib_test_build
fi
