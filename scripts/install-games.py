"""Install the prepared free GB game pack to a mounted Kindle USB volume."""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--drive', required=True)
args = parser.parse_args()
device = Path(args.drive).resolve()
if not (device / 'system/version.txt').read_text().startswith('Kindle '):
    raise ValueError('The selected path is not a Kindle USB volume')
pack = ROOT / 'dist/free-games'
manifest = json.loads((pack / 'manifest.json').read_text(encoding='utf-8'))
operations = []
for name, expected in manifest['files'].items():
    relative = PurePosixPath(name)
    if relative.is_absolute() or '..' in relative.parts or '\\' in name:
        raise ValueError('Unsafe game pack path')
    source = pack.joinpath(*relative.parts)
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError('Checksum mismatch: ' + name)
    if name.startswith('roms/gb/') and name.endswith('.gb'):
        if len(data) < 32768 or data[0x14D] != (-sum(data[0x134:0x14D]) - 25) & 255:
            raise ValueError('Invalid GB ROM header: ' + name)
        target = device.joinpath(*relative.parts)
    else:
        target = device / 'extensions/kindleboy/games-info' / Path(*relative.parts)
    if target.exists() and target.read_bytes() != data:
        raise ValueError('Existing different file will be left untouched: ' + str(target))
    operations.append((source, target))
for source, target in operations:
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    if hashlib.sha256(target.read_bytes()).digest() != hashlib.sha256(source.read_bytes()).digest():
        raise IOError('Device copy verification failed: ' + str(target))
    if target.suffix.lower() == '.gb':
        print('Installed:', target.name)
print('Game files, attribution, licenses and source archives copied and verified.')
