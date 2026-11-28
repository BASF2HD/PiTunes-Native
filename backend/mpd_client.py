import socket
import threading

from shared import ApiError, MPD_HOST, MPD_PORT, parse_mpd_lines


class MPDClient:
    """Thread-safe MPD client with one persistent connection per thread."""

    def __init__(self, host=MPD_HOST, port=MPD_PORT, timeout=5):
        self.host = host
        self.port = port
        self.timeout = timeout
        self._local = threading.local()

    def _get_socket(self):
        sock = getattr(self._local, "sock", None)
        fh = getattr(self._local, "fh", None)
        if sock is not None and fh is not None:
            return sock, fh
        sock = socket.create_connection((self.host, self.port), self.timeout)
        fh = sock.makefile("rwb", buffering=0)
        greeting = fh.readline().decode("utf-8", errors="replace").strip()
        if not greeting.startswith("OK MPD"):
            fh.close()
            sock.close()
            raise ApiError(502, "MPD did not return a valid greeting")
        self._local.sock = sock
        self._local.fh = fh
        return sock, fh

    def _reset(self):
        for stream in (getattr(self._local, "fh", None), getattr(self._local, "sock", None)):
            if stream is not None:
                try:
                    stream.close()
                except OSError:
                    pass
        self._local.sock = None
        self._local.fh = None

    def command(self, command):
        try:
            _sock, fh = self._get_socket()
        except OSError as exc:
            self._reset()
            raise ApiError(502, str(exc)) from exc
        try:
            fh.write((command + "\n").encode("utf-8"))
            lines = []
            while True:
                raw = fh.readline()
                if not raw:
                    raise ApiError(502, "MPD connection closed unexpectedly")
                line = raw.decode("utf-8", errors="replace").rstrip("\n")
                if line.startswith("ACK"):
                    raise ApiError(502, line)
                lines.append(line)
                if line == "OK":
                    return lines
        except (OSError, ApiError) as exc:
            self._reset()
            if isinstance(exc, ApiError):
                raise
            raise ApiError(502, str(exc)) from exc

    def entries(self, command):
        return parse_mpd_lines(self.command(command))

    def single_map(self, command):
        result = {}
        for line in self.command(command):
            if ": " in line:
                key, value = line.split(": ", 1)
                result[key] = value
        return result

    def binary(self, command):
        _sock, fh = self._get_socket()
        try:
            fh.write((command + "\n").encode("utf-8"))
            metadata = {}
            chunks = []
            while True:
                raw = fh.readline()
                if not raw:
                    raise ApiError(502, "MPD connection closed unexpectedly")
                line = raw.decode("utf-8", errors="replace").rstrip("\n")
                if line.startswith("ACK"):
                    raise ApiError(404, line)
                if line == "OK":
                    break
                if ": " not in line:
                    continue
                key, value = line.split(": ", 1)
                if key == "binary":
                    size = int(value)
                    data = fh.read(size)
                    chunks.append(data)
                    fh.read(1)
                else:
                    metadata[key] = value
            if not chunks:
                raise ApiError(404, "No artwork returned by MPD")
            return b"".join(chunks), metadata
        except (OSError, ApiError):
            self._reset()
            raise


mpd = MPDClient()
