#!/usr/bin/env bash
# ==============================================================================
# Script: compare_pr_test_results.sh
# Purpose: Compares CoreCLR / Runtime test results between two PRs/builds.
#          Accepts the 4 files in ANY order (auto-detects PR1/PR2 & Pass/Fail)
#          or accepts 2 directory paths directly.
#          Displays summary counts and lists:
#            - Regressions (Pass -> Fail)
#            - Fixed Tests (Fail -> Pass)
#            - New Tests Passed
#            - New Tests Failed
#            - Missing / Skipped Tests in PR 2
# ==============================================================================

set -euo pipefail

PR1_PASS=""
PR1_FAIL=""
PR2_PASS=""
PR2_FAIL=""

PR1_DIR=""
PR2_DIR=""

LABEL_1="PR 1 (Base)"
LABEL_2="PR 2 (Target)"
OUTPUT_FILE=""
POSITIONAL_ARGS=()

show_help() {
  cat << EOF
Usage:
  Direct Files Mode (Pass the 4 files in ANY order):
    $(basename "$0") <FILE1> <FILE2> <FILE3> <FILE4> [REPORT.md]

  Directory Mode (Pass the 2 folders):
    $(basename "$0") <PR1_DIR> <PR2_DIR> [REPORT.md]

Examples (Any Order Works):
  ./compare_pr_test_results.sh pr1_logs/Passed_tests.txt pr1_logs/failed_tests.txt pr2_logs/passed_test.txt pr2_logs/failed_test.txt
  ./compare_pr_test_results.sh pr2_logs/failed_test.txt pr1_logs/Passed_tests.txt pr2_logs/passed_test.txt pr1_logs/failed_tests.txt
  ./compare_pr_test_results.sh ./pr1_logs ./pr2_logs
EOF
}

# Parse named flags
while [[ $# -gt 0 ]]; do
  case "$1" in
    --pr1-passed|--old-passed)
      PR1_PASS="$2"
      shift 2
      ;;
    --pr1-failed|--old-failed)
      PR1_FAIL="$2"
      shift 2
      ;;
    --pr2-passed|--new-passed)
      PR2_PASS="$2"
      shift 2
      ;;
    --pr2-failed|--new-failed)
      PR2_FAIL="$2"
      shift 2
      ;;
    -1|--old|--base|--pr1)
      PR1_DIR="$2"
      shift 2
      ;;
    -2|--new|--target|--pr2)
      PR2_DIR="$2"
      shift 2
      ;;
    --old-label|--label1|--pr1-label)
      LABEL_1="$2"
      shift 2
      ;;
    --new-label|--label2|--pr2-label)
      LABEL_2="$2"
      shift 2
      ;;
    -o|--output)
      OUTPUT_FILE="$2"
      shift 2
      ;;
    -h|--help)
      show_help
      exit 0
      ;;
    -*)
      echo "Unknown option: $1" >&2
      show_help
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1")
      shift
      ;;
  esac
done

# Smart Auto-Detection for 4 Positional Files in ANY Order
if [[ ${#POSITIONAL_ARGS[@]} -ge 4 ]]; then
  FILES=("${POSITIONAL_ARGS[0]}" "${POSITIONAL_ARGS[1]}" "${POSITIONAL_ARGS[2]}" "${POSITIONAL_ARGS[3]}")
  if [[ -z "$OUTPUT_FILE" && ${#POSITIONAL_ARGS[@]} -ge 5 ]]; then
    OUTPUT_FILE="${POSITIONAL_ARGS[4]}"
  fi

  # Validate all 4 exist
  for f in "${FILES[@]}"; do
    if [[ ! -f "$f" ]]; then
      echo "Error: File does not exist: $f" >&2
      exit 1
    fi
  done

  # Try to auto-classify by matching pr1/pr2 and pass/fail keywords in path/filename
  for f in "${FILES[@]}"; do
    LOWER_F=$(echo "$f" | tr '[:upper:]' '[:lower:]')
    
    # Check PR 1 files
    if [[ "$LOWER_F" =~ (pr1|pr_1|old|base) ]]; then
      if [[ "$LOWER_F" =~ pass ]]; then
        PR1_PASS="$f"
      elif [[ "$LOWER_F" =~ fail ]]; then
        PR1_FAIL="$f"
      fi
    # Check PR 2 files
    elif [[ "$LOWER_F" =~ (pr2|pr_2|new|target) ]]; then
      if [[ "$LOWER_F" =~ pass ]]; then
        PR2_PASS="$f"
      elif [[ "$LOWER_F" =~ fail ]]; then
        PR2_FAIL="$f"
      fi
    fi
  done

  # If auto-detection didn't fill all 4, fallback to standard fixed position order
  if [[ -z "$PR1_PASS" || -z "$PR1_FAIL" || -z "$PR2_PASS" || -z "$PR2_FAIL" ]]; then
    PR1_PASS="${POSITIONAL_ARGS[0]}"
    PR1_FAIL="${POSITIONAL_ARGS[1]}"
    PR2_PASS="${POSITIONAL_ARGS[2]}"
    PR2_FAIL="${POSITIONAL_ARGS[3]}"
  fi

elif [[ ${#POSITIONAL_ARGS[@]} -ge 2 ]]; then
  PR1_DIR="${POSITIONAL_ARGS[0]}"
  PR2_DIR="${POSITIONAL_ARGS[1]}"
  if [[ -z "$OUTPUT_FILE" && ${#POSITIONAL_ARGS[@]} -ge 3 ]]; then
    OUTPUT_FILE="${POSITIONAL_ARGS[2]}"
  fi
fi

# Create a clean temporary working directory
WORK_DIR=$(mktemp -d -t test_compare_XXXXXX)
trap 'rm -rf "$WORK_DIR"' EXIT

# Helper: Extract full unique test identity
clean_test_names() {
  sed -E \
    -e 's/^[.]?\/artifacts\/tests\/coreclr\/[^/]+\///I' \
    -e 's/^[.]?\/artifacts\/tests\/[^/]+\///I' \
    -e 's/^[.]?\/artifacts\///I' \
    -e 's/\/[^/]+\.log:[0-9]+(:[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3})?[[:space:]]+(Passed|Failed)[[:space:]]+test:[[:space:]]*/ -> /I' \
    -e 's/.*(Passed|Failed)[[:space:]]+test:[[:space:]]*//I' \
    -e 's/\r$//' \
    | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//' \
    | grep -v '^$' \
    | sort -u
}

count_file_tests() {
  local FILE="$1"
  if [[ ! -f "$FILE" ]]; then
    echo "0"
    return
  fi
  local CNT
  CNT=$(grep -i -c -E "(Passed|Failed)[[:space:]]+test:" "$FILE" 2>/dev/null || true)
  if [[ "$CNT" -eq 0 ]]; then
    CNT=$(grep -v -E '^(\s*$|\s*#)' "$FILE" | wc -l)
  fi
  echo "$CNT"
}

find_test_file() {
  local DIR="$1"
  local PATTERN="$2"
  find "$DIR" -maxdepth 2 -type f -iname "$PATTERN" 2>/dev/null | head -n 1 || true
}

# ------------------------------------------------------------------------------
# Extract tests based on mode
# ------------------------------------------------------------------------------
if [[ -n "$PR1_PASS" && -n "$PR1_FAIL" && -n "$PR2_PASS" && -n "$PR2_FAIL" ]]; then
  echo "Assigned files:"
  echo "  $LABEL_1 Passed : $PR1_PASS"
  echo "  $LABEL_1 Failed : $PR1_FAIL"
  echo "  $LABEL_2 Passed : $PR2_PASS"
  echo "  $LABEL_2 Failed : $PR2_FAIL"

  clean_test_names < "$PR1_PASS" > "$WORK_DIR/p1_passed.txt"
  clean_test_names < "$PR1_FAIL" > "$WORK_DIR/p1_failed.txt"
  P1_PASS_COUNT=$(count_file_tests "$PR1_PASS")
  P1_FAIL_COUNT=$(count_file_tests "$PR1_FAIL")

  clean_test_names < "$PR2_PASS" > "$WORK_DIR/p2_passed.txt"
  clean_test_names < "$PR2_FAIL" > "$WORK_DIR/p2_failed.txt"
  P2_PASS_COUNT=$(count_file_tests "$PR2_PASS")
  P2_FAIL_COUNT=$(count_file_tests "$PR2_FAIL")

elif [[ -n "$PR1_DIR" && -n "$PR2_DIR" ]]; then
  if [[ ! -d "$PR1_DIR" ]]; then
    echo "Error: Directory does not exist: $PR1_DIR" >&2
    exit 1
  fi
  if [[ ! -d "$PR2_DIR" ]]; then
    echo "Error: Directory does not exist: $PR2_DIR" >&2
    exit 1
  fi

  F_P1_P=$(find_test_file "$PR1_DIR" "*pass*")
  F_P1_F=$(find_test_file "$PR1_DIR" "*fail*")
  F_P2_P=$(find_test_file "$PR2_DIR" "*pass*")
  F_P2_F=$(find_test_file "$PR2_DIR" "*fail*")

  if [[ -z "$F_P1_P" || -z "$F_P1_F" || -z "$F_P2_P" || -z "$F_P2_F" ]]; then
    echo "Error: Could not locate passed/failed files in $PR1_DIR or $PR2_DIR" >&2
    exit 1
  fi

  echo "Auto-detected files in $LABEL_1 ($PR1_DIR):"
  echo "  Passed: $F_P1_P"
  echo "  Failed: $F_P1_F"
  clean_test_names < "$F_P1_P" > "$WORK_DIR/p1_passed.txt"
  clean_test_names < "$F_P1_F" > "$WORK_DIR/p1_failed.txt"
  P1_PASS_COUNT=$(count_file_tests "$F_P1_P")
  P1_FAIL_COUNT=$(count_file_tests "$F_P1_F")

  echo "Auto-detected files in $LABEL_2 ($PR2_DIR):"
  echo "  Passed: $F_P2_P"
  echo "  Failed: $F_P2_F"
  clean_test_names < "$F_P2_P" > "$WORK_DIR/p2_passed.txt"
  clean_test_names < "$F_P2_F" > "$WORK_DIR/p2_failed.txt"
  P2_PASS_COUNT=$(count_file_tests "$F_P2_P")
  P2_FAIL_COUNT=$(count_file_tests "$F_P2_F")

else
  echo "Error: Please provide either 4 files or 2 directories." >&2
  show_help
  exit 1
fi

P1_TOTAL=$((P1_PASS_COUNT + P1_FAIL_COUNT))
P2_TOTAL=$((P2_PASS_COUNT + P2_FAIL_COUNT))

# Combine passed and failed for each build
cat "$WORK_DIR/p1_passed.txt" "$WORK_DIR/p1_failed.txt" | sort -u > "$WORK_DIR/p1_all.txt"
cat "$WORK_DIR/p2_passed.txt" "$WORK_DIR/p2_failed.txt" | sort -u > "$WORK_DIR/p2_all.txt"

# ------------------------------------------------------------------------------
# Computations using comm
# ------------------------------------------------------------------------------
# 1. Fixed (Failed in Old -> Passed in New)
comm -12 "$WORK_DIR/p1_failed.txt" "$WORK_DIR/p2_passed.txt" > "$WORK_DIR/fixed.txt"
FIXED_COUNT=$(wc -l < "$WORK_DIR/fixed.txt")

# 2. Regressions (Passed in Old -> Failed in New)
comm -12 "$WORK_DIR/p1_passed.txt" "$WORK_DIR/p2_failed.txt" > "$WORK_DIR/regressed.txt"
REGRESSED_COUNT=$(wc -l < "$WORK_DIR/regressed.txt")

# 3. Newly Introduced Tests Passed (Not in Old, Passed in New)
comm -13 "$WORK_DIR/p1_all.txt" "$WORK_DIR/p2_passed.txt" > "$WORK_DIR/new_passed.txt"
NEW_PASS_COUNT=$(wc -l < "$WORK_DIR/new_passed.txt")

# 4. Newly Introduced Tests Failed (Not in Old, Failed in New)
comm -13 "$WORK_DIR/p1_all.txt" "$WORK_DIR/p2_failed.txt" > "$WORK_DIR/new_failed.txt"
NEW_FAIL_COUNT=$(wc -l < "$WORK_DIR/new_failed.txt")

# 5. Consistently Passed in both
comm -12 "$WORK_DIR/p1_passed.txt" "$WORK_DIR/p2_passed.txt" > "$WORK_DIR/both_passed.txt"
BOTH_PASS_COUNT=$(wc -l < "$WORK_DIR/both_passed.txt")

# 6. Consistently Failed in both
comm -12 "$WORK_DIR/p1_failed.txt" "$WORK_DIR/p2_failed.txt" > "$WORK_DIR/both_failed.txt"
BOTH_FAIL_COUNT=$(wc -l < "$WORK_DIR/both_failed.txt")

# 7. Missing in Target (In Old, not in New)
comm -23 "$WORK_DIR/p1_all.txt" "$WORK_DIR/p2_all.txt" > "$WORK_DIR/removed_in_target.txt"
REMOVED_COUNT=$(wc -l < "$WORK_DIR/removed_in_target.txt")

# ------------------------------------------------------------------------------
# Console Output
# ------------------------------------------------------------------------------
echo ""
echo "======================================================================"
echo "                    TEST COMPARISON SUMMARY"
echo "======================================================================"
printf "%-30s : %s\n" "Old Build (Baseline)" "$LABEL_1"
printf "%-30s : %s\n" "New Build (Updated)"  "$LABEL_2"
echo "----------------------------------------------------------------------"
printf "%-32s | %-12s | %-12s | %-8s\n" "Category" "$LABEL_1" "$LABEL_2" "Diff"
echo "----------------------------------------------------------------------"
printf "%-32s | %-12d | %-12d | %+d\n" "Total Tests"   "$P1_TOTAL"      "$P2_TOTAL"      "$((P2_TOTAL - P1_TOTAL))"
printf "%-32s | %-12d | %-12d | %+d\n" "Passed Tests"  "$P1_PASS_COUNT" "$P2_PASS_COUNT" "$((P2_PASS_COUNT - P1_PASS_COUNT))"
printf "%-32s | %-12d | %-12d | %+d\n" "Failed Tests"  "$P1_FAIL_COUNT" "$P2_FAIL_COUNT" "$((P2_FAIL_COUNT - P1_FAIL_COUNT))"
echo "----------------------------------------------------------------------"
printf "%-45s : %d\n" "[+] Fixed Tests (Failed -> Passed)"   "$FIXED_COUNT"
printf "%-45s : %d\n" "[-] Regressions (Passed -> Failed)"   "$REGRESSED_COUNT"
printf "%-45s : %d\n" "[+] New Tests Introduced (Passed)"    "$NEW_PASS_COUNT"
printf "%-45s : %d\n" "[-] New Tests Introduced (Failed)"    "$NEW_FAIL_COUNT"
printf "%-45s : %d\n" "[=] Consistently Failed"              "$BOTH_FAIL_COUNT"
printf "%-45s : %d\n" "[=] Consistently Passed"              "$BOTH_PASS_COUNT"
if [[ "$REMOVED_COUNT" -gt 0 ]]; then
printf "%-45s : %d\n" "[?] Missing in $LABEL_2"              "$REMOVED_COUNT"
fi
echo "======================================================================"

# Helper function to print lists cleanly
print_list() {
  local TITLE="$1"
  local FILE_PATH="$2"
  local COUNT="$3"

  echo ""
  echo "======================================================================"
  echo " $TITLE ($COUNT)"
  echo "======================================================================"
  if [[ "$COUNT" -eq 0 ]]; then
    echo "  (None)"
  else
    nl -w2 -s'. ' "$FILE_PATH"
  fi
}

# 1. Regressions
print_list "🚨 REGRESSIONS (Passed in $LABEL_1 -> Failed in $LABEL_2)" "$WORK_DIR/regressed.txt" "$REGRESSED_COUNT"

# 2. Fixed Tests
print_list "🎉 FIXED TESTS (Failed in $LABEL_1 -> Passed in $LABEL_2)" "$WORK_DIR/fixed.txt" "$FIXED_COUNT"

# 3. New Tests Introduced & Passed
print_list "➕ NEW TESTS INTRODUCED & PASSED (Not in $LABEL_1 -> Passed in $LABEL_2)" "$WORK_DIR/new_passed.txt" "$NEW_PASS_COUNT"

# 4. New Tests Introduced & Failed
print_list "❌ NEW TESTS INTRODUCED & FAILED (Not in $LABEL_1 -> Failed in $LABEL_2)" "$WORK_DIR/new_failed.txt" "$NEW_FAIL_COUNT"

# 5. Missing in Target PR
if [[ "$REMOVED_COUNT" -gt 0 ]]; then
  print_list "❓ MISSING / SKIPPED IN $LABEL_2 (Present in $LABEL_1 -> Absent in $LABEL_2)" "$WORK_DIR/removed_in_target.txt" "$REMOVED_COUNT"
fi

echo ""

# ------------------------------------------------------------------------------
# Generate Markdown Report (if output file specified)
# ------------------------------------------------------------------------------
if [[ -n "$OUTPUT_FILE" ]]; then
  mkdir -p "$(dirname "$OUTPUT_FILE")" 2>/dev/null || true

  {
    echo "# Test Results Comparison Report"
    echo ""
    echo "- **Old (Baseline):** \`$LABEL_1\`"
    echo "- **New (Updated):** \`$LABEL_2\`"
    echo ""
    echo "## Summary Statistics"
    echo ""
    echo "| Metric | $LABEL_1 | $LABEL_2 | Difference |"
    echo "| :--- | :---: | :---: | :---: |"
    printf "| **Total Tests** | %d | %d | %+d |\n" "$P1_TOTAL" "$P2_TOTAL" "$((P2_TOTAL - P1_TOTAL))"
    printf "| **Passed Tests** | %d | %d | %+d |\n" "$P1_PASS_COUNT" "$P2_PASS_COUNT" "$((P2_PASS_COUNT - P1_PASS_COUNT))"
    printf "| **Failed Tests** | %d | %d | %+d |\n" "$P1_FAIL_COUNT" "$P2_FAIL_COUNT" "$((P2_FAIL_COUNT - P1_FAIL_COUNT))"
    echo ""
    echo "## Key Result Changes"
    echo ""
    echo "- :tada: **Fixed (Failed in $LABEL_1 -> Passed in $LABEL_2):** $FIXED_COUNT"
    echo "- :rotating_light: **Regressions (Passed in $LABEL_1 -> Failed in $LABEL_2):** $REGRESSED_COUNT"
    echo "- :heavy_plus_sign: **New Tests Passed:** $NEW_PASS_COUNT"
    echo "- :x: **New Tests Failed:** $NEW_FAIL_COUNT"
    echo "- :white_check_mark: **Consistently Passed in Both:** $BOTH_PASS_COUNT"
    echo "- :warning: **Consistently Failed in Both:** $BOTH_FAIL_COUNT"
    if [[ "$REMOVED_COUNT" -gt 0 ]]; then
      echo "- :grey_question: **Missing in $LABEL_2:** $REMOVED_COUNT"
    fi
    echo ""

    write_section() {
      local TITLE="$1"
      local FILE_PATH="$2"
      local EMOJI="$3"
      local COUNT
      COUNT=$(wc -l < "$FILE_PATH")

      echo "### $EMOJI $TITLE ($COUNT)"
      echo ""
      if [[ "$COUNT" -eq 0 ]]; then
        echo "_None_"
      else
        echo "<details>"
        echo "<summary>Click to view all $COUNT test cases</summary>"
        echo ""
        echo '```text'
        cat "$FILE_PATH"
        echo '```'
        echo "</details>"
      fi
      echo ""
    }

    write_section "Regressions (Passed in $LABEL_1 -> Failed in $LABEL_2)" "$WORK_DIR/regressed.txt" ":rotating_light:"
    write_section "Fixed Tests (Failed in $LABEL_1 -> Passed in $LABEL_2)" "$WORK_DIR/fixed.txt" ":tada:"

    if [[ "$NEW_PASS_COUNT" -gt 0 ]]; then
      write_section "New Tests Introduced & Passed" "$WORK_DIR/new_passed.txt" ":heavy_plus_sign:"
    fi

    if [[ "$NEW_FAIL_COUNT" -gt 0 ]]; then
      write_section "New Tests Introduced & Failed" "$WORK_DIR/new_failed.txt" ":x:"
    fi

    write_section "Consistently Failed Tests" "$WORK_DIR/both_failed.txt" ":warning:"

    if [[ "$REMOVED_COUNT" -gt 0 ]]; then
      write_section "Tests in $LABEL_1 but Absent in $LABEL_2" "$WORK_DIR/removed_in_target.txt" ":grey_question:"
    fi

  } > "$OUTPUT_FILE"

  echo "Detailed Markdown report generated at: $OUTPUT_FILE"
fi
