"""Real HTTP dispatcher with isolated writable paths, no MPD/system services."""
import http.client
import json
import os
import subprocess
import sys
import tempfile
import unittest
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeHttpTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        root = Path(cls.tmp.name)
        env = dict(os.environ, PITUNES_CONFIG_DIR=str(root / "config"), PITUNES_CACHE_DIR=str(root / "cache"), MUSIC_DIR=str(root / "music"))
        cls.child = subprocess.Popen([sys.executable, "-u", "-c", "from http.server import ThreadingHTTPServer; from server import Handler; s=ThreadingHTTPServer(('127.0.0.1',0),Handler); print(s.server_port,flush=True); s.serve_forever()"],
                                    cwd=ROOT / "backend", env=env, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        cls.port = int(cls.child.stdout.readline())
        cls.url = f"http://127.0.0.1:{cls.port}"

    @classmethod
    def tearDownClass(cls):
        cls.child.terminate(); cls.child.wait(timeout=5); cls.child.stdout.close(); cls.tmp.cleanup()

    def test_smart_import_and_get_through_actual_handler(self):
        body = {"schemaVersion": 1, "product": "PiTunes", "smartPlaylists": [{"id": "smart-http", "name": "Saved rules", "rules": []}]}
        request = urllib.request.Request(self.url + "/api/library/smart-playlists/import", data=json.dumps(body).encode(), headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(request, timeout=5) as response:
            self.assertEqual(json.load(response)["imported"], 1)
        with urllib.request.urlopen(self.url + "/api/library/smart-playlists", timeout=5) as response:
            self.assertEqual(json.load(response)["smartPlaylists"], body["smartPlaylists"])

    def test_native_update_status_remains_disabled_over_http(self):
        with urllib.request.urlopen(self.url + "/api/system/update/status", timeout=5) as response:
            status = json.load(response)
        self.assertFalse(status["supported"])
        self.assertEqual(status["repository"], "BASF2HD/PiTunes-Native")

    def test_oversized_body_rejected_before_read(self):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        self.addCleanup(conn.close)
        conn.putrequest("POST", "/api/library/smart-playlists/import")
        conn.putheader("Content-Length", 3 * 1024 * 1024); conn.endheaders()
        self.assertEqual(conn.getresponse().status, 413)


if __name__ == "__main__": unittest.main()
