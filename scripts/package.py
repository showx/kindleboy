"""Package the KT3 binary, scriptlets, demo cartridge, licenses and source."""
from pathlib import Path
import hashlib
import json
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / 'dist'


def main():
    binary = (ROOT / 'build/kindleboy').read_bytes()
    if binary[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', binary, 18)[0] != 40:
        raise ValueError('Expected a 32-bit little-endian ARM ELF executable')
    if struct.unpack_from('<I', binary, 36)[0] & 0x400:
        raise ValueError('The KT3 package must use the soft-float ABI')
    phoff = struct.unpack_from('<I', binary, 28)[0]
    phsize, phnum = struct.unpack_from('<HH', binary, 42)
    if any(struct.unpack_from('<I', binary, phoff + n * phsize)[0] == 3 for n in range(phnum)):
        raise ValueError('Expected a static binary without a dynamic interpreter')
    DIST.mkdir(exist_ok=True)
    files = {}
    for path in (ROOT / 'kual/kindleboy').rglob('*'):
        if path.is_file():
            files['extensions/kindleboy/' + path.relative_to(ROOT / 'kual/kindleboy').as_posix()] = path.read_bytes()
    files['extensions/kindleboy/bin/kindleboy'] = binary
    for path in (ROOT / 'launchers').glob('*.sh'):
        files['documents/' + path.name] = path.read_bytes()
    files['roms/gb/KindleBoy-Demo.gb'] = (ROOT / 'build/demo/KindleBoy-Demo.gb').read_bytes()
    for name, path in {
        'LICENSE': 'LICENSE', 'NOTICE.md': 'NOTICE.md', 'UPSTREAM.md': 'docs/UPSTREAM.md',
        'Walnut-CGB-LICENSE': 'vendor/walnut_cgb/LICENSE',
        'Peanut-GB-LICENSE': 'vendor/peanut_gb/LICENSE',
        'minigb-apu-LICENSE': 'vendor/minigb_apu/LICENSE',
        'FBInk-LICENSE': 'vendor/FBInk/LICENSE',
        'FBInk-CREDITS': 'vendor/FBInk/CREDITS',
        'stb-LICENSE': 'vendor/FBInk/stb/LICENSE',
    }.items():
        files['extensions/kindleboy/licenses/' + name] = (ROOT / path).read_bytes()
    files['INSTALL.zh-CN.md'] = (ROOT / 'docs/INSTALL_KT3.zh-CN.md').read_bytes()
    for name in list(files):
        if name.endswith(('.sh', '.conf')):
            files[name] = files[name].replace(b'\r\n', b'\n')
    manifest = {
        'model': 'KT3 (Kindle 8th Generation, 2016)',
        'firmware': '5.15.1.1', 'abi': 'arm-kindlepw2-linux-gnueabi (static)',
        'hardwareValidation': 'KT3 normal mode, A/B, state save/load and exit verified by user and logs; Setup SUCCESS and dedicated autoboot confirmed by user',
        'files': {name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())},
        'dependencies': json.loads((ROOT / 'dependencies.lock.json').read_text()),
    }
    files['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    staging = DIST / 'staging'
    staging.mkdir(exist_ok=True)
    for name, data in files.items():
        path = staging / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    package = DIST / 'kindleboy-kt3-5.15.1.1.zip'
    with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as bundle:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name)
            info.external_attr = (0o100755 if name.endswith('.sh') or name.endswith('/bin/kindleboy') else 0o100644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            bundle.writestr(info, data)
    source = DIST / 'kindleboy-source.zip'
    with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED) as bundle:
        for name in ('src', 'vendor', 'kual', 'launchers', 'test', 'scripts', 'docs', 'games'):
            for path in (ROOT / name).rglob('*'):
                if not path.is_file() or path.suffix in ('.o', '.a', '.pyc', '.pgm'):
                    continue
                relative = path.relative_to(ROOT)
                if any(part in ('__pycache__', '.git', 'Release', 'Debug', 'libi2c-staged') for part in relative.parts):
                    continue
                bundle.write(path, 'kindleboy/' + relative.as_posix())
        for name in ('Makefile', 'LICENSE', 'NOTICE.md', 'README.md', 'dependencies.lock.json', 'Dockerfile.build', '.gitattributes', '.gitignore', '.dockerignore'):
            bundle.write(ROOT / name, 'kindleboy/' + name)
    digest = hashlib.sha256(package.read_bytes()).hexdigest()
    (DIST / (package.name + '.sha256')).write_text(f'{digest}  {package.name}\n')
    print(f'Created {package} ({package.stat().st_size} bytes)')
    print(f'Created {source} (corresponding source and third-party notices)')


if __name__ == '__main__':
    main()
