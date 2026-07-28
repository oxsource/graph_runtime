#!/bin/bash

set -euo pipefail

# Create a test profile JSON file
PROFILE=$(mktemp /tmp/profile_XXXXXX.json)
trap "rm -f $PROFILE" EXIT

cat > "$PROFILE" << 'EOF'
{
  "capture_time": "2026-07-28T12:00:00.000Z",
  "node_count": 2,
  "profiler_config": {
    "enable_profiler": true,
    "histogram_interval_size_usec": 1000000,
    "num_histogram_intervals": 5,
    "trace_log_path": ""
  },
  "nodes": [
    {
      "node_name": "source",
      "open_runtime_usec": 42,
      "close_runtime_usec": 0,
      "process_count": 100,
      "process_time_total_usec": 123456,
      "process_time_mean_usec": 1234.56,
      "process_runtime": {
        "interval_size_usec": 1000000,
        "num_intervals": 5,
        "count": 100,
        "total_usec": 123456,
        "buckets": [80, 15, 3, 1, 1]
      }
    },
    {
      "node_name": "consumer",
      "open_runtime_usec": 8,
      "close_runtime_usec": 0,
      "process_count": 100,
      "process_time_total_usec": 34567,
      "process_time_mean_usec": 345.67,
      "process_runtime": {
        "interval_size_usec": 1000000,
        "num_intervals": 5,
        "count": 100,
        "total_usec": 34567,
        "buckets": [90, 8, 2, 0, 0]
      }
    }
  ]
}
EOF

# Run print_profile (binary should be passed as $1 or found via PATH)
PRINT_PROFILE="${1:-./print_profile}"

# Test 1: basic table output
OUTPUT=$("$PRINT_PROFILE" --files="$PROFILE" 2>&1)
echo "$OUTPUT" | grep -q "source" || { echo "FAIL: source not in output"; exit 1; }
echo "$OUTPUT" | grep -q "consumer" || { echo "FAIL: consumer not in output"; exit 1; }
echo "$OUTPUT" | grep -q "TOTAL" || { echo "FAIL: TOTAL not in output"; exit 1; }
echo "PASS: basic table output"

# Test 2: node-filter
OUTPUT=$("$PRINT_PROFILE" --files="$PROFILE" --node-filter="source" 2>&1)
echo "$OUTPUT" | grep -q "source" || { echo "FAIL: filtered source not found"; exit 1; }
echo "$OUTPUT" | grep -q "consumer" && { echo "FAIL: consumer should be filtered out"; exit 1; }
echo "PASS: node-filter"

# Test 3: CSV format
OUTPUT=$("$PRINT_PROFILE" --files="$PROFILE" --format=csv 2>&1)
echo "$OUTPUT" | grep -q "Node,Count,Mean" || { echo "FAIL: CSV header not found"; exit 1; }
echo "$OUTPUT" | grep -q "source,100" || { echo "FAIL: CSV source data not found"; exit 1; }
echo "PASS: CSV format"

exit 0
