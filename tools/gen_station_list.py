#!/usr/bin/env python3
import json
import re
import unicodedata
from pathlib import Path

try:
    Import("env")
except Exception:
    env = None


def resolve_project_dir():
    if env is not None:
        try:
            return Path(env.subst("$PROJECT_DIR"))
        except Exception:
            pass
    try:
        return Path(__file__).resolve().parents[1]
    except NameError:
        return Path.cwd()


PROJECT_DIR = resolve_project_dir()
STATIONS_JSON = PROJECT_DIR / "stations.json"
STATION_LIST = PROJECT_DIR / "include" / "stationList.h"
STATION_GENRES = PROJECT_DIR / "include" / "stationGenres.h"

MAX_HOST = 63
MAX_PATH = 127
MAX_NAME = 63
MAX_GENRE = 31

def sanitize_ascii(value, max_len, default):
    if value is None:
        value = ""
    value = str(value)
    value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode("ascii")
    value = value.replace("\"", "''")
    value = re.sub(r"\s+", " ", value).strip()
    if not value:
        value = default
    if max_len:
        value = value[:max_len].rstrip()
    return value

def main():
    data = json.loads(STATIONS_JSON.read_text())
    if not isinstance(data, list):
        raise SystemExit("stations.json must contain a list")

    if len(data) != 64:
        raise SystemExit(f"stations.json must contain 64 entries, found {len(data)}")

    list_lines = []
    genre_lines = []

    for idx, entry in enumerate(data):
        host = sanitize_ascii(entry.get("host"), MAX_HOST, "localhost")
        path = sanitize_ascii(entry.get("path"), MAX_PATH, "/")
        name = sanitize_ascii(entry.get("friendlyName"), MAX_NAME, "Station")
        genre = sanitize_ascii(entry.get("genre"), MAX_GENRE, "Unknown")
        port = int(entry.get("port", 80))
        use_meta = int(entry.get("useMetaData", 0))

        list_lines.append(f"\t// {idx}")
        list_lines.append(f"\t\"{host}\",")
        list_lines.append(f"\t\"{path}\",")
        list_lines.append(f"\t{port},")
        list_lines.append(f"\t\"{name}\",")
        list_lines.append(f"\t{use_meta},")
        list_lines.append(f"\t\"{genre}\",")
        list_lines.append("")

        genre_lines.append(f"    {{{idx}, \"{genre}\"}},")

    STATION_LIST.write_text("\n".join(list_lines).rstrip() + "\n")

    station_genres = [
        "#pragma once",
        "",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "",
        "struct StationGenreIndex {",
        "    uint8_t stationIndex;",
        "    const char *genre;",
        "};",
        "",
        "static const StationGenreIndex kStationGenres[] = {",
    ] + genre_lines + [
        "};",
        "",
        "static const size_t kStationGenreCount = sizeof(kStationGenres) / sizeof(kStationGenres[0]);",
        "",
    ]

    STATION_GENRES.write_text("\n".join(station_genres))

if __name__ == "__main__":
    main()
