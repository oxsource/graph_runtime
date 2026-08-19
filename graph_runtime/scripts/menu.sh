#!/usr/bin/env bash
# Interactive module menu for graph_runtime make targets.
#   menu.sh <modules> <targets>
#
# Two-level picker driven entirely by the mk/ registry:
#   1. pick a registered module
#   2. pick one of its <module>-<action> targets
#   3. `make <target>` runs it, then the menu returns
#
# New modules appear automatically — no edits needed here.
set -euo pipefail

modules="$1"
targets="$2"

list_modules() {
  echo
  echo "=== graph_runtime modules ==="
  local i=0 m
  for m in $modules; do
    i=$((i + 1))
    printf '%2d) %s\n' "$i" "$m"
  done
  echo "  q) quit"
}

list_targets() {  # $1 = module name
  local module="$1" i=0 t
  echo
  echo "=== $module targets ==="
  for t in $targets; do
    case "$t" in
      "$module-"*) i=$((i + 1)); printf '%2d) %s\n' "$i" "$t" ;;
    esac
  done
  if [ "$i" -eq 0 ]; then
    echo "  (no targets — this module only defines aliases)"
  fi
  echo "  b) back to modules   q) quit"
}

# Pick a module by number; echoes its name on success, empty string otherwise.
pick_module() {  # $1 = numeric choice
  local choice="$1" i=0 m
  for m in $modules; do
    i=$((i + 1))
    [ "$choice" = "$i" ] && { echo "$m"; return 0; }
  done
  return 1
}

# Pick a target of $1 by number; echoes the target name on success.
pick_target() {  # $1 = module, $2 = numeric choice
  local module="$1" choice="$2" i=0 t
  for t in $targets; do
    case "$t" in
      "$module-"*)
        i=$((i + 1))
        [ "$choice" = "$i" ] && { echo "$t"; return 0; }
        ;;
    esac
  done
  return 1
}

while true; do
  list_modules
  printf 'choose: '
  read -r choice || exit 0
  case "$choice" in
    q|Q) exit 0 ;;
  esac
  module="$(pick_module "$choice" || true)"
  [ -n "$module" ] || { echo "invalid choice: $choice"; continue; }

  while true; do
    list_targets "$module"
    printf 'choose: '
    read -r choice || exit 0
    case "$choice" in
      q|Q) exit 0 ;;
      b|B) break ;;
    esac
    target="$(pick_target "$module" "$choice" || true)"
    [ -n "$target" ] || { echo "invalid choice: $choice"; continue; }

    echo
    echo "==> make $target"
    make "$target"
    echo
    echo "(target '$target' finished — press Enter to continue)"
    read -r _ || exit 0
  done
done
