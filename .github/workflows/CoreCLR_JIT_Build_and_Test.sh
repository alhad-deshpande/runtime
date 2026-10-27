#!/bin/bash
set -euo pipefail
set -x

STAGE="${1:-all}"
export DEBIAN_FRONTEND=noninteractive
[ -f /env.sh ] && source /env.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TEST_DIR="${RUNTIME_DIR}/JIT_Testing"

echo "=================================="
echo "STAGE: $STAGE"
echo "=================================="

# =========================================================
# SETUP FUNCTION
# =========================================================
setup() {
  echo "Installing dependencies..."

  apt-get update
  apt-get install -y \
    bc automake clang curl findutils git hostname libtool \
    libkrb5-dev ninja-build llvm make python3 cmake \
    liblttng-ust-dev tar wget jq lld \
    build-essential zlib1g-dev libssl-dev libbrotli-dev \
    ca-certificates

  echo "Cloning runtime..."
  git clone --recurse-submodules https://github.com/alhad-deshpande/runtime.git
  cd runtime
  git checkout ppc64le_coreclr_jit

  # =========================================================
  #   Auto-clean disabled NuGet feeds
  # =========================================================
  echo "Cleaning NuGet feeds..."

  for CONFIG in NuGet.config eng/NuGet.config; do
    if [ -f "$CONFIG" ]; then
      echo "Processing $CONFIG"

      cp "$CONFIG" "$CONFIG.bkp"

      grep 'value="https://pkgs.dev.azure.com' "$CONFIG" | while read -r line
      do
        FEED_URL=$(echo "$line" | sed -n 's/.*value="\([^"]*\)".*/\1/p')

        echo "Checking: $FEED_URL"

        HTTP_CODE=$(curl -L -s -o /dev/null -w "%{http_code}" "$FEED_URL" || echo "000")

        if [ "$HTTP_CODE" = "404" ]; then
          echo "Removing disabled feed: $FEED_URL"

          ESCAPED=$(printf '%s\n' "$FEED_URL" | sed 's/[\/&]/\\&/g')

          sed -i "\|$ESCAPED|d" "$CONFIG"
        fi
      done
    fi
  done

  echo "NuGet cleanup done"

  cd ..

  echo "Setting up .NET SDK..."

  SDK_VERSION=$(jq -r '.sdk.version' runtime/global.json)

  export DOTNET_DIR="/dotnet-sdk-$(uname -m)"
  mkdir -p "$DOTNET_DIR"

  pushd "$DOTNET_DIR"

  wget "https://github.com/IBM/dotnet-s390x/releases/download/v${SDK_VERSION}/dotnet-sdk-${SDK_VERSION}-linux-$(uname -m).tar.gz"

  mkdir -p .dotnet
  tar xvf dotnet-sdk-*linux-$(uname -m).tar.gz -C .dotnet > /dev/null

  export DOTNET_ROOT="$(realpath .dotnet)"
  export PATH="$DOTNET_ROOT:$PATH"

  popd

  echo "DOTNET installed:"
  dotnet --version

  echo "export DOTNET_ROOT=$DOTNET_ROOT" > /env.sh
  echo "export PATH=$DOTNET_ROOT:\$PATH" >> /env.sh
}

# =========================================================
# BUILD RUNTIME
# =========================================================
build_runtime() {
  cd runtime
  export PATH="$DOTNET_ROOT:$PATH"
  export DOTNET_MULTILEVEL_LOOKUP=0
  export UseInstalledDotNetCli=true

  ./build.sh clr+clr.hosts \
  /p:SkipManagedTools=true \
  /p:PrimaryRuntimeFlavor=CoreCLR \
  /p:PublishAot=false \
  /p:SupportsNativeAotComponents=false \
  2>&1 | tee build.log
}

# =========================================================
# BUILD LIBS
# =========================================================
build_libs() {
  cd runtime
  ./build.sh libs 2>&1 | tee build_libs.log
}

# =========================================================
# BUILD TESTS
# =========================================================
build_tests() {
  cd runtime

  ./src/tests/build.sh /p:LibrariesConfiguration=Debug 2>&1 | tee build_tests.log

  CORE_ROOT="./artifacts/tests/coreclr/linux.ppc64le.Debug/Tests/Core_Root"

  cp "${CORE_ROOT}/IL/System.Private.CoreLib.dll" \
     "${CORE_ROOT}/System.Private.CoreLib.dll"
}

# =========================================================
# RUN 95 TESTS
# =========================================================
run_tests() {
  echo "Cloning JIT tests..."
  git clone https://github.com/alhad-deshpande/JIT_Testing.git
  cd JIT_Testing

  echo "Commit:"
  git log --oneline -1

  echo "Test count:"
  find testcases -name "*.cs" | wc -l

  chmod +x run_test.sh

  # Ensure RUNTIME_DIR points to the actual runtime directory (/runtime in container)
  local runtime_dir="$RUNTIME_DIR"
  if [ -z "$runtime_dir" ] || [ "$runtime_dir" = "/" ] || [ "$runtime_dir" = "//" ] || [ ! -d "${runtime_dir}/artifacts" ]; then
    if [ -d "/runtime" ]; then
      runtime_dir="/runtime"
    elif [ -d "$(pwd)/../runtime" ]; then
      runtime_dir="$(cd "$(pwd)/../runtime" && pwd)"
    fi
  fi

  set +e
  ./run_test.sh "$DOTNET_ROOT" "$runtime_dir" 2>&1 | tee /tmp/jit_run_output.log
  EXIT_CODE=${PIPESTATUS[0]}
  set -e

  if [ $EXIT_CODE -ne 0 ]; then
    echo " JIT Tests FAILED"
    exit $EXIT_CODE
  fi

  TOTAL=$(grep -i "Total Testcases Run" /tmp/jit_run_output.log | tail -1 | grep -oP '\d+' | tail -1)
  PASSED=$(grep -i "Testcases Passed" /tmp/jit_run_output.log | tail -1 | grep -oP '\d+' | tail -1)

  echo "Total Testcases Run : $TOTAL"
  echo "Testcases Passed    : $PASSED"

  if [ -z "$TOTAL" ] || [ -z "$PASSED" ]; then
    echo "ERROR: Could not parse test summary counts from output."
    exit 1
  fi

  if [ "$TOTAL" -ne "$PASSED" ]; then
    echo "ERROR: Test summary mismatch — Total Testcases Run ($TOTAL) != Testcases Passed ($PASSED)"
    exit 1
  fi

  echo " JIT Tests PASSED"
}

# =========================================================
# RUN CORECLR JIT TESTSUITE
# =========================================================
run_coreclr_tests() {
  cd runtime
  echo "Running full CoreCLR JIT testsuite..."
  set +e
  ./src/tests/run.sh Debug 2>&1 | tee tests.log
  EXIT_CODE=${PIPESTATUS[0]}
  set -e
  sleep 60
  echo "Final passed test count:"
  find artifacts/ -iname "*.log" | xargs grep -ni "Passed Test:" | wc || true
  find artifacts/ -iname "*.log" | xargs grep -ni "Passed Test:" > Passed_tests.txt || true
  find artifacts/ -iname "*.log" | xargs grep -ni "Failed Test:" > failed_tests.txt || true
  echo "Copying test logs to /tmp/Test_logs..."
  mkdir -p /tmp/Test_logs
  find artifacts/tests/coreclr/linux.ppc64le.Debug/ \
    -mindepth 1 -maxdepth 1 \
    -type d \
    ! -name "Tests" \
    ! -name "bin" \
    -exec cp -r {} /tmp/Test_logs/ \;
  cp Passed_tests.txt failed_tests.txt /tmp/Test_logs/ || true
  echo "Test logs copied to /tmp/Test_logs"
  exit $EXIT_CODE
}

# =========================================================
# EXTRACT JITTED FUNCTION LOGS
# Uses .github/workflows/extract_Jitted_func_logs.sh
# Output dir: $RUNTIME_DIR/Jitted_Function_Detection
# Never fails the pipeline: errors are reported as warnings.
# =========================================================
extract_jitted_function_log() {
  local script="${SCRIPT_DIR}/extract_Jitted_func_logs.sh"

  local out_dir="${RUNTIME_DIR}/Jitted_Function_Detection"

  if [ ! -f "$script" ]; then
    echo "WARNING: extract_Jitted_func_logs.sh not found at ${script}, skipping"
    return 0
  fi

  local logs_dir=""
  if [ -d "${RUNTIME_DIR}/artifacts" ]; then
    logs_dir="${RUNTIME_DIR}/artifacts"
  elif [ -d "/tmp/Test_logs" ]; then
    logs_dir="/tmp/Test_logs"
  else
    logs_dir="${RUNTIME_DIR}"
  fi

  echo "Extracting jitted function logs from ${logs_dir}..."
  set +e
  bash "$script" "${RUNTIME_DIR}" "${logs_dir}" "$out_dir"
  local rc=$?
  set -e

  if [ $rc -eq 0 ]; then
    echo "Output written to $out_dir"
    [ -f "$out_dir/summary.log" ] && cp "$out_dir/summary.log" /tmp/summary.log 2>/dev/null || true
  else
    echo "WARNING: jitted function log extraction failed (exit $rc), continuing"
  fi
  return 0
}

extract_interpreting_logs() {
  extract_jitted_function_log
}

# =========================================================
# MAIN DISPATCHER
# =========================================================
case "$STAGE" in
  setup)
    setup
    ;;
  build_runtime)
    build_runtime
    ;;
  build_libs)
    build_libs
    ;;
  build_tests)
    build_tests
    ;;
  run_tests)
    run_tests
    ;;
  run_coreclr_tests)
    run_coreclr_tests
    ;;
  extract_jitted_function_log|extract_interpreting_logs)
    extract_jitted_function_log
    ;;
  all)
    setup
    build_runtime
    build_libs
    build_tests
    run_tests
    run_coreclr_tests
    extract_jitted_function_log
    ;;
  *)
    echo "Invalid stage: $STAGE"
    exit 1
    ;;
esac

echo "=================================="
echo "DONE"
echo "=================================="
