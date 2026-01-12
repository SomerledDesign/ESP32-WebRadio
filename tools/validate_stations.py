#!/usr/bin/env python3
import argparse
import json
import socket
import time
from pathlib import Path
from urllib.parse import urlparse

DEFAULT_TIMEOUT = 6
MAX_READ = 2048
MAX_REDIRECTS = 2

def build_url(entry):
    host = entry["host"]
    path = entry["path"]
    port = int(entry.get("port", 80))
    if port == 80:
        return f"http://{host}{path}"
    return f"http://{host}:{port}{path}"

def fetch_headers(url, timeout):
    parsed = urlparse(url)
    host = parsed.hostname
    port = parsed.port or 80
    path = parsed.path or "/"
    if parsed.query:
        path = f"{path}?{parsed.query}"

    with socket.create_connection((host, port), timeout=timeout) as sock:
        req = (
            f"GET {path} HTTP/1.0\r\n"
            f"Host: {host}\r\n"
            "User-Agent: ESP32-WebRadio Validator\r\n"
            "Icy-MetaData: 1\r\n"
            "Connection: close\r\n\r\n"
        )
        sock.sendall(req.encode("ascii", "ignore"))
        sock.settimeout(timeout)
        data = sock.recv(MAX_READ)

    if not data:
        return None, None, None

    text = data.decode("iso-8859-1", "ignore")
    lines = text.split("\r\n")
    status_line = lines[0] if lines else ""

    status = None
    if status_line.startswith("ICY "):
        status = 200
    elif status_line.startswith("HTTP/"):
        parts = status_line.split()
        if len(parts) >= 2 and parts[1].isdigit():
            status = int(parts[1])

    headers = {}
    for line in lines[1:]:
        if not line:
            break
        if ":" in line:
            key, val = line.split(":", 1)
            headers[key.strip().lower()] = val.strip()

    return status, headers, status_line

def validate_url(url, timeout):
    current_url = url
    for _ in range(MAX_REDIRECTS + 1):
        status, headers, status_line = fetch_headers(current_url, timeout)
        if status is None:
            return {"ok": False, "error": "no response", "url": current_url}
        if status in (301, 302, 303, 307, 308):
            location = headers.get("location")
            if not location:
                return {"ok": False, "status": status, "error": "redirect without location", "url": current_url}
            current_url = location
            continue
        ok = status in (200, 206)
        return {
            "ok": ok,
            "status": status,
            "url": current_url,
            "content_type": headers.get("content-type"),
            "icy_genre": headers.get("icy-genre"),
            "icy_name": headers.get("icy-name"),
            "icy_br": headers.get("icy-br"),
            "icy_metaint": headers.get("icy-metaint"),
            "status_line": status_line,
        }

    return {"ok": False, "error": "too many redirects", "url": current_url}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="stations.json")
    parser.add_argument("--report", default="stations_report.json")
    parser.add_argument("--output")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT)
    parser.add_argument("--update-genre", action="store_true")
    args = parser.parse_args()

    input_path = Path(args.input)
    stations = json.loads(input_path.read_text())
    results = []
    kept = []

    for entry in stations:
        url = build_url(entry)
        result = validate_url(url, args.timeout)
        result["friendlyName"] = entry.get("friendlyName")
        results.append(result)

        if result.get("ok"):
            kept.append(entry)

        if args.update_genre:
            if entry.get("genre") in (None, "", "Unknown") and result.get("icy_genre"):
                entry["genre"] = result["icy_genre"]

        entry["lastChecked"] = int(time.time())
        entry["lastStatus"] = result.get("status")
        entry["lastOk"] = bool(result.get("ok"))

    Path(args.report).write_text(json.dumps(results, indent=2, ensure_ascii=True))

    if args.output:
        Path(args.output).write_text(json.dumps(kept, indent=2, ensure_ascii=True))

    input_path.write_text(json.dumps(stations, indent=2, ensure_ascii=True))

    ok_count = sum(1 for r in results if r.get("ok"))
    print(f"Validated {len(results)} stations, ok={ok_count}")

if __name__ == "__main__":
    main()
