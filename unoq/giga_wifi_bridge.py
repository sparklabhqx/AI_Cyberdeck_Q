#!/usr/bin/env python3
import os
import socket
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

HOST = os.environ.get("UNOQ_BRIDGE_BIND_HOST", "0.0.0.0").strip() or "0.0.0.0"
PORT = int(os.environ.get("UNOQ_BRIDGE_PORT", "8080"))

_state_lock = threading.Lock()
_pending_message = ""


def get_lan_ip() -> str:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect(("8.8.8.8", 80))
            return sock.getsockname()[0]
    except OSError:
        return "127.0.0.1"


def set_pending_message(text: str) -> str:
    global _pending_message
    sanitized = text.replace("\r\n", "\n").replace("\r", "\n")
    with _state_lock:
        _pending_message = sanitized
    return sanitized


def pop_pending_message() -> str:
    global _pending_message
    with _state_lock:
        message = _pending_message
        _pending_message = ""
    return message


class BridgeHandler(BaseHTTPRequestHandler):
    server_version = "GigaWifiBridge/0.1"

    def log_message(self, fmt: str, *args) -> None:
        print(f"[{self.log_date_time_string()}] {self.client_address[0]} {fmt % args}", flush=True)

    def send_plain_text(self, status: int, body: str) -> None:
        data = body.encode("utf-8", "replace")
        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path
        query = parse_qs(parsed.query, keep_blank_values=True)

        if path == "/health":
            self.send_plain_text(200, "OK")
            return

        if path == "/latest":
            self.send_plain_text(200, pop_pending_message())
            return

        if path == "/push":
            text = query.get("text", [""])[0]
            stored = set_pending_message(text)
            self.send_plain_text(200, f"STORED {len(stored)}")
            return

        if path == "/clear":
            set_pending_message("/clear")
            self.send_plain_text(200, "STORED /clear")
            return

        if path == "/divider":
            set_pending_message("/divider")
            self.send_plain_text(200, "STORED /divider")
            return

        self.send_plain_text(404, "NOT FOUND")


if __name__ == "__main__":
    lan_ip = get_lan_ip()
    print(f"[bridge] listening on http://{HOST}:{PORT}", flush=True)
    print(f"[bridge] lan endpoint http://{lan_ip}:{PORT}", flush=True)
    ThreadingHTTPServer((HOST, PORT), BridgeHandler).serve_forever()
