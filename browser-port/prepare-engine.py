# SPDX-License-Identifier: MIT
"""Prepare pinned browser sources without executing upstream setup scripts."""
from pathlib import Path,PurePosixPath
import io,tarfile,urllib.request,shutil
r=Path(__file__).resolve().parents[1]
commit='3dabe65c245ab29a22f243e88d407caf2c09b5c2'
dest=r/'.local/browser-engine/src'
if dest.exists():
 print('Reusing existing source directory; not overwriting local port edits.')
else:
 data=urllib.request.urlopen(f'https://codeload.github.com/etonedemid/netsurf-switch/tar.gz/{commit}',timeout=60).read()
 with tarfile.open(fileobj=io.BytesIO(data),mode='r:gz') as archive:
  for item in archive:
   parts=PurePosixPath(item.name).parts[1:]
   if not item.isfile() or not parts or any(p in ('..','test','tests','dist','switch-dist','docs','.github') or ':' in p for p in parts):continue
   path=Path(*parts)
   if path.is_absolute() or path.suffix in ('.nro','.nso','.o','.a','.elf','.exe'):continue
   target=dest/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(archive.extractfile(item).read())
shutil.copyfile(r/'browser-port/switch2_surface.c',dest/'libnsfb/src/surface/switch2.c')
(dest/'libnsfb/src/surface/Makefile').write_text('SURFACE_HANDLER_yes := surface.c switch2.c\nDIR_SOURCES := $(SURFACE_HANDLER_yes)\ninclude $(NSBUILD)/Makefile.subdir\n')
p=dest/'libnsfb/Makefile';s=p.read_text()
if '# DiscordNX2 native surface' not in s:
 i=s.index('# Flags and setup')
 s=s[:i]+'# DiscordNX2 native surface\n'+''.join(f'override NSFB_{x}_AVAILABLE := no\n' for x in ['SDL','SDL2','XCB','VNC','WLD'])+s[i:]
 s+='\n# libounce headers require C11\nCFLAGS += -std=gnu11\n';p.write_text(s)
print('Prepared native surface source for component builds. No browser binary was produced.')
