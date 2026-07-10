#!/usr/bin/env bash
#
# clang-format-apply.sh
#
# Applies the repository .clang-format in-place to C/C++ sources.
#
# Usage:
#   util/clang-format-apply.sh [--check|--dry-run] [dir ...]
#
#   With no arguments, formats the default trees: compiler frontend
#   Pass one or more directories (or files) to override.
#
# Options:
#   -n, --dry-run, --check   Do not modify files. Report files that are not
#                            correctly formatted and exit non-zero if any are
#                            found. Intended for CI.
#
# Environment:
#   CLANG_FORMAT         path to clang-format (default: first on PATH)
#   CLANG_FORMAT_STYLE   path to the style file to use (default:
#                        <script dir>/CompilerFormat)
#   DRY_RUN              if set to a non-empty value, behaves like --check
#
# Notes:
#   * Without --check this rewrites files in place. Commit or stash your work
#     first so the changes are easy to review and revert.
#   * Generated lexer/parser sources (bison-*/flex-*) are skipped.
set -euo pipefail

dry_run="${DRY_RUN:-}"
args=()
for arg in "$@"; do
  case "$arg" in
    -n|--dry-run|--check) dry_run=1 ;;
    --) ;; # ignore separator
    *) args+=("$arg") ;;
  esac
done
set -- "${args[@]+"${args[@]}"}"

CLANG_FORMAT="${CLANG_FORMAT:-$(command -v clang-format || true)}"
if [[ -z "$CLANG_FORMAT" ]]; then
  echo "error: clang-format not found (set CLANG_FORMAT to its path)" >&2
  exit 1
fi


CWD=$(cd "$(dirname "$0")" && pwd)
CLANG_FORMAT_STYLE="${CLANG_FORMAT_STYLE:-$CWD/CompilerFormat}"
if [[ ! -f "$CLANG_FORMAT_STYLE" ]]; then
  echo "error: style file not found: $CLANG_FORMAT_STYLE" >&2
  exit 1
fi
style_arg="file:$CLANG_FORMAT_STYLE"

targets=("$@")
if [[ ${#targets[@]} -eq 0 ]]; then
  targets=(compiler frontend)
fi

echo "clang-format: $("$CLANG_FORMAT" --version)"

is_skipped() {
  case "$1" in
    *bison-*|*flex-*) return 0 ;;
    *) return 1 ;;
  esac
}

format_file() {
  local f="$1"
  is_skipped "$f" && return 0
  if [[ -n "$dry_run" ]]; then
    if ! "$CLANG_FORMAT" --dry-run --Werror --style="$style_arg" "$f"; then
      echo "needs formatting: $f" >&2
      unformatted=$((unformatted + 1))
    fi
  else
    "$CLANG_FORMAT" -i --style="$style_arg" "$f"
  fi
  count=$((count + 1))
}

count=0
unformatted=0
for t in "${targets[@]}"; do
  if [[ -f "$t" ]]; then
    format_file "$t"
  elif [[ -d "$t" ]]; then
    while IFS= read -r -d '' f; do
      format_file "$f"
    done < <(find "$t" \( -name '*.cpp' -o -name '*.c' -o -name '*.h' \
                          -o -name '*.hpp' -o -name '*.cc' \) -type f -print0)
  else
    echo "warning: skipping '$t' (not a file or directory)" >&2
  fi
done

if [[ -n "$dry_run" ]]; then
  echo "checked $count file(s), $unformatted need formatting"
  if [[ "$unformatted" -gt 0 ]]; then
    exit 1
  fi
else
  echo "formatted $count file(s)"
fi
