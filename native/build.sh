#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-gcc}
"$CC" -O2 -municode -Wall -o ligui-input.exe ligui-input.c -luser32 -lgdi32 -lshell32

if [[ -n "${INTERCEPTION_DLL:-}" ]]; then
  cp "$INTERCEPTION_DLL" interception.dll
  echo "copied interception.dll from $INTERCEPTION_DLL"
fi

echo "built $PWD/ligui-input.exe"
