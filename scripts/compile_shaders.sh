#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHADERS_DIR="$REPO_ROOT/shaders"

if ! command -v glslangValidator >/dev/null 2>&1; then
  echo "Error: glslangValidator is not installed or not on PATH."
  exit 1
fi

echo "Compiling shaders from $SHADERS_DIR"
for shader in "$SHADERS_DIR"/*.vert "$SHADERS_DIR"/*.frag; do
  if [[ ! -e "$shader" ]]; then
    continue
  fi
  out="$shader.spv"
  echo "  $shader -> $out"
  glslangValidator -V "$shader" -o "$out"
done

echo "Shader compilation finished."
