"""Real HTTP dispatcher with isolated writable paths, no MPD/system services."""
import http.client
import json
import os
import subprocess
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlencode

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

    def test_native_radio_icon_wait_returns_art_on_a_cold_cache(self):
        icon = b'<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><rect width="32" height="32"/></svg>'

        class StationSite(BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path != "/logo.svg":
                    self.send_error(404)
                    return
                self.send_response(200)
                self.send_header("Content-Type", "image/svg+xml")
                self.end_headers()
                self.wfile.write(icon)

            def log_message(self, *_args):
                pass

        with ThreadingHTTPServer(("127.0.0.1", 0), StationSite) as site:
            worker = threading.Thread(target=site.serve_forever, daemon=True)
            worker.start()
            try:
                query = urlencode({"wait": 1, "url": f"http://127.0.0.1:{site.server_port}/logo.svg"})
                with urllib.request.urlopen(self.url + "/api/library/radio/icon?" + query, timeout=5) as response:
                    self.assertEqual(response.read(), icon)
                query = urlencode({"wait": 1, "url": f"http://127.0.0.1:{site.server_port}/missing.svg"})
                with self.assertRaises(urllib.error.HTTPError) as missing:
                    urllib.request.urlopen(self.url + "/api/library/radio/icon?" + query, timeout=5)
                self.assertEqual(missing.exception.code, 404)
            finally:
                site.shutdown()
                worker.join()


if __name__ == "__main__": unittest.main()
