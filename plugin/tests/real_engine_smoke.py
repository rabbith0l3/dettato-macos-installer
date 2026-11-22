import json
import math
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path


def main():
    engine = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="dettato-real-smoke-") as folder:
        source = Path(folder) / "sine.wav"
        output = Path(folder) / "sine.mid"
        with wave.open(str(source), "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(44100)
            samples = bytearray()
            for index in range(4 * 44100):
                seconds = index / 44100
                value = 0
                if seconds >= 0.5:
                    value = int(6000 * math.sin(2 * math.pi * 220 * seconds)
                                + 6000 * math.sin(2 * math.pi * 330 * seconds))
                samples += struct.pack("<h", value)
            wav.writeframes(samples)

        request = json.dumps({"id": "real-smoke", "input": str(source), "output": str(output)})
        result = subprocess.run([engine], input=request + "\n", text=True, capture_output=True,
                                timeout=1200, check=True)
        event = json.loads(result.stdout.strip().splitlines()[-1])
        if event.get("status") != "succeeded":
            raise RuntimeError(event.get("error", event))
        if not output.read_bytes().startswith(b"MThd"):
            raise RuntimeError("Engine did not create a Standard MIDI File")
        tracks = event.get("summary", {}).get("tracks", [])
        if not tracks or sum(track.get("notes", 0) for track in tracks) < 1:
            raise RuntimeError(f"Expected at least one transcribed note, got {tracks}")
        print(json.dumps({"status": "passed", "midi_bytes": output.stat().st_size,
                          "summary": event.get("summary")}))


if __name__ == "__main__":
    main()
