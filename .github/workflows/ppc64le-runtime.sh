  #!/bin/bash -e

  # Runtime build script for ubuntu:22.04, only for CI job
  # docker run -it --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --name github-api ubuntu:22.04
  # ./runtime.sh  --sdk-path /dotnet-sdk-9.0.100-rc.2.24422.24-linux-ppc64le.tar.gz --ref v9.0.0-rc.2.24473.5 --patch-file /runtime.patch

  set -uxo pipefail
  apt update && apt upgrade -y && apt-get install sudo software-properties-common -y

  REPO=https://github.com/dotnet/runtime.git
  REF=main

  PORTABLE_BUILD=false
  BUILD=false
  TEST=false
  SDK_VERSION=""
  export ARCH=$(arch)
  export SCRIPT_DIR=$(pwd)
  export CONFIGURATION=Debug

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
      --configuration)
        shift
        CONFIGURATION="$1"
        ;;
      --portablebuild)
        shift
        PORTABLE_BUILD="$1"
        ;;
      --outerloop)
        test_args+=(/p:OuterLoop=true)
        ;;
      --sdk-path)
        shift
        SDK_PATH="$1"
        ;;
      --sdk_version)
        shift
        SDK_VERSION="$1"
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
    sudo add-apt-repository -y ppa:dotnet/backports
    sudo apt-get update && \
    sudo apt-get install -y dotnet-sdk-9.0
    sdk_version=$(dotnet --list-sdks | cut -d' ' -f1)
  fi

  runtime-build()
  {
      sudo apt -y install bc automake clang cmake findutils git \
                          hostname libtool libkrb5-dev\
                          llvm make python3  liblttng-ust-dev \
                          tar wget jq lld build-essential zlib1g-dev libssl-dev libbrotli-dev

      rm -rf "$(basename "$REPO" .git)"
      git clone "$REPO"

      cd "$(basename "$REPO" .git)"
      git checkout "$REF"
      COMMIT=$(git rev-parse HEAD)
      echo "$REPO is at $COMMIT"
      # git apply $PATCH_PATH

      if [[ $(uname -m) = "ppc64le" ]]; then
        mkdir dotnet-sdk-$(uname -m)
        pushd dotnet-sdk-$(uname -m)
        if [ ! -d .dotnet ]; then
            mkdir .dotnet
          cd .dotnet
          cp $(which dotnet) ./
          cd ../
        fi
        sdk_version=$(dotnet --list-sdks | cut -d' ' -f1)
        popd

        sed -i -E '/"sdk": \{/!b;n;s/"version": "[^"]+"/"version": "'"$sdk_version"'"/' global.json
        sed -i -E '/"tools": \{/!b;n;s/"dotnet": "[^"]+"/"dotnet": "'"$sdk_version"'"/' global.json
      fi

      BUILD_DIR="$(pwd)"
      EXIT_CODE=256
      BUILD_EXIT_CODE=256

      common_args+=(/p:NoPgoOptimize=true --portablebuild "$PORTABLE_BUILD")
      if [ "$PORTABLE_BUILD" == "false" ]; then
      common_args+=(/p:DotNetBuildFromSource=true)
      fi

      common_args+=(--runtimeconfiguration Release --librariesConfiguration "$CONFIGURATION")
      common_args+=(/p:PrimaryRuntimeFlavor=Mono --warnAsError false --subset clr+mono+libs+host+packs+libs.tests /p:SelfContained=false)
      common_args+=(/p:UsingToolMicrosoftNetCompilers=false /p:DotNetBuildSourceOnly=true /p:DotNetBuildTests=true --cmakeargs -DCLR_CMAKE_USE_SYSTEM_BROTLI=true --cmakeargs -DCLR_CMAKE_USE_SYSTEM_ZLIB=true /p:BaseOS=linux-ppc64le)

      BUILD_EXIT_CODE=0
      OPENSSL_ENABLE_SHA1_SIGNATURES=1 ./build.sh /p:NoWarn=true ${common_args[@]+"${common_args[@]}"} ${build_args[@]+"${build_args[@]}"} || BUILD_EXIT_CODE=$?
      EXIT_CODE=$BUILD_EXIT_CODE
      if [ "$EXIT_CODE" -ne 0 ]; then
        exit 1
      else
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
      ./build.sh --subset libs.tests --test /p:WithoutCategories=IgnoreForCI ${common_args[@]+"${common_args[@]}"} ${test_args[@]+"${test_args[@]}"} || LIB_BUILD_EXIT_CODE=$?

      cd /runtime/artifacts/bin
      CUR_DIR=$(pwd)
      for dir in `ls . | grep Tests$ `
      do
        cd $(find -name ${dir}.dll | xargs dirname)
        ${CUR_DIR}/testhost/net*inux-Debug-*/dotnet exec --runtimeconfig ${dir}.runtimeconfig.json --depsfile ${dir}.deps.json xunit.console.dll ${dir}.dll -xml testResults.xml -nologo -notrait category=OuterLoop -notrait category=failing
        if [ $? -ne 0 ]
        then
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

      RUNTIME_PASS_AVG=$(echo "scale=4; ($RUNTIME_PASSED_TESTCASES / $RUNTIME_TOTAL_TESTCASES) * 100" | bc)
      RUNTIME_SKIP_AVG=$(echo "scale=4; ($RUNTIME_SKIPPED_TESTCASES / $RUNTIME_TOTAL_TESTCASES) * 100" | bc)
      RUNTIME_FAIL_AVG=$(echo "scale=4; ($RUNTIME_FAILED_TESTCASES / $RUNTIME_TOTAL_TESTCASES) * 100" | bc)

      echo LIB test Result:
      echo Total Test cases Run:$RUNTIME_TOTAL_TESTCASES
      echo Test Passed:$RUNTIME_PASSED_TESTCASES
      echo Test failed:$RUNTIME_FAILED_TESTCASES
      echo Test skipped:$RUNTIME_SKIPPED_TESTCASES
      echo $RUNTIME_PASS_AVG
      echo $RUNTIME_SKIP_AVG
      echo $RUNTIME_FAIL_AVG

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
