"""Out-of-process bridge between the audio plug-in and Dettato."""
from __future__ import annotations

import json
import sys
from collections.abc import Callable
from pathlib import Path


class JobError(RuntimeError):
    """A transcription failure safe to show in the plug-in UI."""


def run_job(request: dict, transcribe: Callable | None = None) -> dict:
    """Transcribe one request and return a JSON-serialisable success event."""
    if transcribe is None:
        from dettato import transcribe

    source = request["input"]
    destination = Path(request["output"])
    summary_path = destination.with_suffix(".json")
    for path in (destination, summary_path):
        path.unlink(missing_ok=True)
    try:
        result = transcribe(source)
        result.save_midi(str(destination))
        summary = result.summary()
        summary_path.write_text(json.dumps(summary, indent=1), encoding="utf-8")
    except Exception as error:
        destination.unlink(missing_ok=True)
        summary_path.unlink(missing_ok=True)
        raise JobError(str(error)) from error
    return {
        "type": "result",
        "id": request["id"],
        "status": "succeeded",
        "midi": str(destination),
        "summary": summary,
    }


def serve(incoming, outgoing, transcribe: Callable | None = None) -> None:
    """Serve newline-delimited JSON requests until stdin closes."""
    for line in incoming:
        job_id = None
        try:
            request = json.loads(line)
            if not isinstance(request, dict):
                raise TypeError("request must be a JSON object")
            if any(not isinstance(request.get(key), str) or not request[key]
                   for key in ("id", "input", "output")):
                raise ValueError("request requires non-empty string id, input and output")
            job_id = request.get("id")
            event = run_job(request, transcribe=transcribe)
        # A worker must report model/library failures to the host instead of exiting silently.
        except Exception as error:  # noqa: BLE001
            event = {"type": "result", "id": job_id, "status": "failed", "error": str(error)}
        print(json.dumps(event), file=outgoing, flush=True)


def main() -> None:
    serve(sys.stdin, sys.stdout)


if __name__ == "__main__":
    main()
