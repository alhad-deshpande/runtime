#!/bin/bash

# ==============================================================================
# Script: extract_interpreting_logs.sh
#
# Purpose:
#   Finds all log-*.log files generated under the 'artifacts' directory.
#   For each testcase folder containing log-*.log files:
#     1. Counts "Interpreting" and "Jitting" occurrences across the log files.
#     2. Extracts function names for Fallback Interpreting and Default Jitting.
#     3. Creates all testcase folders and the summary.log inside:
#          <runtime_root>/Interpreting_Function_Detection/<testcase_name>/
#     4. Generates separate log files under each testcase folder:
#          <runtime_root>/Interpreting_Function_Detection/<testcase_name>/interpreting.log
#          <runtime_root>/Interpreting_Function_Detection/<testcase_name>/jitting.log
#     5. Generates summary.log under:
#          <runtime_root>/Interpreting_Function_Detection/summary.log
#     6. Suppresses intermediate execution logs to console and only prints
#        "execution completed successfully" at the end.
#
# Usage:
#   bash extract_interpreting_logs.sh [RUNTIME_ROOT] [ARTIFACTS_DIR] [OUTPUT_DIR]
#
#   RUNTIME_ROOT  – root repository directory (defaults to script directory).
#   ARTIFACTS_DIR – base directory to search for logs (defaults to <RUNTIME_ROOT>/artifacts).
#   OUTPUT_DIR    – directory to store extracted counts and logs (defaults to <RUNTIME_ROOT>/Interpreting_Function_Detection).
# ==============================================================================

set -uo pipefail

# ---------------------------------------------------------------------------
# Resolve runtime root, output directory, and artifacts directory
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -n "${1:-}" ]]; then
    RUNTIME_ROOT="$1"
elif [[ -d "${SCRIPT_DIR}/artifacts" ]]; then
    RUNTIME_ROOT="${SCRIPT_DIR}"
elif [[ -d "${SCRIPT_DIR}/../artifacts" ]]; then
    RUNTIME_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
else
    RUNTIME_ROOT="${SCRIPT_DIR}"
fi

ARTIFACTS_DIR="${2:-${RUNTIME_ROOT}/artifacts}"
OUTPUT_BASE_DIR="${3:-${RUNTIME_ROOT}/Interpreting_Function_Detection}"
SUMMARY_FILE="${OUTPUT_BASE_DIR}/summary.log"

# Clean any existing contents in the output directory before generating fresh counts,
# ensuring no old data from previous runs is appended or retained.
rm -rf "$OUTPUT_BASE_DIR"
mkdir -p "$OUTPUT_BASE_DIR"

# ---------------------------------------------------------------------------
# Validate artifacts directory
# ---------------------------------------------------------------------------
if [[ ! -d "$ARTIFACTS_DIR" ]]; then
    echo "ERROR: Artifacts directory does not exist: $ARTIFACTS_DIR"
    echo "       Please ensure tests have been executed and artifacts are generated."
    exit 1
fi

# ---------------------------------------------------------------------------
# Helper: extract function names from lines containing a keyword and emit
# them in the format:   <DisplayPrefix> -> <FunctionName>
# ---------------------------------------------------------------------------
extract_funcnames() {
    local keyword="$1"
    local display_label="$2"
    local file="$3"
    # Match the keyword and replace leading text up to keyword/arrows with the custom display label
    grep "${keyword}" "$file" 2>/dev/null | sed -E "s/.*${keyword}([[:space:]]*->[[:space:]]*|[[:space:]]+)/${display_label} -> /" || true
}

# ---------------------------------------------------------------------------
# Counters and summary accumulators
# ---------------------------------------------------------------------------
TOTAL_LOGS_PROCESSED=0
TOTAL_TESTCASES_PROCESSED=0
GRAND_TOTAL_INTERP=0
GRAND_TOTAL_JIT=0

TMP_SUMMARY_ROWS=$(mktemp)
TMP_INTERP_DETAILS=$(mktemp)
trap 'rm -f "$TMP_SUMMARY_ROWS" "$TMP_INTERP_DETAILS"' EXIT

# ---------------------------------------------------------------------------
# Collect all directories under artifacts that contain log-*.log files
# ---------------------------------------------------------------------------
mapfile -t LOG_DIRS < <(find "$ARTIFACTS_DIR" -type f -name "log-*.log" -exec dirname {} + 2>/dev/null | sort -u)

# Fallback to *.log if no log-*.log files found
if [[ ${#LOG_DIRS[@]} -eq 0 ]]; then
    mapfile -t LOG_DIRS < <(find "$ARTIFACTS_DIR" -type f -iname "*.log" -exec dirname {} + 2>/dev/null | sort -u)
fi

if [[ ${#LOG_DIRS[@]} -eq 0 ]]; then
    echo "WARNING: No log files found under $ARTIFACTS_DIR"
    exit 0
fi

# ---------------------------------------------------------------------------
# Process each testcase directory
# ---------------------------------------------------------------------------
for tc_dir in "${LOG_DIRS[@]}"; do
    testcase_name=$(basename "$tc_dir")
    testcase_out_dir="${OUTPUT_BASE_DIR}/${testcase_name}"
    mkdir -p "$testcase_out_dir"

    # Find log-*.log files (fallback to *.log in the directory)
    mapfile -t tc_logs < <(find "$tc_dir" -maxdepth 1 -type f -name "log-*.log" | sort)
    if [[ ${#tc_logs[@]} -eq 0 ]]; then
        mapfile -t tc_logs < <(find "$tc_dir" -maxdepth 1 -type f -iname "*.log" | sort)
    fi
    [[ ${#tc_logs[@]} -eq 0 ]] && continue

    TOTAL_TESTCASES_PROCESSED=$(( TOTAL_TESTCASES_PROCESSED + 1 ))

    tc_total_interp=0
    tc_total_jit=0

    # Temporary files to aggregate log files for this testcase
    tmp_interp=$(mktemp)
    tmp_jit=$(mktemp)

    for log_file in "${tc_logs[@]}"; do
        TOTAL_LOGS_PROCESSED=$(( TOTAL_LOGS_PROCESSED + 1 ))

        # Count occurrences in this specific log file
        log_interp_count=$(grep -c "Interpreting" "$log_file" 2>/dev/null || true)
        log_jit_count=$(grep -c "Jitting"         "$log_file" 2>/dev/null || true)
        log_interp_count=${log_interp_count:-0}
        log_jit_count=${log_jit_count:-0}

        # Extract lines to temp aggregation
        if [[ "$log_interp_count" -gt 0 ]]; then
            extract_funcnames "Interpreting" "Fallback Interpreting" "$log_file" >> "$tmp_interp"
            tc_total_interp=$(( tc_total_interp + log_interp_count ))
        fi

        if [[ "$log_jit_count" -gt 0 ]]; then
            extract_funcnames "Jitting" "Default Jitting" "$log_file" >> "$tmp_jit"
            tc_total_jit=$(( tc_total_jit + log_jit_count ))
        fi
    done

    # Generate primary interpreting.log for testcase
    output_interp_file="${testcase_out_dir}/interpreting.log"
    {
        echo "Testcase                    : $testcase_name"
        echo "Source Directory            : $tc_dir"
        echo "Total Fallback Interpreting : $tc_total_interp"
        echo "Extracted Date              : $(date)"
        echo "=================================================================="
        if [[ -s "$tmp_interp" ]]; then
            cat "$tmp_interp"
        else
            echo "No Fallback Interpreting functions found."
        fi
    } > "$output_interp_file"

    # Generate primary jitting.log for testcase
    output_jit_file="${testcase_out_dir}/jitting.log"
    {
        echo "Testcase                    : $testcase_name"
        echo "Source Directory            : $tc_dir"
        echo "Total Default Jitting       : $tc_total_jit"
        echo "Extracted Date              : $(date)"
        echo "=================================================================="
        if [[ -s "$tmp_jit" ]]; then
            cat "$tmp_jit"
        else
            echo "No Default Jitting functions found."
        fi
    } > "$output_jit_file"

    # Format counts for summary table: show "-" if interpreting count is 0
    display_interp="$tc_total_interp"
    if [[ "$tc_total_interp" -eq 0 ]]; then
        display_interp="-"
    fi

    # Record row in summary table (Width increased to 60 for long testcase names)
    printf "%-60s | %-20s | %-25s\n" "$testcase_name" "$tc_total_jit" "$display_interp" >> "$TMP_SUMMARY_ROWS"

    # If any interpreting functions occurred, record full details with log file path
    if [[ "$tc_total_interp" -gt 0 ]]; then
        {
            echo "------------------------------------------------------------------"
            echo "Testcase : $testcase_name (Fallback Interpreting Count: $tc_total_interp)"
            echo "------------------------------------------------------------------"
            for log_file in "${tc_logs[@]}"; do
                cnt=$(grep -c "Interpreting" "$log_file" 2>/dev/null || true)
                cnt=${cnt:-0}
                if [[ "$cnt" -gt 0 ]]; then
                    echo "Log File: $log_file"
                    extract_funcnames "Interpreting" "Fallback Interpreting" "$log_file"
                    echo ""
                fi
            done
        } >> "$TMP_INTERP_DETAILS"
    fi

    rm -f "$tmp_interp" "$tmp_jit"

    GRAND_TOTAL_INTERP=$(( GRAND_TOTAL_INTERP + tc_total_interp ))
    GRAND_TOTAL_JIT=$(( GRAND_TOTAL_JIT + tc_total_jit ))
done

# ---------------------------------------------------------------------------
# Build & Write Final Summary to summary.log
# ---------------------------------------------------------------------------
{
    echo "=============================================================================================================="
    echo "                                SUMMARY OF DEFAULT JITTING / FALLBACK INTERPRETING                            "
    echo "=============================================================================================================="
    echo "Generated Date                  : $(date)"
    echo "Total testcase folders          : $TOTAL_TESTCASES_PROCESSED"
    echo "Total log files scanned         : $TOTAL_LOGS_PROCESSED"
    echo "Grand Total Default Jitting     : $GRAND_TOTAL_JIT"
    echo "Grand Total Fallback Interpreting: $GRAND_TOTAL_INTERP"
    echo "=============================================================================================================="
    echo ""
    echo "PER-TESTCASE COUNTS:"
    printf "%-60s | %-20s | %-25s\n" "TESTCASE NAME" "DEFAULT JITTING" "FALLBACK INTERPRETING"
    printf -- "-------------------------------------------------------------+----------------------+--------------------------\n"
    cat "$TMP_SUMMARY_ROWS"
    echo "=============================================================================================================="
    echo ""
    echo "FALLBACK INTERPRETING FUNCTIONS BREAKDOWN:"
    if [[ -s "$TMP_INTERP_DETAILS" ]]; then
        cat "$TMP_INTERP_DETAILS"
    else
        echo "None (No testcase had Fallback Interpreting functions)."
    fi
    echo "=============================================================================================================="
} > "$SUMMARY_FILE"

# ---------------------------------------------------------------------------
# Completion message
# ---------------------------------------------------------------------------
echo "Execution completed successfully."
