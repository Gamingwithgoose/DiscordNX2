# SPDX-License-Identifier: MIT
"""Adapt a pinned upstream host copy in ignored build output, preserving notices."""
from pathlib import Path
import sys
engine, out = map(Path, sys.argv[1:])
p = out / 'include/lkl_host.h'
s = p.read_text()
if '#elif defined(__SWITCH2__)' not in s:
    old = '#include <sys/uio.h>\n#else\nstruct iovec'
    assert old in s, 'Unexpected LKL host interface'
    s = s.replace(old, '#include <sys/uio.h>\n#elif defined(__SWITCH2__)\n#include <sys/_iovec.h>\n#else\nstruct iovec')
    p.write_text(s)
s = (engine / 'nro/source/nx_host.c').read_text(encoding='utf-8')
old = '\tfree(t);\n\toplog("thread_join<", t);'
assert old in s, 'Unexpected pinned thread join implementation'
s = s.replace(old, '\toplog("thread_join<", t);\n\tfree(t);')
(out / 'host/nx_host.c').write_text(s, encoding='utf-8')
