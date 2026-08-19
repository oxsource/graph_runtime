# Help module: lists every `## `-annotated target across all modules.
# -h suppresses the per-file `name:` prefix that grep adds when given multiple files.
# `menu` is an interactive two-level picker (module -> target) driven entirely by
# the registry — new mk/*.mk files appear in it automatically.
$(call register_module,help)
$(call register_target,help)
$(call register_target,modules)
$(call register_target,menu)

.PHONY: help modules menu

help: ## List all targets
	@grep -hE '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) \
	  | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-16s\033[0m %s\n", $$1, $$2}'

modules: ## List registered modules (AOSP LOCAL_MODULE equivalents)
	@echo "modules: $(REGISTERED_MODULES)"

menu: ## Interactive module menu — pick a module, then a target to run
	@bash scripts/menu.sh "$(REGISTERED_MODULES)" "$(TARGET_OWNER)"
