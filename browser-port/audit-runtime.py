# SPDX-License-Identifier: MIT
"""Compatibility gate, not an assertion that a browser can pass CAPTCHA."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1]
runtime=r/'.local/browser-engine/src/netsurf/content/handlers/javascript/quickjs/runtime.js'
if not runtime.exists():sys.exit('Prepare source first.')
s=runtime.read_text();issues=[]
crypto=s[s.index('global.crypto ='):s.index('global.structuredClone')]
if 'Math.random' in crypto:issues.append('getRandomValues uses Math.random rather than a native CSPRNG')
if '/* postMessage to self */' in s:issues.append('postMessage only targets the same page; cross-frame challenge messaging is not implemented here')
if 'Web Workers are not supported' in s:issues.append('Web Workers are explicitly unsupported')
for issue in issues:print('BLOCKED:',issue)
if issues:sys.exit(2)
print('No known source-pattern blockers found; this is NOT evidence of CAPTCHA compatibility.')
