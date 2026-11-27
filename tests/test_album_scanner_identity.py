import sqlite3
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from backend.library import db, scanner


class AlbumScannerIdentityTest(unittest.TestCase):
    def setUp(self):
        self.conn = sqlite3.connect(":memory:")
        self.conn.row_factory = sqlite3.Row
        with patch.object(db, "get_connection", return_value=self.conn):
            db.init_db()

    def tearDown(self):
        self.conn.close()

    def album(self, title, album_artist_id, year):
        return scanner._get_or_create_album(
            self.conn, title, album_artist_id, album_artist_id, year, "Rock", 1
        )

    def test_album_identity_uses_title_artist_and_year(self):
        original = self.album("Greatest Hits", 1, 1998)
        same_release = self.album("greatest hits", 1, 1998)
        newer_release = self.album("Greatest Hits", 1, 2024)
        different_artist = self.album("Greatest Hits", 2, 1998)

        self.assertEqual(original, same_release)
        self.assertNotEqual(original, newer_release)
        self.assertNotEqual(original, different_artist)
        self.assertEqual(self.conn.execute("SELECT COUNT(*) FROM albums").fetchone()[0], 3)

    def test_rescan_repairs_mp4_track_artist_without_reassigning_the_album(self):
        root = Path("fixture-library")
        rel = "One Record (1987)/01.m4a"
        folder = str(Path(rel).parent)
        audio = SimpleNamespace(tags={}, info=SimpleNamespace(length=60))
        with patch.object(scanner, "MutagenFile", return_value=audio), patch.object(scanner, "_resolve_art_for_album"):
            item = dict(rel=rel, mtime=10, size=20, prev=None, meta=scanner._parse_tags(root / rel))
            album_id, added, _ = scanner._index_album_folder(self.conn, root, folder, [item], 1, False)
            self.assertEqual(added, 1)
            track_id = self.conn.execute("SELECT id FROM tracks").fetchone()[0]

            audio.tags["\xa9ART"] = ["Singer crédit"]
            item.update(prev=(10, 20, track_id, album_id, "", ""), meta=scanner._parse_tags(root / rel))
            updated_album, added, updated = scanner._index_album_folder(self.conn, root, folder, [item], 2, False)

        self.assertEqual((updated_album, added, updated), (album_id, 0, 1))
        track = self.conn.execute("SELECT id, album_id, artist FROM tracks").fetchone()
        self.assertEqual(tuple(track), (track_id, album_id, "Singer crédit"))


if __name__ == "__main__":
    unittest.main()
