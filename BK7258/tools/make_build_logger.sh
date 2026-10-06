#!/usr/bin/env bash

set -u

# Resolved once for this checkout. Calling the absolute path prevents the
# logger from invoking itself through PATH.
readonly REAL_BUILD_TOOL="/usr/bin/make"

if (( $# < 2 )); then
    printf 'usage: %s <project-directory> <make-arguments...>\n' "$0" >&2
    exit 64
fi

readonly PROJECT_DIRECTORY="$1"
shift

# CMake may invoke a build tool in temporary feature-detection directories.
# Those invocations must remain transparent and must not create project logs.
case "$(pwd -P)" in
    */CMakeFiles/CMakeScratch/*|*/CMakeFiles/CMakeTmp/*|*/CMakeFiles/CompilerId*/*|*/try_compile/*|*/TryCompile-*)
        exec "$REAL_BUILD_TOOL" "$@"
        ;;
esac

readonly LOG_DIR="${PROJECT_DIRECTORY}/build/LOG"
mkdir -p -- "$LOG_DIR" || exit 1

timestamp="$(date '+%Y%m%d-%H%M%S-%3N')"
while [[ -e "${LOG_DIR}/debug-build-${timestamp}.log" ]]; do
    timestamp="$(date '+%Y%m%d-%H%M%S-%3N')"
done

readonly BUILD_LOG="${LOG_DIR}/debug-build-${timestamp}.log"
readonly LATEST_LOG="${LOG_DIR}/debug-build-latest.log"

: >"$BUILD_LOG"
: >"$LATEST_LOG"

log_line() {
    printf '%s\n' "$*" | tee -a -- "$BUILD_LOG" "$LATEST_LOG"
}

log_line "开始时间: $(date '+%Y-%m-%dT%H:%M:%S.%3N%:z')"
log_line "当前工作目录: $(pwd -P)"
log_line "实际构建工具: ${REAL_BUILD_TOOL}"

{
    printf '完整参数:'
    printf ' %q' "$@"
    printf '\n参数数量: %d\n' "$#"
    argument_index=0
    for argument in "$@"; do
        printf 'argv[%d]: %q\n' "$argument_index" "$argument"
        argument_index=$((argument_index + 1))
    done
    printf '%s\n' '----- 构建输出开始 -----'
} | tee -a -- "$BUILD_LOG" "$LATEST_LOG"

"$REAL_BUILD_TOOL" "$@" 2>&1 | tee -a -- "$BUILD_LOG" "$LATEST_LOG"
build_status=${PIPESTATUS[0]}

log_line "----- 构建输出结束 -----"
log_line "结束时间: $(date '+%Y-%m-%dT%H:%M:%S.%3N%:z')"
log_line "底层构建工具原始退出码: ${build_status}"

exit "$build_status"
