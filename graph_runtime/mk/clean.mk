# Clean module: remove Bazel build outputs.
$(call register_module,clean)
$(call register_target,clean-bazel)

.PHONY: clean-bazel

clean-bazel: ## Remove all Bazel outputs (bazel clean)
	$(BAZEL) clean
