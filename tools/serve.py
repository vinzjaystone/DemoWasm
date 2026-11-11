#!/usr/bin/env python3
"""Serve a non-threaded WASM build, including through an iframe preview proxy."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        self.send_header("X-Content-Type-Options", "nosniff")
        # No host allowlist, X-Frame-Options, or COEP: this single-threaded build
        # needs neither SharedArrayBuffer nor cross-origin isolation.
        super().end_headers()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--directory", type=Path, default=Path("build/web"))
    args = parser.parse_args()
    directory = args.directory.resolve()
    if not (directory / "index.html").is_file():
        parser.error(f"{directory}/index.html does not exist; run `make web` first")
    server = ThreadingHTTPServer(("0.0.0.0", args.port), partial(Handler, directory=str(directory)))
    print(f"Ashfall: serving {directory} on http://0.0.0.0:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
