# Friendly short aliases for the canonical prefixed targets (conflict-free, unique).
# Each alias maps to a <module>-<action> target; duplicates abort the build.
$(call register_module,aliases)
$(call register_alias,build,examples-build)
$(call register_alias,verify,examples-verify)
$(call register_alias,test,tests-verify)
$(call register_alias,clean,clean-bazel)
