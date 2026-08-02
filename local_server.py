#!/usr/bin/env python3
"""Serve the dashboard and expose the current local Codex usage on localhost.

The service never reads or sends auth.json itself. It starts `codex app-server`,
asks that process for account usage, then returns only display-safe statistics to
the browser. Keep this server on 127.0.0.1.
"""

from __future__ import annotations

import argparse
import json
import queue
import shutil
import subprocess
import threading
from datetime import date, timedelta
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


ROOT_DIR = Path(__file__).resolve().parent
REQUEST_TIMEOUT_SECONDS = 15


class CodexAppServerError(RuntimeError):
    """A concise error suitable for showing in the local browser UI."""


class CodexAppServer:
    def __init__(self) -> None:
        self.process: subprocess.Popen[str] | None = None
        self.messages: queue.Queue[dict[str, Any] | None] = queue.Queue()

    @staticmethod
    def executable() -> str:
        # Prefer the npm command wrapper on Windows. The packaged Desktop
        # binary can be present in PATH but unavailable to a child process.
        for candidate in ("codex.cmd", "codex.exe", "codex"):
            executable = shutil.which(candidate)
            if executable:
                return executable
        raise CodexAppServerError("未找到 codex 命令。请先安装并登录 Codex。")

    def start(self) -> None:
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        self.process = subprocess.Popen(
            [self.executable(), "app-server"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="utf-8",
            bufsize=1,
            creationflags=creationflags,
        )
        assert self.process.stdout is not None
        threading.Thread(target=self._read_stdout, daemon=True).start()

    def _read_stdout(self) -> None:
        assert self.process is not None and self.process.stdout is not None
        for line in self.process.stdout:
            try:
                self.messages.put(json.loads(line))
            except json.JSONDecodeError:
                continue
        self.messages.put(None)

    def send(self, message: dict[str, Any]) -> None:
        if not self.process or not self.process.stdin:
            raise CodexAppServerError("Codex app-server 未启动。")
        self.process.stdin.write(json.dumps(message, ensure_ascii=False) + "\n")
        self.process.stdin.flush()

    def request(self, request_id: int, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        self.send({"id": request_id, "method": method, "params": params or {}})
        while True:
            try:
                message = self.messages.get(timeout=REQUEST_TIMEOUT_SECONDS)
            except queue.Empty as error:
                raise CodexAppServerError(f"等待 Codex 响应超时：{method}") from error
            if message is None:
                raise CodexAppServerError("Codex app-server 已提前退出。")
            if message.get("id") != request_id:
                continue
            if "error" in message:
                detail = message["error"].get("message", "未知错误")
                raise CodexAppServerError(f"Codex 请求失败：{detail}")
            return message.get("result", {})

    def fetch_usage(self) -> dict[str, Any]:
        self.start()
        try:
            self.request(
                1,
                "initialize",
                {
                    "clientInfo": {
                        "name": "codex-ink-display",
                        "title": "Codex Ink Display",
                        "version": "1.0.0",
                    },
                    "protocolVersion": 2,
                },
            )
            self.send({"method": "initialized", "params": {}})
            account = self.request(2, "account/read", {"refreshToken": False})
            usage = self.request(3, "account/usage/read")
            rate_limits = self.request(4, "account/rateLimits/read")
            return make_dashboard_payload(account, usage, rate_limits)
        finally:
            self.stop()

    def stop(self) -> None:
        if not self.process:
            return
        if self.process.stdin:
            self.process.stdin.close()
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
        self.process = None


def number(value: Any) -> int:
    try:
        return max(0, int(value))
    except (TypeError, ValueError):
        return 0


def usage_percent(window: Any) -> float | None:
    if not isinstance(window, dict) or window.get("usedPercent") is None:
        return None
    try:
        return max(0.0, min(100.0, float(window["usedPercent"])))
    except (TypeError, ValueError):
        return None


def make_dashboard_payload(
    account_response: dict[str, Any], usage_response: dict[str, Any], rate_limits_response: dict[str, Any]
) -> dict[str, Any]:
    summary = usage_response.get("summary") if isinstance(usage_response.get("summary"), dict) else {}
    buckets = usage_response.get("dailyUsageBuckets")
    tokens_by_day: dict[str, int] = {}
    if isinstance(buckets, list):
        for bucket in buckets:
            if isinstance(bucket, dict) and isinstance(bucket.get("startDate"), str):
                tokens_by_day[bucket["startDate"]] = number(bucket.get("tokens"))

    today = date.today()
    history_dates = [today - timedelta(days=offset) for offset in range(6, -1, -1)]
    history = [tokens_by_day.get(day.isoformat(), 0) for day in history_dates]
    month_prefix = today.strftime("%Y-%m-")
    month_tokens = sum(tokens for day, tokens in tokens_by_day.items() if day.startswith(month_prefix))
    rate_limits = rate_limits_response.get("rateLimits")
    rate_limits = rate_limits if isinstance(rate_limits, dict) else {}
    account = account_response.get("account")
    account = account if isinstance(account, dict) else {}

    return {
        "account": {
            "type": account.get("type"),
            "planType": account.get("planType"),
        },
        "usage": {
            "todayTokens": tokens_by_day.get(today.isoformat(), 0),
            "monthTokens": month_tokens,
            "lifetimeTokens": number(summary.get("lifetimeTokens")),
            "peakDailyTokens": number(summary.get("peakDailyTokens")),
            "currentStreakDays": number(summary.get("currentStreakDays")),
            "longestStreakDays": number(summary.get("longestStreakDays")),
            "history": history,
        },
        "rateLimits": {
            "primaryUsedPercent": usage_percent(rate_limits.get("primary")),
            "secondaryUsedPercent": usage_percent(rate_limits.get("secondary")),
            "primaryResetsAt": rate_limits.get("primary", {}).get("resetsAt")
            if isinstance(rate_limits.get("primary"), dict)
            else None,
            "secondaryResetsAt": rate_limits.get("secondary", {}).get("resetsAt")
            if isinstance(rate_limits.get("secondary"), dict)
            else None,
        },
    }


class DashboardHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, directory=str(ROOT_DIR), **kwargs)

    def do_GET(self) -> None:  # noqa: N802
        if self.path == "/":
            self.send_response(HTTPStatus.FOUND)
            self.send_header("Location", "/html/")
            self.end_headers()
            return
        if self.path.split("?", 1)[0] == "/api/usage":
            self.serve_usage()
            return
        super().do_GET()

    def serve_usage(self) -> None:
        try:
            payload = CodexAppServer().fetch_usage()
            self.send_json(HTTPStatus.OK, payload)
        except CodexAppServerError as error:
            self.send_json(HTTPStatus.BAD_GATEWAY, {"error": str(error)})
        except Exception:
            self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": "本机看板服务发生未预期错误。"})

    def send_json(self, status: HTTPStatus, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format: str, *args: Any) -> None:
        print(f"[server] {self.address_string()} - {format % args}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Codex 墨水屏用量看板本机服务")
    parser.add_argument("--host", default="127.0.0.1", help="监听地址（默认仅本机）")
    parser.add_argument("--port", default=8765, type=int, help="监听端口")
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), DashboardHandler)
    print(f"Codex 用量看板已启动：http://{args.host}:{args.port}/html/")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n服务已停止。")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
