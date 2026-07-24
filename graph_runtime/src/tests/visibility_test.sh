#!/usr/bin/env bash
# visibility_test.sh
# Verifies Bazel visibility constraints:
#   - Internal targets are NOT visible to external consumers
#   - Public target IS visible to external consumers
set -euo pipefail

# Targets to test
PUBLIC_TARGET="//src/public:runtime"
INTERNAL_TARGETS=(
  "//src/scheduler:scheduler"
  "//src/log:log_core"
  "//src/hook:hook"
  "//src/node:node_base"
  "//src/stream:timestamp"
  "//src/config:config"
)

errors=0

# 1. Public target must be visible to external
echo "Checking public target visibility..."
if bazel query "visible(//external:target, $PUBLIC_TARGET)" 2>/dev/null | grep -q .; then
  echo "  PASS: $PUBLIC_TARGET is visible externally"
else
  echo "  FAIL: $PUBLIC_TARGET is NOT visible externally"
  errors=$((errors + 1))
fi

# 2. Internal targets must NOT be visible to external
echo "Checking internal target visibility..."
for target in "${INTERNAL_TARGETS[@]}"; do
  result=$(bazel query "visible(//external:target, $target)" 2>/dev/null || true)
  if echo "$result" | grep -q .; then
    echo "  FAIL: $target IS visible externally"
    errors=$((errors + 1))
  else
    echo "  PASS: $target is hidden from external"
  fi
done

if [ $errors -gt 0 ]; then
  echo ""
  echo "FAILED: $errors visibility check(s) failed"
  exit 1
fi

echo ""
echo "PASS: all visibility checks passed"
