# Preserve browser data before replacing the local kiosk

Favourites, regular playlists and radio are already in `/etc/pitunes/userdata.json`.
Preserve that file and settings/music/MPD state independently from the rebuildable
library cache. Numeric album favourites can become stale after a destructive DB
rebuild; do not reset the DB as an import shortcut.

Smart playlists originally live only in each browser's `pitunes-smart-playlists`
localStorage. The native app never opens a Chromium profile or removes browser
data. Before erasing an old appliance image, export from the actual browser/profile
that created the rules. Another browser on the same device does not share them.

On a client already served by the native remote frontend at the same origin,
open `http://pitunes.local/migrate.html` and download the export. This reads only
the explicit PiTunes whitelist and leaves localStorage unchanged. If still on the
original frontend, use its browser console to copy this payload into a JSON file:

```js
JSON.stringify({schemaVersion:1, product:"PiTunes",
  smartPlaylists:JSON.parse(localStorage.getItem("pitunes-smart-playlists") || "[]"),
  preferences:{}}, null, 2)
```

Keep the export with your private backup. In native Settings → Library → Import
browser data, choose it. The additive import persists definitions in the same
backend userdata store, retains unknown rule fields, deduplicates identical IDs,
and refuses conflicting IDs or corrupt existing storage without overwriting it.
It does not automatically apply browser output routes or local music paths to
appliance audio/mount settings. Unsupported preference keys are rejected.

**Native smart rule editing/evaluation/playback is still pending.** The import
protects definitions, including fields not populated by the old music schema;
it does not pretend those rules have executed. Tests cover verbatim round-trip,
idempotence, preservation of favourites/playlists/unknown keys, conflicts, corrupt
storage, and the real HTTP import/list routes. Browser-original export data is
never deleted by this workflow.
