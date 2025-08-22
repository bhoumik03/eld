#!/bin/bash
set -e  

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBC_TEST_DIR="${1:-libc-test}"
EXPECTED_FAILURES_FILE="$SCRIPT_DIR/expected-failures.txt"
REPORT_FILE="$LIBC_TEST_DIR/src/REPORT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Main function
main() {
    log_info "Checking libc-test results for unexpected failures"
    log_info "libc-test directory: $LIBC_TEST_DIR"
    log_info "Expected failures file: $EXPECTED_FAILURES_FILE"
    
    # Validate inputs
    if [ ! -f "$REPORT_FILE" ]; then
        log_error "REPORT file not found: $REPORT_FILE"
        exit 1
    fi
    
    if [ ! -f "$EXPECTED_FAILURES_FILE" ]; then
        log_error "Expected failures file not found: $EXPECTED_FAILURES_FILE"
        exit 1
    fi
    
    # Create temporary files
    local temp_dir=$(mktemp -d)
    local actual_failures="$temp_dir/actual_failures.txt"
    local unexpected_failures="$temp_dir/unexpected_failures.txt"
    local fixed_failures="$temp_dir/fixed_failures.txt"
    
    # Extract failures from REPORT
    grep "^FAIL" "$REPORT_FILE" > "$actual_failures" || touch "$actual_failures"
    
    # Calculate statistics
    local total_tests=$(wc -l < "$REPORT_FILE")
    local actual_failure_count=$(wc -l < "$actual_failures")
    local expected_failure_count=$(wc -l < "$EXPECTED_FAILURES_FILE")
    
    log_info "=== TEST STATISTICS ==="
    echo "Total tests: $total_tests"
    echo "Actual failures: $actual_failure_count"
    echo "Expected failures: $expected_failure_count"
    
    # Show expected failures
    log_info "=== EXPECTED FAILURES (ALLOWLIST) ==="
    cat "$EXPECTED_FAILURES_FILE"
    echo
    
    # Show actual failures
    log_info "=== ACTUAL FAILURES ==="
    if [ -s "$actual_failures" ]; then
        cat "$actual_failures"
    else
        log_success "No failures found!"
    fi
    echo
    
    # Find unexpected failures
    if [ -s "$actual_failures" ]; then
        comm -23 <(sort "$actual_failures") <(sort "$EXPECTED_FAILURES_FILE") > "$unexpected_failures"
        local unexpected_count=$(wc -l < "$unexpected_failures")
        
        if [ $unexpected_count -gt 0 ]; then
            log_error "=== UNEXPECTED FAILURES DETECTED ==="
            cat "$unexpected_failures"
            echo
            log_error "WORKFLOW FAILURE: $unexpected_count new test failure(s) found"
            log_error "These failures indicate ELD + musl compatibility regressions"
            
            cleanup "$temp_dir"
            exit 1
        else
            log_success "All failures are expected (in allowlist)"
        fi
    else
        log_success "PERFECT: No test failures found!"
    fi
    
    # Check for improvements (fewer failures than expected)
    if [ $actual_failure_count -lt $expected_failure_count ]; then
        log_success "=== IMPROVEMENT DETECTED ==="
        log_success "Fewer failures than expected!"
        echo "Expected: $expected_failure_count, Actual: $actual_failure_count"
        
        # Find which expected failures are now passing
        comm -23 <(sort "$EXPECTED_FAILURES_FILE") <(sort "$actual_failures") > "$fixed_failures"
        if [ -s "$fixed_failures" ]; then
            log_success "These previously failing tests are now passing:"
            cat "$fixed_failures"
        fi
    fi
    
    log_success "WORKFLOW SUCCESS: No new regressions detected"
    
    cleanup "$temp_dir"
}

# Cleanup function
cleanup() {
    local temp_dir="$1"
    if [ -d "$temp_dir" ]; then
        rm -rf "$temp_dir"
    fi
}

# Help function
show_help() {
    cat << EOF
Usage: $0 [OPTIONS] [LIBC_TEST_DIR]

Check libc-test results against expected failures allowlist.

OPTIONS:
    -h, --help          Show this help message
    -v, --verbose       Enable verbose output
    
ARGUMENTS:
    LIBC_TEST_DIR       Path to libc-test directory (default: libc-test)

EXAMPLES:
    $0                  # Use default libc-test directory
    $0 /path/to/libc-test
    $0 --verbose libc-test

EXIT CODES:
    0   Success (no unexpected failures)
    1   Failure (unexpected failures found or error)
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -v|--verbose)
            set -x  # Enable verbose mode
            shift
            ;;
        -*)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
        *)
            LIBC_TEST_DIR="$1"
            shift
            ;;
    esac
done

# Run main function
main "$@"
