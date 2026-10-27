"""Arm one local Kindle action via the existing WinterBreak2 browser dialog."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('action', choices=('play', 'setup', 'diagnose', 'original'))
parser.add_argument('--drive', default='H:/')
args = parser.parse_args()
device = Path(args.drive)
if not (device / 'system/version.txt').read_text().startswith('Kindle 5.15.1.1 '):
    raise ValueError('Expected the confirmed Kindle firmware')
original = (ROOT / 'dist/jailbreak/winterbreak2/dialoger.html').read_bytes()
target = device / 'winterbreak2/dialoger.html'
state_path = ROOT / '.local/browser-launch.json'
allowed = {hashlib.sha256(original).hexdigest()}
if state_path.exists():
    state = json.loads(state_path.read_text())
    allowed.add(state.get('temporarySha256'))
if hashlib.sha256(target.read_bytes()).hexdigest() not in allowed:
    raise ValueError('Unrecognized dialoger file; refusing to overwrite it')
request = device / 'extensions/kindleboy/browser-action.request'
if args.action == 'original':
    target.write_bytes(original)
    request.unlink(missing_ok=True)
    state = {'mode': 'original_restored', 'restoreOnNextUsbConnection': False}
else:
    backup = ROOT / '.local/browser-launch-backup/dialoger-original.html'
    backup.parent.mkdir(parents=True, exist_ok=True)
    backup.write_bytes(original)
    html = original.decode().replace('\r\r\n', '\n').replace('\r\n', '\n')
    command = "sh -c 'sh /mnt/us/extensions/kindleboy/bin/browser-action.sh'"
    html, count = re.subn(r'(?m)^(\s*)source_command:.*$', lambda m: m.group(1) + 'source_command: ' + json.dumps(command) + ',', html)
    if count != 1:
        raise ValueError('Unexpected official dialoger template')
    javascript = re.search(r'<script>(.*?)</script>', html, re.S).group(1)
    subprocess.run(['node', '--check'], input=javascript, text=True, check=True)
    data = html.encode()
    target.write_bytes(data)
    request.write_text(args.action + '\n', encoding='ascii')
    state = {'mode': 'temporary_' + args.action, 'target': str(target), 'backup': str(backup),
             'originalSha256': hashlib.sha256(original).hexdigest(),
             'temporarySha256': hashlib.sha256(data).hexdigest(), 'restoreOnNextUsbConnection': True}
state_path.parent.mkdir(exist_ok=True)
state_path.write_text(json.dumps(state, indent=2) + '\n')
print('Browser action:', args.action, '(no action has run on the Kindle yet)')
