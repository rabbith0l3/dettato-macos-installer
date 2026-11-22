import io
import json

import numpy as np
import pretty_midi
import pytest
from dettato_plugin.worker import JobError, run_job, serve

from dettato.pipeline import Track, Transcription


def test_transcribes_audio_into_a_readable_multitrack_midi(tmp_path):
    source = tmp_path / "take.wav"
    source.write_bytes(b"test doubles decode the audio")
    destination = tmp_path / "take.mid"

    def transcribe(path):
        assert path == str(source)
        return Transcription(
            source=str(path),
            seconds=1.0,
            tracks=[
                Track("Acoustic Grand Piano", 0, np.array([[0.0, 0.5, 60, 100]])),
                Track("Drums", 0, np.array([[0.0, 0.1, 36, 110]]), is_drum=True),
            ],
        )

    result = run_job({"id": "job-1", "input": str(source), "output": str(destination)}, transcribe=transcribe)

    midi = pretty_midi.PrettyMIDI(str(destination))
    assert [(track.name, track.is_drum, len(track.notes)) for track in midi.instruments] == [
        ("Acoustic Grand Piano", False, 1),
        ("Drums", True, 1),
    ]
    assert midi.instruments[0].notes[0].pitch == 60
    assert result["id"] == "job-1"
    assert result["status"] == "succeeded"
    assert json.loads((tmp_path / "take.json").read_text())["tracks"][1]["notes"] == 1


def test_failed_transcription_removes_stale_and_partial_results(tmp_path):
    source = tmp_path / "broken.wav"
    source.write_bytes(b"not audio")
    destination = tmp_path / "broken.mid"
    destination.write_bytes(b"stale MIDI from an earlier run")
    destination.with_suffix(".json").write_text("stale summary")

    def fail_after_partial_output(_):
        destination.write_bytes(b"partial MIDI")
        raise ValueError("cannot decode input")

    with pytest.raises(JobError, match="cannot decode input"):
        run_job({"id": "job-2", "input": str(source), "output": str(destination)}, transcribe=fail_after_partial_output)

    assert not destination.exists()
    assert not destination.with_suffix(".json").exists()


def test_worker_reports_bad_json_and_continues_serving_requests(tmp_path):
    source = tmp_path / "take.wav"
    source.write_bytes(b"audio")
    destination = tmp_path / "result.mid"
    valid = json.dumps({"id": "next", "input": str(source), "output": str(destination)})
    incoming = io.StringIO("not-json\n" + valid + "\n")
    outgoing = io.StringIO()

    def transcribe(_):
        return Transcription(str(source), 0.0, [])

    serve(incoming, outgoing, transcribe=transcribe)

    events = [json.loads(line) for line in outgoing.getvalue().splitlines()]
    assert events[0]["status"] == "failed"
    assert events[0]["id"] is None
    assert events[1]["id"] == "next"
    assert events[1]["status"] == "succeeded"


def test_worker_rejects_missing_id_without_running_the_models(tmp_path):
    request = json.dumps({"input": "take.wav", "output": str(tmp_path / "result.mid")})
    outgoing = io.StringIO()
    called = False

    def transcribe(_):
        nonlocal called
        called = True
        raise AssertionError("model must not run for a malformed request")

    serve(io.StringIO(request + "\n"), outgoing, transcribe=transcribe)

    event = json.loads(outgoing.getvalue())
    assert event["status"] == "failed"
    assert called is False
