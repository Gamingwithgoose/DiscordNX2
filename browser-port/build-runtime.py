# SPDX-License-Identifier: MIT
"""Build the optional runtime probe locally. Does not build Chromium or alter the app."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
if os.name != 'nt':
    sys.exit('This entry point uses the Windows devkitPro installation and Debian WSL.')
git = shutil.which('git') or r'C:\Program Files\Git\cmd\git.exe'
engine = root / '.local/chromium-nx'
revision = '87a8095d20cf73b0fde0eee3215e70ebc887f048'
if not engine.exists():
    engine.mkdir(parents=True)
    subprocess.run([git, '-C', str(engine), 'init'], check=True)
    subprocess.run([git, '-C', str(engine), 'remote', 'add', 'origin', 'https://github.com/shodowlo/chromium-nx.git'], check=True)
    subprocess.run([git, '-C', str(engine), 'fetch', '--depth=1', 'origin', revision], check=True)
    subprocess.run([git, '-C', str(engine), 'checkout', '--detach', 'FETCH_HEAD'], check=True)
actual = subprocess.check_output([git, '-C', str(engine), 'rev-parse', 'HEAD'], text=True).strip()
if actual != revision:
    sys.exit(f'Expected Chromium NX {revision}; found {actual}. Not overwriting an existing checkout.')
if not (engine / 'gx/gmem.c').exists():
    subprocess.run([git, '-C', str(engine), 'checkout', '--detach', revision], check=True)
wslroot = subprocess.check_output(['wsl', '-d', 'Debian', '--', 'wslpath', '-a', root.as_posix()], text=True).strip()
subprocess.run(['wsl', '-d', 'Debian', '--', 'bash', wslroot + '/browser-port/build-runtime-wsl.sh'], check=True)
environment = os.environ.copy()
environment['DISCORDNX2_ROOT'] = str(root)
environment['PYTHON'] = str(Path(sys.executable).as_posix())
subprocess.run([r'C:\devkitPro\msys2\usr\bin\bash.exe', '-lc',
    'cd "$(cygpath -u "$DISCORDNX2_ROOT")" && bash browser-port/build-chromium-components.sh'],
    env=environment, check=True)
destination = root / 'dist/browser-runtime'
destination.mkdir(parents=True, exist_ok=True)
shutil.copyfile(root / '.local/browser-engine/chromium-components/DiscordNX2-Runtime-Test.nro',
                destination / 'DiscordNX2-Runtime-Test.nro')
print('Built dist/browser-runtime/DiscordNX2-Runtime-Test.nro. This is a Linux runtime test, not a browser or login fix.')
