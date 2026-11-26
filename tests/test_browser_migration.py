import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "backend"))
from library import userdata


class BrowserMigrationTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.file = Path(self.tmp.name) / "userdata.json"
        self.file.write_text(json.dumps({"favouriteTracks": ["song.flac"], "playlists": [{"id": "old", "name": "Existing", "trackIds": ["song.flac"]}], "unknownFutureKey": {"keep": True}}))
        for name, value in (("_USERDATA_FILE", self.file), ("CONFIG_DIR", self.file.parent)):
            p = patch.object(userdata, name, value); p.start(); self.addCleanup(p.stop)
        self.export = {"schemaVersion": 1, "product": "PiTunes", "smartPlaylists": [{"id": "smart-1", "name": "தமிழ்", "matchMode": "all", "rules": [{"field": "bpm", "operator": "greater-than", "value": "80"}], "futureProperty": 7}], "preferences": {"pitunes-browse-state": "{}"}}

    def test_verbatim_definitions_additive_idempotent_and_original_data_retained(self):
        original_export = json.dumps(self.export, sort_keys=True)
        self.assertEqual(userdata.import_browser_data(self.export)["imported"], 1)
        self.assertEqual(userdata.import_browser_data(self.export)["imported"], 0)
        self.assertEqual(json.dumps(self.export, sort_keys=True), original_export)
        saved = json.loads(self.file.read_text())
        self.assertEqual(saved["favouriteTracks"], ["song.flac"])
        self.assertEqual(saved["playlists"][0]["id"], "old")
        self.assertEqual(saved["unknownFutureKey"], {"keep": True})
        self.assertEqual(userdata.list_smart_playlists(), self.export["smartPlaylists"])
        self.assertEqual(saved["browserMigrationPreferences"], self.export["preferences"])

    def test_conflict_and_invalid_input_leave_disk_unchanged(self):
        userdata.import_browser_data(self.export)
        before = self.file.read_bytes()
        self.export["smartPlaylists"][0]["name"] = "Different"
        with self.assertRaises(ValueError): userdata.import_browser_data(self.export)
        self.assertEqual(before, self.file.read_bytes())
        self.export["preferences"]["wifi-password"] = "example"
        with self.assertRaises(ValueError): userdata.import_browser_data(self.export)
        self.assertEqual(before, self.file.read_bytes())

    def test_corrupt_store_never_overwritten(self):
        self.file.write_bytes(b'{broken')
        with self.assertRaises(ValueError): userdata.import_browser_data(self.export)
        self.assertEqual(self.file.read_bytes(), b'{broken')


if __name__ == "__main__": unittest.main()
