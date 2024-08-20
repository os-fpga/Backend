#!/usr/bin/env python3
#!/nfs_home/dhow/bin/python3
#!/usr/bin/python3
"""generate map from bitstream.xml"""

import sys
import re

bits = 0
seen = set()
with open(sys.argv[1], encoding='ascii') as ifile:
    for line in ifile:
        line = line.strip()
        if ' path="' in line:
            path = re.sub(r'^.* path="([^"]+)".*$', r'\1', line)
            continue
        if 'bl address' in line:
            data = re.sub(r'^.* address="([^"]+)".*$', r'\1', line)
            x = data.index('1')
            assert 0 <= x
            continue
        if 'wl address' in line:
            data = re.sub(r'^.* address="([^"]+)".*$', r'\1', line)
            y = data.index('1')
            assert 0 <= y
            continue
        if '</bit>' in line:
            if (x, y) in seen:
                sys.stderr.write("Collision\n")
                sys.exit(1)
            seen.add((x, y))
            print(f"{x},{y},{path}")
            bits += 1
    # for
# with
sys.stderr.write(f"Wrote {bits:,d} coordinate mappings\n")


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
