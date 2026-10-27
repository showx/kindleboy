"""Fetch pinned build dependencies; never writes to a connected Kindle."""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import io
import json
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def fetch(url):
    for attempt in range(3):
        try:
            request = urllib.request.Request(url, headers={'User-Agent': 'kindleboy-build'})
            with urllib.request.urlopen(request, timeout=120) as response:
                return response.read()
        except OSError:
            if attempt == 2:
                raise
            time.sleep(1)


def source(spec, destination):
    marker = destination / '.kindleboy-revision'
    if marker.exists() and marker.read_text().strip() == spec['revision']:
        print(f'Using {spec["repository"]} at {spec["revision"][:12]}', flush=True)
        return
    data = fetch(f'https://codeload.github.com/{spec["repository"]}/zip/{spec["revision"]}')
    with zipfile.ZipFile(io.BytesIO(data)) as bundle:
        for member in bundle.infolist():
            archive_path = PurePosixPath(member.filename)
            if archive_path.is_absolute() or '..' in archive_path.parts or '\\' in member.filename:
                raise ValueError('Unsafe source archive path')
            parts = archive_path.parts[1:]
            if not parts:
                continue
            path = destination.joinpath(*parts)
            if member.is_dir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(bundle.read(member))
    marker.write_text(spec['revision'] + '\n', encoding='utf-8')
    print(f'Fetched {spec["repository"]}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--with-toolchain', action='store_true')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'dependencies.lock.json').read_text())
    source(lock['fbink'], ROOT / 'vendor/FBInk')
    source(lock['stb'], ROOT / 'vendor/FBInk/stb')
    if args.with_toolchain:
        spec = lock['toolchain']
        path = ROOT / 'downloads/kindlepw2.tar.gz'
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != spec['sha256']:
            print('Fetching kindlepw2 toolchain...', flush=True)
            data = fetch(spec['url'])
            if hashlib.sha256(data).hexdigest() != spec['sha256']:
                raise ValueError('Toolchain checksum mismatch')
            path.write_bytes(data)
        print('Toolchain SHA-256 verified', flush=True)


if __name__ == '__main__':
    main()
