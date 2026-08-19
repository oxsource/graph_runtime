# Examples module: build & verify the demos under src/examples/ (Bazel).
#
# Verification is split by execution model so a failure pinpoints the category:
#   - examples-verify-sync       : sync batch (Schedule/RunOnce path)
#   - examples-verify-async      : async streaming (Start/WaitUntilDone + injection)
#   - examples-verify-capability : standalone demos (parser registry, log hooks)
# Each example binary must exit 0 (see scripts/verify/verify_examples.sh).
$(call register_module,examples)
$(call register_target,examples-build)
$(call register_target,examples-verify)
$(call register_target,examples-verify-sync)
$(call register_target,examples-verify-async)
$(call register_target,examples-verify-capability)

.PHONY: examples-build examples-verify examples-verify-sync examples-verify-async examples-verify-capability

examples-build: ## Build all examples (bazel build //src/examples:all)
	$(BAZEL) build //src/examples:all

examples-verify: ## Build + run & verify every example (each must exit 0)
	$(MAKE) examples-verify-sync
	$(MAKE) examples-verify-async
	$(MAKE) examples-verify-capability
	@echo "[examples] all categories passed"

examples-verify-sync: ## Verify sync/batch examples (string_pipeline, *_json, profiler_demo)
	bash $(V)/verify_examples.sh sync

examples-verify-async: ## Verify async/streaming examples (add_packet, async_pipeline, interactive)
	bash $(V)/verify_examples.sh async

examples-verify-capability: ## Verify capability demos (custom_parser, log_intercept)
	bash $(V)/verify_examples.sh capability
