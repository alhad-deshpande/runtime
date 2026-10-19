#!/usr/bin/env bash
# ==============================================================================
# Script: compare_pr_test_results.sh
# Purpose: Compares CoreCLR / Runtime test results between two PRs/builds.
#          - Accepts 4 files in ANY order (auto-detects PR1/PR2 & Pass/Fail)
#          - Accepts 2 directory paths directly
#          - Automatically fetches baseline logs from the latest merged PR on
#            the target branch via GitHub REST API if baseline logs are not provided
#          Displays summary counts and lists:
#            - Regressions (Pass -> Fail)
#            - Fixed Tests (Fail -> Pass)
#            - New Tests Passed
#            - New Tests Failed
#            - Missing / Skipped Tests in PR 2
#          Generates a Markdown report and posts to $GITHUB_STEP_SUMMARY in CI.
# ==============================================================================

set -euo pipefail

REPO="${GITHUB_REPOSITORY:-alhad-deshpande/runtime}"
BRANCH="ppc64le_coreclr_jit"
CURRENT_RUN_ID="${GITHUB_RUN_ID:-}"
COMMIT_SHA="${GITHUB_SHA:-current}"

PR1_PASS=""
PR1_FAIL=""
PR2_PASS=""
PR2_FAIL=""

PR1_DIR="./prev_logs"
PR2_DIR="./logs"

LABEL_1=""
LABEL_2="Current PR Commit (${COMMIT_SHA})"
OUTPUT_FILE=""
FETCH_BASELINE_IF_MISSING=true
POSITIONAL_ARGS=()

show_help() {
  cat << EOF
Usage:
  1. Direct Files Mode (Pass the 4 files in ANY order):
     $(basename "$0") <FILE1> <FILE2> <FILE3> <FILE4> [REPORT.md]

  2. Directory Mode (Pass the 2 folders):
     $(basename "$0") <PR1_DIR> <PR2_DIR> [REPORT.md]

  3. Automated CI / GitHub REST API Mode (Auto-fetches baseline):
     $(basename "$0") --new ./logs --branch ppc64le_coreclr_jit -o comparison_report.md

Options:
  --new, -2, --target <DIR>       Directory containing current run test logs (e.g. ./logs)
  --old, -1, --base <DIR>         Directory containing baseline test logs (e.g. ./prev_logs)
  --branch <BRANCH>               Baseline target branch to fetch merged PR from (default: ppc64le_coreclr_jit)
  --repo <OWNER/REPO>             GitHub repository (default: \$GITHUB_REPOSITORY or alhad-deshpande/runtime)
  --old-label, --label1 <TEXT>    Label for baseline build (auto-detected if fetched)
  --new-label, --label2 <TEXT>    Label for current PR build
  -o, --output <FILE>             Path to save the Markdown comparison report
  -h, --help                      Show this help message
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
    --branch)
      BRANCH="$2"
      shift 2
      ;;
    --repo)
      REPO="$2"
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

# Smart Auto-Detection for Positional Arguments
if [[ ${#POSITIONAL_ARGS[@]} -ge 4 ]]; then
  FILES=("${POSITIONAL_ARGS[0]}" "${POSITIONAL_ARGS[1]}" "${POSITIONAL_ARGS[2]}" "${POSITIONAL_ARGS[3]}")
  if [[ -z "$OUTPUT_FILE" && ${#POSITIONAL_ARGS[@]} -ge 5 ]]; then
    OUTPUT_FILE="${POSITIONAL_ARGS[4]}"
  fi

  for f in "${FILES[@]}"; do
    if [[ ! -f "$f" ]]; then
      echo "Error: File does not exist: $f" >&2
      exit 1
    fi
  done

  # Try to auto-classify by matching pr1/pr2 and pass/fail keywords
  for f in "${FILES[@]}"; do
    LOWER_F=$(echo "$f" | tr '[:upper:]' '[:lower:]')
    if [[ "$LOWER_F" =~ (pr1|pr_1|old|base|prev) ]]; then
      if [[ "$LOWER_F" =~ pass ]]; then
        PR1_PASS="$f"
      elif [[ "$LOWER_F" =~ fail ]]; then
        PR1_FAIL="$f"
      fi
    elif [[ "$LOWER_F" =~ (pr2|pr_2|new|target|curr) ]]; then
      if [[ "$LOWER_F" =~ pass ]]; then
        PR2_PASS="$f"
      elif [[ "$LOWER_F" =~ fail ]]; then
        PR2_FAIL="$f"
      fi
    fi
  done

  # Fallback to standard positional order if auto-classification was incomplete
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
elif [[ ${#POSITIONAL_ARGS[@]} -eq 1 ]]; then
  PR2_DIR="${POSITIONAL_ARGS[0]}"
fi

# Set default labels if still empty
[ -z "$LABEL_1" ] && LABEL_1="Previous Baseline Run"

# Helper: Recursive find for test files
find_test_file() {
  local DIR="$1"
  local PATTERN="$2"
  if [[ ! -d "$DIR" ]]; then
    return 0
  fi
  find "$DIR" -type f -iname "$PATTERN" 2>/dev/null | head -n 1 || true
}

# ------------------------------------------------------------------------------
# Auto-Fetch Baseline from GitHub REST API if Baseline Files Not Present Locally
# ------------------------------------------------------------------------------
fetch_baseline_logs_from_api() {
  local TARGET_DIR="$1"
  mkdir -p "$TARGET_DIR"

  echo "================================================================="
  echo "Fetching baseline logs from latest merged PR into branch: $BRANCH"
  echo "Repository: $REPO"
  echo "================================================================="

  local AUTH_HEADER=()
  if [ -n "${GH_TOKEN:-}" ]; then
    AUTH_HEADER=(-H "Authorization: Bearer $GH_TOKEN")
  fi

  local PREV_RUN_ID=""
  local BASELINE_NAME=""

  if command -v python3 >/dev/null 2>&1; then
    local LOOKUP_RESULT
    LOOKUP_RESULT=$(python3 -c "
import urllib.request, json, os, sys

repo = '${REPO}'
token = os.environ.get('GH_TOKEN', '')
branch = '${BRANCH}'
current_run_id = '${CURRENT_RUN_ID}'

headers = {'Accept': 'application/vnd.github+json'}
if token:
    headers['Authorization'] = f'Bearer {token}'

def fetch_json(url):
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req) as resp:
            return json.loads(resp.read().decode('utf-8'))
    except Exception:
        return None

# Strategy 1: Find latest merged PR into the baseline branch
try:
    pulls_url = f'https://api.github.com/repos/{repo}/pulls?state=closed&base={branch}&sort=updated&direction=desc&per_page=15'
    pulls = fetch_json(pulls_url) or []
    for pr in pulls:
        if pr.get('merged_at'):
            pr_num = pr.get('number')
            pr_title = pr.get('title', '')
            head_sha = pr.get('head', {}).get('sha')
            
            if head_sha:
                runs_data = fetch_json(f'https://api.github.com/repos/{repo}/actions/runs?head_sha={head_sha}')
                for r in (runs_data or {}).get('workflow_runs', []):
                    if str(r.get('id')) != str(current_run_id) and r.get('conclusion') == 'success':
                        print(f\"{r.get('id')}|Merged PR #{pr_num} ({pr_title})\")
                        sys.exit(0)
                        
            head_ref = pr.get('head', {}).get('ref')
            if head_ref:
                runs_data = fetch_json(f'https://api.github.com/repos/{repo}/actions/runs?branch={head_ref}&per_page=5')
                for r in (runs_data or {}).get('workflow_runs', []):
                    if str(r.get('id')) != str(current_run_id) and r.get('conclusion') == 'success':
                        print(f\"{r.get('id')}|Merged PR #{pr_num} ({pr_title})\")
                        sys.exit(0)
except Exception:
    pass

# Strategy 2: Fallback to the latest successful completed workflow run in the repo
try:
    runs_data = fetch_json(f'https://api.github.com/repos/{repo}/actions/runs?status=completed&per_page=20')
    for r in (runs_data or {}).get('workflow_runs', []):
        if str(r.get('id')) != str(current_run_id) and r.get('conclusion') == 'success':
            branch_name = r.get('head_branch', 'unknown')
            print(f\"{r.get('id')}|Latest Successful Run #{r.get('run_number')} ({branch_name})\")
            sys.exit(0)
except Exception:
    pass
" 2>/dev/null || echo "")

    if [ -n "$LOOKUP_RESULT" ]; then
      PREV_RUN_ID=$(echo "$LOOKUP_RESULT" | cut -d'|' -f1)
      BASELINE_NAME=$(echo "$LOOKUP_RESULT" | cut -d'|' -f2)
      if [ -n "$BASELINE_NAME" ]; then
        LABEL_1="$BASELINE_NAME"
      fi
    fi
  fi

  # Fallback using curl + grep
  if [ -z "$PREV_RUN_ID" ]; then
    PREV_RUN_ID=$(curl -s "${AUTH_HEADER[@]}" -H "Accept: application/vnd.github+json" \
      "https://api.github.com/repos/${REPO}/actions/runs?status=completed&per_page=15" \
      | grep -B 2 -A 5 '"conclusion": *"success"' \
      | grep -o '"id": *[0-9]*' | head -1 | awk '{print $2}' || echo "")
  fi

  if [ -n "$PREV_RUN_ID" ] && [ "$PREV_RUN_ID" != "null" ]; then
    echo "Found Baseline Run ID: $PREV_RUN_ID ($LABEL_1)"
    local ARTIFACTS_API="https://api.github.com/repos/${REPO}/actions/runs/${PREV_RUN_ID}/artifacts"
    local ARTIFACT_URL=""
    
    if command -v jq >/dev/null 2>&1; then
      ARTIFACT_URL=$(curl -s "${AUTH_HEADER[@]}" -H "Accept: application/vnd.github+json" "$ARTIFACTS_API" \
        | jq -r '.artifacts[] | select(.name | startswith("jit-logs")) | .archive_download_url' | head -1)
      if [ -z "$ARTIFACT_URL" ]; then
        ARTIFACT_URL=$(curl -s "${AUTH_HEADER[@]}" -H "Accept: application/vnd.github+json" "$ARTIFACTS_API" \
          | jq -r '.artifacts[0].archive_download_url // empty' 2>/dev/null || echo "")
      fi
    else
      ARTIFACT_URL=$(curl -s "${AUTH_HEADER[@]}" -H "Accept: application/vnd.github+json" "$ARTIFACTS_API" \
        | grep -B 2 -A 5 '"name": *"jit-logs' | grep '"archive_download_url"' | head -1 | sed -n 's/.*"archive_download_url": *"\(https:[^"]*\)".*/\1/p')
      if [ -z "$ARTIFACT_URL" ]; then
        ARTIFACT_URL=$(curl -s "${AUTH_HEADER[@]}" -H "Accept: application/vnd.github+json" "$ARTIFACTS_API" \
          | grep '"archive_download_url"' | head -1 | sed -n 's/.*"archive_download_url": *"\(https:[^"]*\)".*/\1/p')
      fi
    fi

    if [ -n "$ARTIFACT_URL" ] && [ "$ARTIFACT_URL" != "null" ]; then
      echo "Downloading baseline artifacts..."
      curl -s -L "${AUTH_HEADER[@]}" -H "Accept: application/vnd.github+json" \
        -o "${TARGET_DIR}/artifacts.zip" "$ARTIFACT_URL" || true
      
      if [ -f "${TARGET_DIR}/artifacts.zip" ]; then
        echo "Extracting baseline artifacts..."
        if command -v unzip >/dev/null 2>&1; then
          unzip -q -o "${TARGET_DIR}/artifacts.zip" -d "$TARGET_DIR" || true
        elif command -v python3 >/dev/null 2>&1; then
          python3 -m zipfile -e "${TARGET_DIR}/artifacts.zip" "$TARGET_DIR" || true
        fi
      fi
    else
      echo "No baseline artifact archive found for run $PREV_RUN_ID"
    fi
  else
    echo "No previous baseline run found for $BRANCH"
  fi
}

# Check if baseline logs need to be fetched via API
if [[ -z "$PR1_PASS" || -z "$PR1_FAIL" ]]; then
  F_P1_P_CHECK=$(find_test_file "$PR1_DIR" "*pass*")
  F_P1_F_CHECK=$(find_test_file "$PR1_DIR" "*fail*")

  if [[ -z "$F_P1_P_CHECK" || -z "$F_P1_F_CHECK" ]]; then
    if [ "$FETCH_BASELINE_IF_MISSING" = true ]; then
      fetch_baseline_logs_from_api "$PR1_DIR"
    fi
  fi
fi

# Create clean temporary working directory
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
  F_P1_P=$(find_test_file "$PR1_DIR" "*pass*")
  F_P1_F=$(find_test_file "$PR1_DIR" "*fail*")
  F_P2_P=$(find_test_file "$PR2_DIR" "*pass*")
  F_P2_F=$(find_test_file "$PR2_DIR" "*fail*")

  if [[ -z "$F_P1_P" || -z "$F_P1_F" || -z "$F_P2_P" || -z "$F_P2_F" ]]; then
    echo "Comparison skipped: required test log files not found."
    if [[ -z "$F_P1_P" || -z "$F_P1_F" ]]; then echo "  - Missing baseline logs in $PR1_DIR"; fi
    if [[ -z "$F_P2_P" || -z "$F_P2_F" ]]; then echo "  - Missing current logs in $PR2_DIR"; fi
    exit 0
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
  echo "Error: Please provide either 4 files or directories to compare." >&2
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

print_list "🚨 REGRESSIONS (Passed in $LABEL_1 -> Failed in $LABEL_2)" "$WORK_DIR/regressed.txt" "$REGRESSED_COUNT"
print_list "🎉 FIXED TESTS (Failed in $LABEL_1 -> Passed in $LABEL_2)" "$WORK_DIR/fixed.txt" "$FIXED_COUNT"
print_list "➕ NEW TESTS INTRODUCED & PASSED (Not in $LABEL_1 -> Passed in $LABEL_2)" "$WORK_DIR/new_passed.txt" "$NEW_PASS_COUNT"
print_list "❌ NEW TESTS INTRODUCED & FAILED (Not in $LABEL_1 -> Failed in $LABEL_2)" "$WORK_DIR/new_failed.txt" "$NEW_FAIL_COUNT"
if [[ "$REMOVED_COUNT" -gt 0 ]]; then
  print_list "❓ MISSING / SKIPPED IN $LABEL_2 (Present in $LABEL_1 -> Absent in $LABEL_2)" "$WORK_DIR/removed_in_target.txt" "$REMOVED_COUNT"
fi

# ------------------------------------------------------------------------------
# Markdown Report Generation
# ------------------------------------------------------------------------------
TOTAL_DIFF=$((P2_TOTAL - P1_TOTAL))
PASS_DIFF=$((P2_PASS_COUNT - P1_PASS_COUNT))
FAIL_DIFF=$((P2_FAIL_COUNT - P1_FAIL_COUNT))

FMT_DIFF() {
  local val="$1"
  if [[ "$val" -gt 0 ]]; then
    echo "+$val"
  else
    echo "$val"
  fi
}

append_md_list() {
  local TITLE="$1"
  local FILE_PATH="$2"
  local COUNT="$3"
  local ICON="$4"
  local TARGET_MD="$5"

  echo "" >> "$TARGET_MD"
  echo "### $ICON $TITLE ($COUNT)" >> "$TARGET_MD"
  if [[ "$COUNT" -eq 0 ]]; then
    echo "_None_" >> "$TARGET_MD"
  else
    echo "<details><summary>Click to view $COUNT tests</summary>" >> "$TARGET_MD"
    echo "" >> "$TARGET_MD"
    awk '{print (NR) ". `" $0 "`"}' "$FILE_PATH" >> "$TARGET_MD"
    echo "" >> "$TARGET_MD"
    echo "</details>" >> "$TARGET_MD"
  fi
}

REPORT_MD=$(mktemp)

cat << EOF > "$REPORT_MD"
## 🧪 CoreCLR Test Comparison Report

| Metric | $LABEL_1 | $LABEL_2 | Diff |
| :--- | :--- | :--- | :--- |
| **Total Tests** | $P1_TOTAL | $P2_TOTAL | **$(FMT_DIFF "$TOTAL_DIFF")** |
| **Passed Tests** | $P1_PASS_COUNT | $P2_PASS_COUNT | **$(FMT_DIFF "$PASS_DIFF")** |
| **Failed Tests** | $P1_FAIL_COUNT | $P2_FAIL_COUNT | **$(FMT_DIFF "$FAIL_DIFF")** |

### 📊 Differential Breakdown
* **🎉 Fixed Tests (Failed &rarr; Passed):** \`$FIXED_COUNT\`
* **🚨 Regressions (Passed &rarr; Failed):** \`$REGRESSED_COUNT\`
* **➕ New Tests Introduced & Passed:** \`$NEW_PASS_COUNT\`
* **❌ New Tests Introduced & Failed:** \`$NEW_FAIL_COUNT\`
* **🔒 Consistently Passed:** \`$BOTH_PASS_COUNT\`
* **⚠️ Consistently Failed:** \`$BOTH_FAIL_COUNT\`
EOF

if [[ "$REMOVED_COUNT" -gt 0 ]]; then
  echo "* **❓ Missing in Target Build:** \`$REMOVED_COUNT\`" >> "$REPORT_MD"
fi

append_md_list "Regressions (Passed &rarr; Failed)" "$WORK_DIR/regressed.txt" "$REGRESSED_COUNT" "🚨" "$REPORT_MD"
append_md_list "Fixed Tests (Failed &rarr; Passed)" "$WORK_DIR/fixed.txt" "$FIXED_COUNT" "🎉" "$REPORT_MD"
append_md_list "New Tests Introduced & Passed" "$WORK_DIR/new_passed.txt" "$NEW_PASS_COUNT" "➕" "$REPORT_MD"
append_md_list "New Tests Introduced & Failed" "$WORK_DIR/new_failed.txt" "$NEW_FAIL_COUNT" "❌" "$REPORT_MD"

if [[ "$REMOVED_COUNT" -gt 0 ]]; then
  append_md_list "Missing / Skipped Tests" "$WORK_DIR/removed_in_target.txt" "$REMOVED_COUNT" "❓" "$REPORT_MD"
fi

if [[ -n "$OUTPUT_FILE" ]]; then
  cp "$REPORT_MD" "$OUTPUT_FILE"
  echo ""
  echo "Markdown report written to: $OUTPUT_FILE"
fi

# Append to GITHUB_STEP_SUMMARY if present in GitHub Actions environment
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  cat "$REPORT_MD" >> "$GITHUB_STEP_SUMMARY"
  echo "Appended summary report to \$GITHUB_STEP_SUMMARY"
fi

rm -f "$REPORT_MD"
exit 0
