#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${1:-$ROOT/build-macos}"
PYTHON="${PYTHON:-python3}"
VENV="$BUILD_DIR/engine-venv"

"$PYTHON" -m venv "$VENV"
"$VENV/bin/python" -m pip install --upgrade pip setuptools wheel
"$VENV/bin/python" -m pip install "$ROOT" 'pyinstaller>=6,<7' 'imageio-ffmpeg>=0.5' 'pip-licenses>=4,<6'

rm -rf "$BUILD_DIR/engine-dist" "$BUILD_DIR/engine-work" "$BUILD_DIR/engine-spec"
mkdir -p "$BUILD_DIR/engine-dist" "$BUILD_DIR/engine-work" "$BUILD_DIR/engine-spec"

"$VENV/bin/python" -m PyInstaller \
  --noconfirm --clean --onedir --console \
  --name DettatoEngine \
  --distpath "$BUILD_DIR/engine-dist" \
  --workpath "$BUILD_DIR/engine-work" \
  --specpath "$BUILD_DIR/engine-spec" \
  --paths "$ROOT/src" \
  --paths "$ROOT/plugin/engine" \
  --collect-all dettato \
  --collect-all dettato_plugin \
  --collect-all imageio_ffmpeg \
  "$ROOT/plugin/engine/launch_engine.py"

ENGINE="$BUILD_DIR/engine-dist/DettatoEngine/DettatoEngine"
test -x "$ENGINE"
"$VENV/bin/python" - "$ENGINE" <<'PY'
import json
import subprocess
import sys

result = subprocess.run([sys.argv[1]], input="not-json\n", text=True,
                        capture_output=True, timeout=60, check=True)
event = json.loads(result.stdout.strip())
assert event["status"] == "failed", event
print("Frozen Dettato Engine starts and answers the plug-in protocol")
PY

"$VENV/bin/pip-licenses" --format=plain-vertical --with-license-file --with-notice-file \
  --output-file="$BUILD_DIR/engine-dist/DettatoEngine/THIRD_PARTY_LICENSES.txt"
