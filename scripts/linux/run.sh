#!/bin/bash
set -euo pipefail

SCRIPT_FOLDER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT_FOLDER="$(cd "${SCRIPT_FOLDER}/../.." && pwd)"

EXIT_SUCCESS=0
EXIT_FAILURE=1
VERBOSE_ENABLED=1
VERBOSE_DISABLED=0

BUILD_TYPE="Release"
VERBOSE_BUILD="${VERBOSE_DISABLED}"

usage() {
  cat <<'USAGE'
Usage: ./run.sh [--debug|--release] [-d|-r] [--verbose|-v] [-c Debug|Release] [-h|--help] [-- <app args>]
  --debug,   -d  Build type Debug
  --release, -r  Build type Release (default)
  --verbose, -v  Verbose build output
  -c             Build type (Debug or Release)
  Extra arguments after -- are forwarded to Quick3DTest (eg. -- run-tests).
USAGE
}

FORWARD_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug|-d)
      BUILD_TYPE="Debug"
      shift
      ;;
    --release|-r)
      BUILD_TYPE="Release"
      shift
      ;;
    --verbose|-v)
      VERBOSE_BUILD="${VERBOSE_ENABLED}"
      shift
      ;;
    -c)
      if [[ -z "${2:-}" ]]; then
        echo "Option -c requires an argument." >&2
        usage >&2
        exit "${EXIT_FAILURE}"
      fi
      BUILD_TYPE="$2"
      shift 2
      ;;
    --)
      shift
      FORWARD_ARGS=("$@")
      break
      ;;
    -h|--help)
      usage
      exit "${EXIT_SUCCESS}"
      ;;
    *)
      echo "Unknown option: ${1}" >&2
      usage >&2
      exit "${EXIT_FAILURE}"
      ;;
  esac
done

if [[ "${BUILD_TYPE}" != "Debug" && "${BUILD_TYPE}" != "Release" ]]; then
  echo "Invalid build type: ${BUILD_TYPE} (use Debug or Release)" >&2
  exit "${EXIT_FAILURE}"
fi

BUILD_FOLDER="${PROJECT_ROOT_FOLDER}/build-${BUILD_TYPE,,}"
LOG_FILE="${PROJECT_ROOT_FOLDER}/tmp/run.log"

if [ ! -d "${BUILD_FOLDER}" ]; then
  echo "[run] ${BUILD_FOLDER} not found, running build first..."
  if [ "${VERBOSE_BUILD}" -eq "${VERBOSE_ENABLED}" ]; then
    "${SCRIPT_FOLDER}/build.sh" -c "${BUILD_TYPE}" -v
  else
    "${SCRIPT_FOLDER}/build.sh" -c "${BUILD_TYPE}"
  fi
fi

BINARY="${BUILD_FOLDER}/quick3dtest/Quick3DTest"
if [[ "${BUILD_TYPE}" == "Debug" ]]; then
  BINARY="${BUILD_FOLDER}/quick3dtest/Quick3DTestd"
fi

if [ ! -f "${BINARY}" ]; then
  echo "[run] Binary not found at ${BINARY}" >&2
  exit "${EXIT_FAILURE}"
fi

mkdir -p "$(dirname "${LOG_FILE}")"
echo "[run] Logging to ${LOG_FILE}"
exec "${BINARY}" "${FORWARD_ARGS[@]}" 2>&1 | tee "${LOG_FILE}"
