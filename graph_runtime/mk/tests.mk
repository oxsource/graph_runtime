# Tests module: run the Bazel test suite (unit tests, CLI tests, integration).
$(call register_module,tests)
$(call register_target,tests-verify)

.PHONY: tests-verify

tests-verify: ## Run the full Bazel test suite (bazel test //...)
	$(BAZEL) test //...
