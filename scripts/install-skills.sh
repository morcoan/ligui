#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
dest="${DSH_HOME:-$HOME/.dsh}/skills"
mkdir -p "$dest"
cp -R "$here/skills/ligui-memory" "$here/skills/ligui-self-improve" "$dest/"
echo "installed Ligui skills into $dest"
