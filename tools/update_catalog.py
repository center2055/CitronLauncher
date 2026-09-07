#!/usr/bin/env python3
"""rebuilds catalog/versions.json from a version index and the package cdn.

usage: update_catalog.py <index-url-or-file> [output]

the index is a json document with releaseVersions and previewVersions lists,
each entry carrying a version label, mirror urls, an md5 and a timestamp.
package sizes are read from the cdn with head requests.
"""

import json
import sys
import time
import urllib.parse
import urllib.request

HOSTS = [
    "assets1.xboxlive.com",
    "assets2.xboxlive.com",
    "xvcf1.xboxlive.com",
    "xvcf2.xboxlive.com",
    "d1.xboxlive.com",
    "d2.xboxlive.com",
]


def load_index(source):
    if source.startswith("http://") or source.startswith("https://"):
        with urllib.request.urlopen(source, timeout=30) as r:
            return json.load(r)
    with open(source, encoding="utf-8") as f:
        return json.load(f)


def head_size(url):
    req = urllib.request.Request(url, method="HEAD")
    for _ in range(3):
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                length = r.headers.get("Content-Length")
                return int(length) if length else 0
        except Exception:
            time.sleep(1)
    return 0


def convert(index, previous):
    known = {(v["version"], v["channel"]): v for v in previous.get("versions", [])}
    out = []
    for key, channel in (("releaseVersions", "release"), ("previewVersions", "preview")):
        for entry in index.get(key, []):
            label = entry["version"].split()
            version = label[-1]
            urls = entry.get("urls", [])
            if not urls:
                continue
            parsed = urllib.parse.urlparse(urls[0])
            path = parsed.path
            old = known.get((version, channel))
            size = old["size"] if old and old.get("path") == path and old.get("size") else 0
            if not size:
                size = head_size("http://" + HOSTS[0] + path)
                print(f"{channel} {version} {size}", file=sys.stderr)
            out.append({
                "version": version,
                "channel": channel,
                "released": int(entry.get("timestamp", 0)),
                "size": size,
                "md5": entry.get("md5", "").lower(),
                "path": path,
            })
    out.sort(key=lambda v: ([int(p) for p in v["version"].split(".")], v["channel"]), reverse=True)
    return {"format": 1, "updated": int(time.time()), "hosts": HOSTS, "versions": out}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    output = sys.argv[2] if len(sys.argv) > 2 else "catalog/versions.json"
    try:
        with open(output, encoding="utf-8") as f:
            previous = json.load(f)
    except (OSError, ValueError):
        previous = {}
    catalog = convert(load_index(sys.argv[1]), previous)
    with open(output, "w", encoding="utf-8", newline="\n") as f:
        json.dump(catalog, f, indent=2)
        f.write("\n")
    print(f"{len(catalog['versions'])} versions written to {output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
