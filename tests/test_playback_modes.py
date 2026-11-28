import ast
import shlex
import sys
import threading
import time
import types
import unittest
from typing import Any
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "backend"))
from playback_modes import mode_commands
from mpd_client import MPDClient
from unittest.mock import Mock
from shared import ApiError, mpd_quote


def load_functions(file, names, context):
    context.setdefault("Any", Any)
    tree = ast.parse((ROOT / file).read_text())
    functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
    assert len(functions) == len(names)
    exec(compile(ast.Module(body=functions, type_ignores=[]), str(file), "exec"), context)


class FakeMPD:
    def __init__(self):
        self.commands = []
        self.queue = []
        self.playing_file = None
        self.status = {}

    def command(self, command):
        self.commands.append(command)
        parts = shlex.split(command)
        if parts[0] == "clear":
            self.queue.clear()
        elif parts[0] == "add":
            self.queue.append(parts[1])
        elif parts[0] == "addid":
            self.queue.insert(int(parts[2]), parts[1])
        elif parts[0] == "play":
            self.playing_file = self.queue[int(parts[1])]
        return ["OK"]

    def single_map(self, command):
        return self.status if command == "status" else {"file": self.playing_file or "album/song.flac"}


class PlaybackModesTest(unittest.TestCase):
    def test_native_modes(self):
        self.assertEqual(mode_commands({"repeatMode": "off"}), ["repeat 0", "single 0"])
        self.assertEqual(mode_commands({"repeatMode": "all"}), ["repeat 1", "single 0", "consume 0"])
        self.assertEqual(mode_commands({"repeatMode": "song"}), ["repeat 1", "single 1", "consume 0"])
        self.assertEqual(mode_commands({"shuffle": True}), ["random 1"])
        self.assertEqual(mode_commands({"shuffle": False}), ["random 0"])

    def test_invalid_options_are_rejected_before_commands(self):
        for body in ({}, None, {"repeatMode": "once"}, {"shuffle": "false"}, {"shuffle": 1}, {"repeatMode": "all", "shuffle": "bad"}):
            with self.subTest(body=body), self.assertRaises(ApiError) as error:
                mode_commands(body)
            self.assertEqual(error.exception.status, 400)

    def test_endpoint_sends_only_options_and_rejects_radio(self):
        mpd = FakeMPD()
        context = {"mpd": mpd, "mode_commands": mode_commands, "ApiError": ApiError,
                   "_external_transport_action": lambda *args: None,
                   "note_playback_activity": lambda: None, "get_external_input_state": None,
                   "_is_radio_stream_uri": lambda uri: uri.startswith("http"),
                   "compat_player_state": lambda: {"ok": True}}
        load_functions("backend/server.py", {"compat_player_post"}, context)
        self.assertEqual(context["compat_player_post"]("/api/player/options", {"repeatMode": "song", "shuffle": True}), {"ok": True})
        self.assertEqual(mpd.commands, ["command_list_begin\nrepeat 1\nsingle 1\nconsume 0\nrandom 1\ncommand_list_end"])
        mpd.playing_file = "https://radio.example/stream"
        with self.assertRaises(ApiError) as error:
            context["compat_player_post"]("/api/player/options", {"shuffle": False})
        self.assertEqual(error.exception.status, 409)
        self.assertEqual(len(mpd.commands), 1)

    def test_status_exposes_native_modes_to_all_clients(self):
        mpd = FakeMPD()
        mpd.status = {"repeat": "1", "single": "1", "random": "1", "state": "play", "bitrate": "920", "songid": "12"}
        mpd.single_map = lambda command: mpd.status if command == "status" else {"file": "album/song.flac", "Id": "12"}
        context = {"mpd": mpd, "as_track": lambda song: {"file": song["file"]},
                   "get_external_input_state": None, "sync_local_playback_takeover": None,
                   "_is_radio_stream_uri": lambda uri: False, "_with_ui_context": lambda payload: payload}
        load_functions("backend/server.py", {"api_status", "compat_player_state"}, context)
        status = context["compat_player_state"]()["status"]
        self.assertTrue(status["repeat"])
        self.assertTrue(status["single"])
        self.assertTrue(status["random"])
        self.assertEqual(status["bitrate"], 920)
        mpd.status["songid"] = "13"
        self.assertEqual(context["compat_player_state"]()["status"]["bitrate"], 0)

    def queue_fixture(self):
        mpd = FakeMPD()
        workers = []
        context = {"mpd": mpd, "mpd_quote": mpd_quote, "ApiError": ApiError,
                   "_mpd_append_lock": threading.Lock(), "_mpd_append_generation": 0,
                   "note_playback_activity": lambda: None, "log_playback": lambda *args, **kw: None,
                   "set_play_context": lambda **kw: None, "SYNC_QUEUE_HEAD": 0,
                   "lib_ui_context": types.SimpleNamespace(publish_playback=lambda *args, **kw: None),
                   "_file_exists_in_library": lambda uri: True,
                   "pitunes_stream_url": lambda uri, port: f"http://127.0.0.1:{port}/api/stream?file={uri}"}
        class DeferredThread:
            def __init__(self, target, **kwargs):
                self.target = target
            def start(self):
                workers.append(self.target)
        context["threading"] = types.SimpleNamespace(Thread=DeferredThread)
        load_functions("backend/playback.py", {"_normalize_uri", "_forward_queue_from_target",
            "_cancel_mpd_append_queue", "mpd_add_uri", "_mpd_append_uris_async", "play_queue_fast"}, context)
        return mpd, workers, context

    def test_mid_album_fast_start_retains_full_queue_without_restarting(self):
        mpd, workers, context = self.queue_fixture()
        context["play_queue_fast"](target_uri="c.flac", queue=["a.flac", "b.flac", "c.flac", "d.flac"], album_id="1")
        self.assertEqual(mpd.queue, ["c.flac"])
        self.assertEqual(mpd.playing_file, "c.flac")
        workers[0]()
        self.assertEqual(mpd.queue, ["a.flac", "b.flac", "c.flac", "d.flac"])
        self.assertEqual(mpd.playing_file, "c.flac")
        self.assertEqual(mpd.commands.count("play 0"), 1)

    def test_a_last_track_still_retains_earlier_tracks_for_repeat(self):
        mpd, workers, context = self.queue_fixture()
        context["play_queue_fast"](target_uri="b.wav", queue=["a.wav", "b.wav"], album_id="1")
        workers[0]()
        self.assertEqual(mpd.queue, ["a.wav", "b.wav"])
        self.assertEqual(mpd.playing_file, "b.wav")

    def test_cancelled_queue_cannot_append_to_a_new_selection(self):
        mpd, workers, context = self.queue_fixture()
        context["play_queue_fast"](target_uri="b.flac", queue=["a.flac", "b.flac", "c.flac"], album_id="1")
        context["_cancel_mpd_append_queue"]()
        workers[0]()
        self.assertEqual(mpd.queue, ["b.flac"])

    def test_repeat_and_shuffle_do_not_continue_into_other_albums(self):
        for status in ({"repeat": "1"}, {"random": "1"}):
            mpd = FakeMPD()
            mpd.status = status
            context = {"mpd": mpd, "time": time, "_continue_lock": threading.Lock(),
                       "_play_lock": threading.Lock(), "_last_continue_at": 0,
                       "_play_context": {"continuous": True, "album_id": "1"}}
            load_functions("backend/playback.py", {"maybe_continue_next_album"}, context)
            self.assertFalse(context["maybe_continue_next_album"]())

    def test_mpd_ack_does_not_replay_a_mutation(self):
        client = MPDClient()
        stream = Mock()
        stream.readline.return_value = b"ACK [55@0] {next} Not playing\n"
        client._get_socket = Mock(return_value=(None, stream))
        with self.assertRaisesRegex(ApiError, "Not playing"):
            client.command("next")
        stream.write.assert_called_once_with(b"next\n")

    def test_local_file_can_play_before_mpd_finishes_indexing(self):
        mpd, _, context = self.queue_fixture()
        original = mpd.command

        def command(value):
            if value == 'add "track.flac"':
                raise ApiError(502, "ACK [50@0] {add} No such directory")
            return original(value)

        mpd.command = command
        context["play_queue_fast"](target_uri="track.flac", queue=["track.flac"], album_id="1")
        self.assertEqual(mpd.playing_file, "http://127.0.0.1:8080/api/stream?file=track.flac")

    def test_stopped_next_uses_queue_position_and_preserves_audio_error(self):
        mpd = FakeMPD()
        mpd.queue = ["a.flac", "b.flac"]
        context = {"mpd": mpd, "ApiError": ApiError, "_play_lock": threading.Lock(),
                   "_continue_lock": threading.Lock(), "_play_context": {},
                   "_mpd_append_pending": lambda: False}
        load_functions("backend/playback.py", {"_status_int", "next_track_or_album"}, context)
        mpd.status = {"state": "stop", "song": "0", "playlistlength": "2"}
        context["next_track_or_album"]()
        self.assertEqual(mpd.commands, ["play 1"])
        mpd.status["error"] = "Failed to open ALSA output"
        with self.assertRaisesRegex(ApiError, "Failed to open ALSA output"):
            context["next_track_or_album"]()
        self.assertEqual(mpd.commands, ["play 1"])

    def test_next_skips_tracks_then_advances_album_in_original_browse_order(self):
        mpd = FakeMPD()
        advanced = []
        order = {"albumBrowseSort": "year-desc", "mode": "album"}
        context = {
            "mpd": mpd, "ApiError": ApiError, "_play_lock": threading.Lock(), "_continue_lock": threading.Lock(),
            "_play_context": {"continuous": True, "album_id": "7", "browse_context": order},
            "_mpd_append_pending": lambda: False,
            "next_album_id_after": lambda current, browse: "8" if current == "7" and browse == order else "",
            "append_album_and_play": lambda album, **kw: advanced.append((album, kw["browse_context"])) or True,
        }
        load_functions("backend/playback.py", {"_status_int", "next_track_or_album"}, context)
        mpd.status = {"song": "0", "playlistlength": "2"}
        context["next_track_or_album"]()
        self.assertEqual(mpd.commands, ["next"])
        mpd.status["song"] = "1"
        context["next_track_or_album"]()
        self.assertEqual(mpd.commands, ["next"])
        self.assertEqual(advanced, [("8", order)])


if __name__ == "__main__":
    unittest.main()
