# SPDX-License-Identifier: MIT
"""Compile only the native surface against pinned upstream interfaces.
This does not build or package the NetSurf/QuickJS browser engine.
"""
from pathlib import Path
import os,subprocess,urllib.request
r=Path(__file__).resolve().parents[1]
commit='3dabe65c245ab29a22f243e88d407caf2c09b5c2'
dest=r/'.local/browser-port/libnsfb'
for name in ['include/libnsfb.h','include/libnsfb_plot.h','include/libnsfb_event.h','src/nsfb.h','src/surface.h','src/plot.h']:
 p=dest/name;p.parent.mkdir(parents=True,exist_ok=True)
 if not p.exists():p.write_bytes(urllib.request.urlopen(f'https://raw.githubusercontent.com/etonedemid/netsurf-switch/{commit}/libnsfb/{name}',timeout=30).read())
dkp=Path(os.environ.get('DEVKITPRO','C:/devkitPro' if os.name=='nt' else '/opt/devkitpro'))
cc=dkp/'devkitA64/bin'/('aarch64-none-elf-gcc.exe' if os.name=='nt' else 'aarch64-none-elf-gcc')
args=[str(cc),'-mcpu=cortex-a78c+crypto','-mtp=soft','-fPIE','-D__SWITCH__','-D__SWITCH2__','-std=gnu11','-Wall','-Wextra','-Werror',f'-I{dkp}/libounce/include',f'-I{dest}/include',f'-I{dest}/src','-c',str(r/'browser-port/switch2_surface.c'),'-o',str(dest.parent/'switch2_surface.o')]
subprocess.run(args,check=True)
print('PASS: original Switch 2 framebuffer/input backend cross-compiles against pinned libnsfb headers')
print('Not a complete browser build; HTML/JS/TLS integration and hCaptcha acceptance remain unverified.')
