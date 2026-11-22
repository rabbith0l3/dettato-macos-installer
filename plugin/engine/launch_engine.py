"""Frozen Dettato Engine entry point used by the macOS plug-ins."""
from __future__ import annotations

import os
from pathlib import Path

import imageio_ffmpeg

# Dettato falls back to ffmpeg for M4A/AAC and files libsndfile cannot decode.
# imageio-ffmpeg supplies a self-contained binary, so users need no Homebrew install.
ffmpeg = Path(imageio_ffmpeg.get_ffmpeg_exe())
os.environ["PATH"] = str(ffmpeg.parent) + os.pathsep + os.environ.get("PATH", "")

from dettato_plugin.worker import main

if __name__ == "__main__":
    main()
