#!/usr/bin/env bash
# Verify examples under src/examples/: run each binary, require exit 0.
# Invoked by `make examples-verify-*` (the binaries must already be built).
#
#   verify_examples.sh          # all categories
#   verify_examples.sh sync     # sync/batch examples only
#   verify_examples.sh async    # async/streaming examples only
#   verify_examples.sh capability  # capability demos only
#
# To add an example, add it to the right list below — the mk module needs no
# changes (its category targets are already wired to this script).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

EXAMPLES_DIR="bazel-bin/src/examples"

# Classification by execution model (kept in sync with mk/examples.mk).
SYNC=(string_pipeline string_pipeline_json profiler_demo)
ASYNC=(add_packet_demo async_pipeline_demo interactive_pipeline)
CAP=(custom_parser log_intercept_demo)

case "${1:-all}" in
  all)        names=("${SYNC[@]}" "${ASYNC[@]}" "${CAP[@]}");;
  sync)       names=("${SYNC[@]}");;
  async)      names=("${ASYNC[@]}");;
  capability) names=("${CAP[@]}");;
  *)
    echo "[verify_examples] unknown category '$1' (all|sync|async|capability)" >&2
    exit 2
    ;;
esac

failed=0
for name in "${names[@]}"; do
  bin="$EXAMPLES_DIR/$name"
  if [ ! -x "$bin" ]; then
    echo "==> $name — NOT BUILT (run 'make examples-build' first)"
    failed=1
    continue
  fi
  log="/tmp/graphrt_example_${name}.log"
  echo "==> $name"
  if "$bin" >"$log" 2>&1; then
    echo "OK ($name)"
  else
    rc=$?
    echo "FAILED ($name): exit=$rc log=$log"
    tail -20 "$log"
    failed=1
  fi
done

if [ "$failed" -ne 0 ]; then
  exit 1
fi
echo "[verify_examples] $((${#names[@]})) example(s) passed"
