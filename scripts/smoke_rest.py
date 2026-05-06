#!/usr/bin/env python3
"""
HTTP smoke tests against a running SSVC OpenConnect firmware (real device or emulator).

Uses only the Python standard library (urllib). Auth: optional Bearer JWT when FT_SECURITY is on.

Example:
  python scripts/smoke_rest.py http://192.168.1.42
  python scripts/smoke_rest.py http://192.168.1.42 --extended
  python scripts/smoke_rest.py http://192.168.1.42 --extended --ssvc-step-delay 4 --require-telemetry-progress
  set SMOKE_USER / SMOKE_PASSWORD (default admin/admin)
"""

from __future__ import annotations

import argparse
import json
import ssl
import sys
import time
import threading
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from dataclasses import dataclass, field
from typing import Any


@dataclass
class SmokeClient:
    base: str
    token: str | None = None
    insecure: bool = False
    timeout: float = 30.0
    user_agent: str = "ssvc-openconnect-smoke/1.0"

    def _headers(self, extra: dict[str, str] | None = None) -> dict[str, str]:
        h = {
            "User-Agent": self.user_agent,
            "Accept": "application/json, text/plain, */*",
        }
        if extra:
            h.update(extra)
        if self.token:
            h["Authorization"] = f"Bearer {self.token}"
        return h

    def request(
        self,
        method: str,
        path: str,
        *,
        query: dict[str, str] | None = None,
        json_body: Any = None,
        data: bytes | None = None,
        content_type: str | None = None,
        timeout_s: float | None = None,
    ) -> tuple[int, dict[str, str], bytes]:
        url = self.base.rstrip("/") + path
        if query:
            url += "?" + urllib.parse.urlencode(query)
        headers = self._headers()
        body: bytes | None = data
        if json_body is not None:
            body = json.dumps(json_body).encode("utf-8")
            headers["Content-Type"] = content_type or "application/json"
        req = urllib.request.Request(url, data=body, headers=headers, method=method)
        ctx = None
        if self.insecure and url.lower().startswith("https"):
            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
        deadline = self.timeout if timeout_s is None else timeout_s
        try:
            with urllib.request.urlopen(req, timeout=deadline, context=ctx) as resp:
                raw = resp.read()
                hdrs = {k.lower(): v for k, v in resp.headers.items()}
                return resp.status, hdrs, raw
        except urllib.error.HTTPError as e:
            raw = e.read()
            hdrs = {k.lower(): v for k, v in e.headers.items()} if e.headers else {}
            return e.code, hdrs, raw
        except TimeoutError:
            return (
                408,
                {},
                b'{"error":"request timed out (client)"}',
            )


@dataclass
class SmokeResult:
    passed: int = 0
    failed: int = 0
    skips: int = 0
    errors: list[str] = field(default_factory=list)

    def ok(self, name: str) -> None:
        self.passed += 1
        print(f"  OK  {name}")

    def fail(self, name: str, detail: str) -> None:
        self.failed += 1
        msg = f"  FAIL {name}: {detail}"
        print(msg)
        self.errors.append(msg)

    def skip(self, name: str, reason: str) -> None:
        self.skips += 1
        print(f"  SKIP {name}: {reason}")


def _json_loads(raw: bytes) -> Any:
    if not raw:
        return None
    try:
        return json.loads(raw.decode("utf-8", errors="replace"))
    except json.JSONDecodeError:
        return None


def _verify_telemetry_root(data: dict[str, Any]) -> str | None:
    """Return error message if /rest/telemetry payload is unusable; else None."""
    if data.get("error"):
        return str(data["error"])
    st = data.get("status")
    if not isinstance(st, dict):
        return "missing or invalid status object"
    if not isinstance(st.get("stage"), str):
        return "status.stage must be a string"
    # Inner process state is also keyed \"status\" (running/idle/...)
    if not isinstance(st.get("status"), str):
        return "status.status (process state) must be a string"
    return None


def _telemetry_snapshot(data: dict[str, Any]) -> tuple[Any, ...]:
    """Snapshot for progress checks (excludes lastUpdate so polling is stable)."""
    st = data.get("status") if isinstance(data.get("status"), dict) else {}
    tel = data.get("telemetry") if isinstance(data.get("telemetry"), dict) else {}
    return (
        st.get("stage"),
        st.get("status"),
        tel.get("type"),
        st.get("uartConnectionError"),
    )


def _read_telemetry(c: SmokeClient) -> tuple[dict[str, Any] | None, str | None]:
    code, _, body = c.request("GET", "/rest/telemetry", timeout_s=max(c.timeout, 30.0))
    if code != 200:
        return None, f"GET /rest/telemetry -> HTTP {code}"
    data = _json_loads(body)
    if not isinstance(data, dict):
        return None, "response is not a JSON object"
    err = _verify_telemetry_root(data)
    if err:
        return None, err
    return data, None


def _observe_after_ssvc_command(
    c: SmokeClient,
    r: SmokeResult,
    label: str,
    prev_snap: tuple[Any, ...],
    args: argparse.Namespace,
) -> tuple[Any, ...] | None:
    """
    Wait ssvc_step_delay, then read /rest/telemetry.
    If require_telemetry_progress: poll until snapshot changes or telemetry_wait elapses.
    """
    delay = float(args.ssvc_step_delay)
    if delay > 0:
        time.sleep(delay)

    max_wait = float(args.telemetry_wait)
    interval = float(args.telemetry_poll_interval)
    deadline = time.monotonic() + max_wait

    while True:
        data, err = _read_telemetry(c)
        if data is None:
            r.fail(f"{label} /rest/telemetry", err or "unknown")
            return None
        snap = _telemetry_snapshot(data)
        stg, proc, typ, uart = snap
        if not args.require_telemetry_progress:
            r.ok(
                f"{label}: telemetry stage={stg!r} process={proc!r} "
                f"type={typ!r} uartErr={uart!r}"
            )
            return snap
        if snap != prev_snap:
            r.ok(
                f"{label}: telemetry changed -> stage={stg!r} process={proc!r} "
                f"type={typ!r} uartErr={uart!r}"
            )
            return snap
        if time.monotonic() >= deadline:
            r.fail(
                f"{label} telemetry progress",
                f"no change after {max_wait}s (stage={stg!r} process={proc!r} type={typ!r})",
            )
            return None
        time.sleep(interval)


def _pick_ssvc_roundtrip_payload(ssvc: dict[str, Any]) -> dict[str, Any] | None:
    """Single-key PUT body that PARAM_HANDLERS accepts (idempotent if value unchanged)."""
    if "hyst" in ssvc:
        return {"hyst": float(ssvc["hyst"])}
    if "formula" in ssvc:
        return {"formula": bool(ssvc["formula"])}
    if "decrement" in ssvc:
        return {"decrement": int(ssvc["decrement"])}
    if "tank_mmhg" in ssvc:
        return {"tank_mmhg": float(ssvc["tank_mmhg"])}
    return None


def run_extended_smoke(c: SmokeClient, args: argparse.Namespace, r: SmokeResult) -> None:
    """SSVC settings round-trip, /rest/zones, rectification commands (requires stable UART/emulator)."""
    print("\n--- extended ---\n")

    cmd_timeout = max(float(args.command_timeout), c.timeout)

    # --- PUT one SSVC field read from GET (validates SettingsHandler + PARAM_HANDLERS path) ---
    code, _, body = c.request("GET", "/rest/settings")
    if code != 200:
        r.fail("extended GET /rest/settings", f"HTTP {code}")
    else:
        root = _json_loads(body)
        ssvc = root.get("ssvcSettings") if isinstance(root, dict) else None
        if isinstance(ssvc, dict):
            payload = _pick_ssvc_roundtrip_payload(ssvc)
            if payload:
                code2, _, body2 = c.request(
                    "PUT",
                    "/rest/settings",
                    json_body=payload,
                    timeout_s=max(c.timeout, 60.0),
                )
                if code2 != 200:
                    r.fail(
                        "extended PUT /rest/settings (round-trip field)",
                        f"HTTP {code2}: {body2[:200]!r}",
                    )
                else:
                    key = next(iter(payload.keys()))
                    r.ok(f"extended PUT /rest/settings {key!r} (same as GET)")
                    # SSVC queue processes settings — wait briefly then confirm telemetry shape.
                    time.sleep(min(2.0, max(0.5, float(args.ssvc_step_delay))))
                    tdata, terr = _read_telemetry(c)
                    if tdata is None:
                        r.fail("extended /rest/telemetry after settings PUT", terr or "")
                    else:
                        ts = _telemetry_snapshot(tdata)
                        r.ok(
                            "extended /rest/telemetry after settings PUT "
                            f"(stage={ts[0]!r} process={ts[1]!r} type={ts[2]!r})"
                        )
            else:
                r.skip(
                    "extended SSVC round-trip",
                    "no hyst/formula/decrement/tank_mmhg in ssvcSettings",
                )
        else:
            r.fail("extended GET settings shape", "missing ssvcSettings object")

    # --- Bulk zones (HttpEndpoint POST /rest/zones; GET requires admin by default) ---
    code, _, body = c.request("GET", "/rest/zones")
    if code == 403:
        r.skip("extended GET /rest/zones", "403 - admin required")
    elif code != 200:
        r.fail("extended GET /rest/zones", f"HTTP {code}: {body[:200]!r}")
    else:
        r.ok("extended GET /rest/zones")
        zroot = _json_loads(body)
        zones = zroot.get("zones") if isinstance(zroot, dict) else None
        if isinstance(zones, dict) and zones:
            code2, _, body2 = c.request(
                "POST",
                "/rest/zones",
                json_body={"zones": dict(zones)},
                timeout_s=max(c.timeout, 60.0),
            )
            if code2 != 200:
                r.fail("extended POST /rest/zones (idempotent)", f"HTTP {code2}: {body2[:200]!r}")
            else:
                r.ok("extended POST /rest/zones (same map as GET)")
                time.sleep(min(1.5, max(0.3, float(args.ssvc_step_delay) * 0.5)))
                zt, zerr = _read_telemetry(c)
                if zt is None:
                    r.fail("extended /rest/telemetry after zones POST", zerr or "")
                else:
                    zs = _telemetry_snapshot(zt)
                    r.ok(
                        "extended /rest/telemetry after zones POST "
                        f"(stage={zs[0]!r} process={zs[1]!r})"
                    )
        else:
            r.skip("extended POST /rest/zones", "empty zones map (nothing to echo)")

    # --- Rectification: pause between commands, validate /rest/telemetry ---
    base_t, base_err = _read_telemetry(c)
    if base_t is None:
        r.fail("extended baseline /rest/telemetry", base_err or "")
        return
    prev_snap = _telemetry_snapshot(base_t)
    r.ok(
        "extended baseline telemetry "
        f"stage={prev_snap[0]!r} process={prev_snap[1]!r} type={prev_snap[2]!r} "
        f"uart={prev_snap[3]!r}"
    )

    for cmd in ("start", "next", "stop"):
        code, _, body = c.request(
            "POST",
            "/rest/commands",
            json_body={"commands": cmd},
            timeout_s=cmd_timeout,
        )
        if code == 408:
            r.fail(f"extended POST /rest/commands {cmd!r}", "timed out - try --command-timeout")
            break
        if code != 200:
            r.fail(f"extended POST /rest/commands {cmd!r}", f"HTTP {code}: {body[:200]!r}")
            break
        r.ok(f"extended POST /rest/commands {cmd!r} (HTTP 200)")
        after = _observe_after_ssvc_command(
            c, r, f"after command {cmd!r}", prev_snap, args
        )
        if after is None:
            break
        prev_snap = after


def establish_auth(c: SmokeClient, user: str, password: str, r: SmokeResult) -> bool:
    code, _, body = c.request("GET", "/rest/verifyAuthorization")
    if code == 200:
        r.ok("auth: open or already authorized (verifyAuthorization 200 without login)")
        return True
    if code == 404:
        r.skip("auth", "verifyAuthorization returned 404 (FT_SECURITY off?) - continuing without token")
        return True
    if code != 401:
        r.fail("auth", f"unexpected verifyAuthorization status {code}: {body[:200]!r}")
        return False

    code2, _, body2 = c.request(
        "POST",
        "/rest/signIn",
        json_body={"username": user, "password": password},
    )
    if code2 != 200:
        r.fail("auth signIn", f"HTTP {code2}: {body2[:300]!r}")
        return False
    data = _json_loads(body2)
    if not isinstance(data, dict) or "access_token" not in data:
        r.fail("auth signIn", f"missing access_token in JSON: {data!r}")
        return False
    c.token = str(data["access_token"])
    code3, _, _ = c.request("GET", "/rest/verifyAuthorization")
    if code3 != 200:
        r.fail("auth after login", f"verifyAuthorization expected 200, got {code3}")
        return False
    r.ok("auth: JWT signIn + verifyAuthorization")
    return True


MOCK_JWT = "mock_smoke_jwt"


class _SmokeMockHandler(BaseHTTPRequestHandler):
    """Minimal stub of REST endpoints for `python smoke_rest.py --self-test`."""

    protocol_version = "HTTP/1.1"
    # Mutable fake rectification state for GET /rest/telemetry (updated by start/next/stop).
    tel_stage: str = "waiting"
    tel_process: str = "idle"
    tel_type: str = "waiting"
    tel_last_update: int = 1000

    def log_message(self, format: str, *args: Any) -> None:
        return

    def _auth_header(self) -> str | None:
        return self.headers.get("Authorization")

    def _send_raw(self, code: int, body: bytes, content_type: str = "application/json") -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_body(self) -> bytes:
        n = int(self.headers.get("Content-Length", "0") or "0")
        return self.rfile.read(n) if n else b""

    def do_GET(self) -> None:
        path = urllib.parse.urlparse(self.path).path
        auth = self._auth_header()

        if path == "/rest/verifyAuthorization":
            if auth and MOCK_JWT in (auth or ""):
                self._send_raw(200, b"{}")
            else:
                self._send_raw(401, b'{"error":"unauthorized"}')
            return

        if path == "/rest/settings":
            if not auth:
                self._send_raw(401, b"{}")
                return
            self._send_raw(
                200,
                json.dumps({"ssvcSettings": {"hyst": 0.5}}).encode(),
            )
            return

        if path == "/rest/zones":
            if not auth:
                self._send_raw(401, b"{}")
                return
            self._send_raw(
                200,
                json.dumps({"zones": {"0000000000000001": 0}}).encode(),
            )
            return

        if path == "/rest/subsystem":
            self._send_raw(200, json.dumps({"i2c_bus": True}).encode())
            return

        if path in ("/rest/oc/info", "/rest/oc/hardware-config"):
            self._send_raw(200, b"{}")
            return

        if path == "/rest/oc/relay/capabilities":
            self._send_raw(
                200,
                json.dumps({"supported": True, "pcf8574": {"mock": True}}).encode(),
            )
            return

        if path == "/rest/oc/relay/metadata":
            relays = [
                {
                    "index": 1,
                    "editable": False,
                    "targetBit": 0,
                },
                {
                    "index": 2,
                    "editable": True,
                    "targetBit": 3,
                },
            ]
            self._send_raw(
                200,
                json.dumps({"supported": True, "relays": relays}).encode(),
            )
            return

        if path == "/rest/oc/relay/state":
            self._send_raw(
                200,
                json.dumps({"supported": True, "userShadow": 0}).encode(),
            )
            return

        if path == "/rest/profiles/active":
            self._send_raw(200, json.dumps({"id": "default"}).encode())
            return

        if path == "/rest/profiles":
            self._send_raw(200, json.dumps([]).encode())
            return

        if path == "/rest/files":
            self._send_raw(200, json.dumps([]).encode())
            return

        if path == "/rest/telemetry":
            if not auth:
                self._send_raw(401, b"{}")
                return
            cls = type(self)
            body = {
                "telemetry": {"type": cls.tel_type},
                "status": {
                    "stage": cls.tel_stage,
                    "status": cls.tel_process,
                    "stages": {},
                    "uartConnectionError": False,
                },
                "lastUpdate": cls.tel_last_update,
            }
            self._send_raw(200, json.dumps(body).encode())
            return

        if path == "/rest/telegram/config":
            self.send_error(404)
            return

        self.send_error(404)

    def do_POST(self) -> None:
        path = urllib.parse.urlparse(self.path).path
        raw = self._read_body()

        if path == "/rest/signIn":
            self._send_raw(
                200,
                json.dumps({"access_token": MOCK_JWT}).encode(),
            )
            return

        if path == "/rest/commands":
            try:
                obj = json.loads(raw.decode()) if raw else {}
            except json.JSONDecodeError:
                self.send_error(400)
                return
            cmd = obj.get("commands") if isinstance(obj, dict) else None
            if cmd == "__smoke_unknown__":
                self._send_raw(501, b"no")
                return
            if cmd == "version":
                self._send_raw(200, b"ok")
                return
            cls = type(self)
            if cmd == "start":
                cls.tel_stage = "heads"
                cls.tel_process = "running"
                cls.tel_type = "heads"
                cls.tel_last_update += 1
                self._send_raw(200, b"ok")
                return
            if cmd == "next":
                cls.tel_stage = "hearts"
                cls.tel_type = "hearts"
                cls.tel_last_update += 1
                self._send_raw(200, b"ok")
                return
            if cmd == "stop":
                cls.tel_stage = "waiting"
                cls.tel_process = "idle"
                cls.tel_type = "waiting"
                cls.tel_last_update += 1
                self._send_raw(200, b"ok")
                return
            self.send_error(500)
            return

        if path == "/rest/zones":
            try:
                obj = json.loads(raw.decode()) if raw else {}
            except json.JSONDecodeError:
                self.send_error(400)
                return
            self._send_raw(200, json.dumps(obj).encode())
            return

        if path == "/rest/oc/relay/override":
            self._send_raw(200, json.dumps({"ok": True}).encode())
            return

        self.send_error(404)

    def do_PUT(self) -> None:
        path = urllib.parse.urlparse(self.path).path
        raw = self._read_body()

        if path == "/rest/subsystem":
            try:
                parsed: Any = json.loads(raw.decode()) if raw else None
            except json.JSONDecodeError:
                self.send_error(400)
                return
            if isinstance(parsed, list):
                self._send_raw(400, b"{}")
                return
            self._send_raw(200, json.dumps({"success": True}).encode())
            return

        if path == "/rest/settings":
            self._send_raw(200, json.dumps({"success": True}).encode())
            return

        if path == "/rest/sensors/zone":
            self._send_raw(404, json.dumps({"status": "error"}).encode())
            return

        self.send_error(404)


def _run_self_test() -> int:
    _SmokeMockHandler.tel_stage = "waiting"
    _SmokeMockHandler.tel_process = "idle"
    _SmokeMockHandler.tel_type = "waiting"
    _SmokeMockHandler.tel_last_update = 1000

    server = HTTPServer(("127.0.0.1", 0), _SmokeMockHandler)
    server.allow_reuse_address = True
    port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = f"http://127.0.0.1:{port}"
    print(f"Self-test mock server on {base}\n")

    c = SmokeClient(base=base, timeout=5.0)
    r = SmokeResult()
    args = argparse.Namespace(
        check_unauthorized=True,
        skip_unauthorized_check=False,
        extended=True,
        command_timeout=30.0,
        ssvc_step_delay=0.05,
        telemetry_wait=5.0,
        telemetry_poll_interval=0.1,
        require_telemetry_progress=False,
    )

    try:
        if not establish_auth(c, "admin", "admin", r):
            return 1
        run_smoke(c, args, r)
        run_extended_smoke(c, args, r)
    finally:
        server.shutdown()
        server.server_close()

    print(f"\nDone: {r.passed} passed, {r.failed} failed, {r.skips} skipped")
    return 1 if r.failed else 0


def run_smoke(c: SmokeClient, args: argparse.Namespace, r: SmokeResult) -> None:
    # --- negatives ---
    if args.check_unauthorized:
        c_n = SmokeClient(base=c.base, token=None, insecure=c.insecure, timeout=c.timeout)
        code, _, _ = c_n.request("GET", "/rest/settings")
        if code == 401:
            r.ok("negative: GET /rest/settings without token -> 401")
        elif code == 200:
            r.skip("negative unauthorized", "got 200 without token (security disabled)")
        else:
            r.fail("negative unauthorized", f"expected 401 or 200, got {code}")

    code, _, body = c.request("POST", "/rest/commands", json_body={"commands": "__smoke_unknown__"})
    if code == 501:
        r.ok("negative: unknown command -> 501")
    else:
        r.fail("negative unknown command", f"expected 501, got {code}: {body[:120]!r}")

    code, _, _ = c.request("PUT", "/rest/subsystem", json_body=[])
    if code == 400:
        r.ok("negative: PUT subsystem invalid JSON -> 400")
    else:
        r.fail("negative subsystem json", f"expected 400, got {code}")

    # --- settings minimal PUT (empty object triggers success path in SettingsHandler) ---
    code, _, body = c.request("GET", "/rest/settings")
    if code != 200:
        r.fail("GET /rest/settings", f"HTTP {code}")
    else:
        r.ok("GET /rest/settings")
        root = _json_loads(body)
        if isinstance(root, dict) and "ssvcSettings" in root:
            r.ok("GET /rest/settings has ssvcSettings")
        else:
            r.fail("GET /rest/settings shape", "missing ssvcSettings key")

    code, _, body = c.request("PUT", "/rest/settings", json_body={})
    if code != 200:
        r.fail("PUT /rest/settings {}", f"HTTP {code}: {body[:200]!r}")
    else:
        r.ok("PUT /rest/settings {}")

    # --- subsystem ---
    code, _, body = c.request("GET", "/rest/subsystem")
    if code != 200:
        r.fail("GET /rest/subsystem", f"HTTP {code}")
    else:
        st = _json_loads(body)
        r.ok("GET /rest/subsystem")
        if isinstance(st, dict) and st:
            # Prefer a non-telegram subsystem: enabling/disabling telegram_bot can take >60s on some builds.
            keys = [k for k in st if isinstance(k, str)]
            non_tg = [k for k in keys if k != "telegram_bot"]
            first = non_tg[0] if non_tg else keys[0]
            flip = not bool(st[first])
            sub_timeout = max(c.timeout, 120.0)
            code2, _, body2 = c.request(
                "PUT",
                "/rest/subsystem",
                json_body={first: flip},
                timeout_s=sub_timeout,
            )
            if code2 != 200:
                r.fail("PUT /rest/subsystem toggle", f"HTTP {code2}: {body2[:200]!r}")
            else:
                j2 = _json_loads(body2)
                if isinstance(j2, dict) and j2.get("success") is True:
                    r.ok(f"PUT /rest/subsystem flip {first!r}")
                else:
                    r.ok(f"PUT /rest/subsystem (success field: {j2!r})")
            code3, _, body3 = c.request(
                "PUT",
                "/rest/subsystem",
                json_body={first: not flip},
                timeout_s=sub_timeout,
            )
            if code3 == 200:
                r.ok(f"PUT /rest/subsystem restore {first!r}")
            elif code3 == 408:
                r.fail("PUT subsystem restore", "timed out (try increasing --timeout)")
            else:
                r.fail("PUT subsystem restore", f"HTTP {code3}: {body3[:200]!r}")
        else:
            r.skip("PUT subsystem toggle", "empty subsystem map")

    # --- OpenConnect info & hardware ---
    for path in ("/rest/oc/info", "/rest/oc/hardware-config"):
        code, _, body = c.request("GET", path)
        if code != 200:
            r.fail(f"GET {path}", f"HTTP {code}")
        else:
            r.ok(f"GET {path}")

    # --- relay ---
    supported = False
    meta: Any = None
    code, _, body = c.request("GET", "/rest/oc/relay/capabilities")
    if code != 200:
        r.fail("GET relay/capabilities", f"HTTP {code}")
    else:
        cap = _json_loads(body)
        r.ok("GET /rest/oc/relay/capabilities")
        supported = isinstance(cap, dict) and cap.get("supported") is True

    code, _, body = c.request("GET", "/rest/oc/relay/metadata")
    if code != 200:
        r.fail("GET relay/metadata", f"HTTP {code}")
    else:
        meta = _json_loads(body)
        r.ok("GET /rest/oc/relay/metadata")


    code, _, body = c.request("GET", "/rest/oc/relay/state")
    if code != 200:
        r.fail("GET relay/state", f"HTTP {code}")
    else:
        r.ok("GET /rest/oc/relay/state")

    if supported:
        editable_bit: int | None = None
        if isinstance(meta, dict) and meta.get("relays"):
            for rel in meta["relays"]:
                if isinstance(rel, dict) and rel.get("editable") is True:
                    tb = rel.get("targetBit")
                    if isinstance(tb, int):
                        editable_bit = tb
                        break
        if editable_bit is not None:
            code, _, body = c.request(
                "POST",
                "/rest/oc/relay/override",
                json_body={"bit": editable_bit, "enable": True, "energized": False},
            )
            if code != 200:
                r.fail("POST relay/override", f"HTTP {code}: {body[:200]!r}")
            else:
                r.ok(f"POST /rest/oc/relay/override bit={editable_bit}")
            code2, _, body2 = c.request(
                "POST",
                "/rest/oc/relay/override",
                json_body={"bit": editable_bit, "enable": False, "energized": False},
            )
            if code2 == 200:
                r.ok("POST /rest/oc/relay/override clear override")
            else:
                r.fail("relay override clear", f"HTTP {code2}: {body2[:120]!r}")
        else:
            r.skip("relay override", "no editable relay in metadata (or missing targetBit)")
    else:
        r.skip("relay override", "relay path not supported in this build (PINOUT_USE_GPIO)")

    # --- SSVC command queue (harmless on emulator) ---
    for cmd in ("version",):
        code, _, body = c.request("POST", "/rest/commands", json_body={"commands": cmd})
        if code == 200:
            r.ok(f"POST /rest/commands {cmd!r}")
        else:
            r.fail(f"command {cmd}", f"HTTP {code}: {body[:200]!r}")

    # --- sensor zone (valid hex address; may 404 if no sensor) ---
    code, _, body = c.request(
        "PUT",
        "/rest/sensors/zone",
        query={"address": "0000000000000001", "zone": "unknown"},
    )
    if code in (200, 404):
        r.ok(f"PUT /rest/sensors/zone (HTTP {code} - endpoint reachable)")
    else:
        r.fail("PUT /rest/sensors/zone", f"unexpected HTTP {code}: {body[:200]!r}")

    # --- profiles ---
    code, _, body = c.request("GET", "/rest/profiles/active")
    if code != 200:
        r.fail("GET /rest/profiles/active", f"HTTP {code}")
    else:
        r.ok("GET /rest/profiles/active")

    code, _, body = c.request("GET", "/rest/profiles")
    if code != 200:
        r.fail("GET /rest/profiles", f"HTTP {code}")
    else:
        r.ok("GET /rest/profiles")

    # --- files ---
    code, _, body = c.request("GET", "/rest/files")
    if code != 200:
        r.fail("GET /rest/files", f"HTTP {code}")
    else:
        r.ok("GET /rest/files")

    # --- telegram (optional) ---
    code, _, body = c.request("GET", "/rest/telegram/config")
    if code == 200:
        r.ok("GET /rest/telegram/config")
    elif code == 404:
        r.skip("GET /rest/telegram/config", "404 - endpoint not registered")
    else:
        r.fail("GET /rest/telegram/config", f"HTTP {code}")


def main() -> int:
    p = argparse.ArgumentParser(description="SSVC OpenConnect REST smoke tests")
    p.add_argument(
        "base_url",
        nargs="?",
        default=None,
        help="Device base URL, e.g. http://192.168.1.42 (or set SMOKE_BASE_URL)",
    )
    p.add_argument("--user", default=None, help="Username for /rest/signIn (default: env SMOKE_USER or admin)")
    p.add_argument("--password", default=None, help="Password (default: env SMOKE_PASSWORD or admin)")
    p.add_argument("--insecure", action="store_true", help="Skip TLS certificate verification for https://")
    p.add_argument("--timeout", type=float, default=30.0)
    p.add_argument(
        "--skip-unauthorized-check",
        action="store_true",
        help="Do not send GET /rest/settings without token",
    )
    p.add_argument(
        "--self-test",
        action="store_true",
        help="Run against a built-in mock HTTP server (no device required)",
    )
    p.add_argument(
        "--extended",
        action="store_true",
        help="Also run SSVC round-trip PUT, /rest/zones, and start/next/stop commands",
    )
    p.add_argument(
        "--command-timeout",
        type=float,
        default=180.0,
        metavar="SEC",
        help="Timeout per rectification command (start/next/stop); default 180",
    )
    p.add_argument(
        "--ssvc-step-delay",
        type=float,
        default=3.0,
        metavar="SEC",
        help="Pause after POST /rest/commands before polling /rest/telemetry (SSVC processing time); default 3",
    )
    p.add_argument(
        "--telemetry-wait",
        type=float,
        default=12.0,
        metavar="SEC",
        help="With --require-telemetry-progress: max seconds to wait for telemetry snapshot to change",
    )
    p.add_argument(
        "--telemetry-poll-interval",
        type=float,
        default=0.5,
        metavar="SEC",
        help="Poll interval while waiting for telemetry progress; default 0.5",
    )
    p.add_argument(
        "--require-telemetry-progress",
        action="store_true",
        help="Require stage/process/type in /rest/telemetry to change after each start/next/stop",
    )
    args = p.parse_args()

    if args.self_test:
        return _run_self_test()

    import os

    base = args.base_url or os.environ.get("SMOKE_BASE_URL")
    if not base:
        print("Error: pass base_url or set SMOKE_BASE_URL", file=sys.stderr)
        p.print_help()
        return 2

    user = args.user or os.environ.get("SMOKE_USER", "admin")
    password = args.password or os.environ.get("SMOKE_PASSWORD", "admin")

    c = SmokeClient(base=base, insecure=args.insecure, timeout=args.timeout)
    r = SmokeResult()

    print(f"Smoke target: {base}\n")
    args.check_unauthorized = not args.skip_unauthorized_check
    try:
        if not establish_auth(c, user, password, r):
            print("\nAborting after auth failure.")
            return 1
        run_smoke(c, args, r)
        if args.extended:
            run_extended_smoke(c, args, r)
    except urllib.error.URLError as e:
        print(f"\nNetwork error: {e}", file=sys.stderr)
        return 1

    print(
        f"\nDone: {r.passed} passed, {r.failed} failed, {r.skips} skipped",
    )
    return 1 if r.failed else 0


if __name__ == "__main__":
    sys.exit(main())
