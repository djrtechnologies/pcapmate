# SPDX-License-Identifier: GPL-2.0-only
"""Generate the userspace subset; upstream remains unmodified and pinned."""
from pathlib import Path
import re
project=Path(__file__).resolve().parents[1]
root=project/'Sources/Hardware'
src=project/'upstream/cxd28xx'
for name in ['cxd2878.h','cxd2878_priv.h']:
 s=(src/name).read_text()
 s=re.sub(r'^#include <[^>]+>\n','',s,flags=re.M)
 (root/name).write_text(s)
s=(src/'cxd2878.c').read_text()
s=s[:s.index('/* ── ALP-div-TS decapsulation')]
s=re.sub(r'^#include .*\n','',s,flags=re.M)
a=s.index('static const struct dvb_frontend_ops cxd2878_ops =')
b=s.index('static struct cxd_base *match_base',a)
s=s[:a]+s[b:]
s=s.replace('int ret,waittime;', 'int ret = 0, waittime = 0;')
s=s.replace('rdata[0]&0x3F!=0x30', '(rdata[0]&0x3F)!=0x30')
s=s.replace('memcpy(&dev->fe.ops,&cxd2878_ops,sizeof(struct dvb_frontend_ops));','')
s=re.sub(r'EXPORT_SYMBOL_GPL\([^;]+;','',s)
# Initialize status scratch values; failed reads are not valid measurements.
s=s.replace('s32  rflevel,snr=0,rflevel_dBm=0;', 's32 rflevel=-10000,snr=0,rflevel_dBm=0;')
(root/'demod.inc').write_text(s)
s=(src/'it930x.c').read_text().splitlines(True)
# Bridge constants, GPIO table, board definitions, command/I2C/init functions.
parts=[s[25:150],s[242:785],s[797:1120]]
(root/'bridge.inc').write_text(''.join(s[:6]) + '\n' + ''.join(''.join(p) for p in parts))

# Prominent modification notice on every generated adaptation (GPLv2 section 2a).
notice = '// SPDX-License-Identifier: GPL-2.0-only\n// Userspace adaptation modified by Dylan Ross, 2026-11-15; original notices retained below.\n'
for name in ['bridge.inc','demod.inc','cxd2878.h','cxd2878_priv.h']:
 p = root/name
 p.write_text(notice+p.read_text())
