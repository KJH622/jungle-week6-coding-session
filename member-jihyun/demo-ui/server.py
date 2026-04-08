#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import struct
import shutil
import subprocess
import tempfile
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse


ROOT_DIR = Path(__file__).resolve().parents[2]
DEMO_DIR = ROOT_DIR / "member-jihyun" / "demo-ui"
SRC_DIR = ROOT_DIR / "member-jihyun" / "src"
SCHEMA_DIR = ROOT_DIR / "common" / "schema"
RUNTIME_DIR = DEMO_DIR / ".runtime"
BINARY_PATH = SRC_DIR / "sql_processor"
CACHE_STATS_PATH = RUNTIME_DIR / "cache_stats.json"
PAGE_SIZE = 4096
PAGE_HEADER_SIZE = 16


def build_binary() -> None:
    subprocess.run(
        ["make"],
        cwd=SRC_DIR,
        check=True,
        capture_output=True,
        text=True,
    )


def ensure_runtime_dir() -> None:
    RUNTIME_DIR.mkdir(parents=True, exist_ok=True)
    for schema_path in SCHEMA_DIR.glob("*.schema"):
        shutil.copy2(schema_path, RUNTIME_DIR / schema_path.name)


def reset_runtime_data() -> None:
    ensure_runtime_dir()
    for data_path in RUNTIME_DIR.glob("*.data"):
        data_path.unlink()
    for sql_path in RUNTIME_DIR.glob("demo_*.sql"):
        sql_path.unlink()
    CACHE_STATS_PATH.unlink(missing_ok=True)


def get_data_file_state(table_name: str) -> dict[str, object]:
    data_path = RUNTIME_DIR / f"{table_name}.data"
    if not data_path.exists():
        return {
            "table": table_name,
            "pageCount": 0,
            "rowCount": 0,
            "lastPageUsedBytes": 0,
        }

    page_count = 0
    row_count = 0
    last_page_used_bytes = 0

    with data_path.open("rb") as file:
        while True:
            page = file.read(PAGE_SIZE)
            if not page:
                break
            if len(page) < PAGE_HEADER_SIZE:
                break

            page_id, next_page_id, page_row_count, used_bytes = struct.unpack("<IIII", page[:PAGE_HEADER_SIZE])
            _ = page_id, next_page_id

            page_count += 1
            row_count += int(page_row_count)
            last_page_used_bytes = int(used_bytes)

    return {
        "table": table_name,
        "pageCount": page_count,
        "rowCount": row_count,
        "lastPageUsedBytes": last_page_used_bytes,
    }


def build_state(last_exit_code: int | None = None) -> dict[str, object]:
    ensure_runtime_dir()
    cache_stats = {
        "hit": 0,
        "miss": 0,
        "dirtyPages": 0,
        "flushCount": 0,
    }

    if CACHE_STATS_PATH.exists():
        try:
            raw = json.loads(CACHE_STATS_PATH.read_text(encoding="utf-8"))
            cache_stats["hit"] = int(raw.get("hit", 0))
            cache_stats["miss"] = int(raw.get("miss", 0))
            cache_stats["dirtyPages"] = int(raw.get("dirtyPages", 0))
            cache_stats["flushCount"] = int(raw.get("flushCount", 0))
        except (json.JSONDecodeError, OSError, ValueError, TypeError):
            pass

    return {
        "binaryReady": BINARY_PATH.exists(),
        "runtimeDir": str(RUNTIME_DIR),
        "lastExitCode": last_exit_code,
        "cacheStats": cache_stats,
        "dataFiles": {
            "users": get_data_file_state("users"),
            "products": get_data_file_state("products"),
        },
    }


class DemoRequestHandler(SimpleHTTPRequestHandler):
    last_exit_code: int | None = None

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DEMO_DIR), **kwargs)

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/api/state":
            self._send_json(HTTPStatus.OK, build_state(self.last_exit_code))
            return

        if parsed.path == "/":
            self.path = "/index.html"

        super().do_GET()

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/api/run-sql":
            self._handle_run_sql()
            return

        if parsed.path == "/api/reset-data":
            self._handle_reset_data()
            return

        self._send_json(HTTPStatus.NOT_FOUND, {"error": "not found"})

    def log_message(self, format: str, *args) -> None:
        return

    def _handle_run_sql(self) -> None:
        try:
            payload = self._read_json_body()
            sql = str(payload.get("sql", ""))

            build_binary()
            ensure_runtime_dir()

            with tempfile.NamedTemporaryFile(
                mode="w",
                encoding="utf-8",
                suffix=".sql",
                prefix="demo_",
                dir=RUNTIME_DIR,
                delete=False,
            ) as temp_sql:
                temp_sql.write(sql)
                temp_sql_path = Path(temp_sql.name)

            try:
                CACHE_STATS_PATH.unlink(missing_ok=True)
                env = dict(os.environ)
                env["SQL_PROCESSOR_CACHE_STATS_PATH"] = str(CACHE_STATS_PATH)
                result = subprocess.run(
                    [str(BINARY_PATH), temp_sql_path.name],
                    cwd=RUNTIME_DIR,
                    capture_output=True,
                    text=True,
                    timeout=5,
                    check=False,
                    env=env,
                )
            finally:
                temp_sql_path.unlink(missing_ok=True)

            self.last_exit_code = result.returncode
            self._send_json(
                HTTPStatus.OK,
                {
                    "stdout": result.stdout,
                    "stderr": result.stderr,
                    "exitCode": result.returncode,
                    "state": build_state(self.last_exit_code),
                },
            )
        except subprocess.CalledProcessError as error:
            self._send_json(
                HTTPStatus.INTERNAL_SERVER_ERROR,
                {
                    "error": "build failed",
                    "stdout": error.stdout,
                    "stderr": error.stderr,
                },
            )
        except subprocess.TimeoutExpired:
            self._send_json(
                HTTPStatus.REQUEST_TIMEOUT,
                {"error": "sql_processor timed out after 5 seconds"},
            )
        except json.JSONDecodeError:
            self._send_json(HTTPStatus.BAD_REQUEST, {"error": "invalid json"})

    def _handle_reset_data(self) -> None:
        reset_runtime_data()
        self.last_exit_code = None
        self._send_json(
            HTTPStatus.OK,
            {
                "message": "runtime data reset",
                "state": build_state(self.last_exit_code),
            },
        )

    def _read_json_body(self) -> dict[str, object]:
        content_length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(content_length) if content_length > 0 else b"{}"
        return json.loads(body.decode("utf-8"))

    def _send_json(self, status: HTTPStatus, payload: dict[str, object]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main() -> None:
    reset_runtime_data()
    server = ThreadingHTTPServer(("127.0.0.1", 8000), DemoRequestHandler)
    print("Demo server running at http://127.0.0.1:8000")
    server.serve_forever()


if __name__ == "__main__":
    main()
