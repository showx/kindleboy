"""Prepare the official WinterBreak2 v1.1.0 files on the PC only."""
from pathlib import Path
import hashlib
import io
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://github.com/KindleModding/Winterbreak2/releases/download/v1.1.0/wb2.zip'
SHA256 = '9e85970902a1f2af6b4c3243755d80595dd36404b85083f8be8c65a69d04cfdf'
path = ROOT / 'downloads/wb2-v1.1.0.zip'
path.parent.mkdir(parents=True, exist_ok=True)
if not path.exists():
    with urllib.request.urlopen(URL, timeout=30) as response:
        path.write_bytes(response.read())
data = path.read_bytes()
if hashlib.sha256(data).hexdigest() != SHA256:
    raise ValueError('WinterBreak2 archive checksum mismatch')
destination = ROOT / 'dist/jailbreak'
with zipfile.ZipFile(io.BytesIO(data)) as bundle:
    expected = {'winterbreak2/', 'winterbreak2/dialoger.html'}
    if set(bundle.namelist()) != expected:
        raise ValueError('Unexpected WinterBreak2 archive contents')
    (destination / 'winterbreak2').mkdir(parents=True, exist_ok=True)
    (destination / 'winterbreak2/dialoger.html').write_bytes(bundle.read('winterbreak2/dialoger.html'))
print('Prepared official WinterBreak2 files; no Kindle was modified.')
