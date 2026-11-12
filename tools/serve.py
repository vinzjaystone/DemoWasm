#!/usr/bin/env python3
"""Serve a non-threaded WASM build, including through an iframe preview proxy."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}

    def __init__(self, *args, start_page="index.html", **kwargs):
        self.start_page = start_page
        super().__init__(*args, **kwargs)

    def redirect_start_page(self):
        if self.start_page == "index.html" or urlsplit(self.path).path != "/":
            return False
        self.send_response(302)
        self.send_header("Location", self.start_page)
        self.end_headers()
        return True

    def do_GET(self):
        if not self.redirect_start_page():
            super().do_GET()

    def do_HEAD(self):
        if not self.redirect_start_page():
            super().do_HEAD()

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
    parser.add_argument("--page", choices=("index.html", "editor.html"), default="index.html")
    args = parser.parse_args()
    directory = args.directory.resolve()
    if not (directory / args.page).is_file():
        parser.error(f"{directory}/{args.page} does not exist; run `make web` first")
    server = ThreadingHTTPServer(("0.0.0.0", args.port), partial(Handler, directory=str(directory), start_page=args.page))
    print(f"Ashfall: serving {directory} on http://0.0.0.0:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
