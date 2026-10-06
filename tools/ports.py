"""I/O port map of the programs on drive B:.

  python ports.py [IMAGE]

Counts `in`/`out` with an immediate port and `in`/`out` through DX right
after `mov dx,imm16` (within four instructions), per program. PKLITE
programs are listed but not scanned: their code is compressed and a scan
is noise until phase 1 unpacks them (HANDOFF 6.1). Offsets are relative
to the code, i.e. behind the MZ header; COM files start at 100h.
"""

import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ariafw  # noqa: E402

MOV_DX = re.compile(r'mov dx,0x([0-9a-f]+)$')
IMMEDIATE = re.compile(r'(in|out) (?:(?:al|ax),0x([0-9a-f]+)|0x([0-9a-f]+),(?:al|ax))$')
VIA_DX = re.compile(r'(in (?:al|ax),dx|out dx,(?:al|ax))$')


def scan(code, origin):
    hits = []
    last_dx = None
    for index, (offset, _, text) in enumerate(ariafw.disassemble(code, origin)):
        match = MOV_DX.match(text)
        if match:
            last_dx = (int(match.group(1), 16), index)
            continue
        match = IMMEDIATE.match(text)
        if match:
            hits.append((match.group(1), int(match.group(2) or match.group(3), 16), offset, 'imm'))
            continue
        match = VIA_DX.match(text)
        if match and last_dx is not None and index - last_dx[1] <= 4:
            hits.append((text.split()[0], last_dx[0], offset, 'dx'))
    return hits


def main(argv):
    image, _ = ariafw.load(argv[0] if argv else None)
    volume = ariafw.Fat12(image)
    by_port = collections.defaultdict(list)
    for path in ariafw.programs(volume):
        code, kind = ariafw.code_of(ariafw.program_bytes(volume, path))
        if kind == 'pklite':
            print('%-22s PKLITE, skipped' % path)
            continue
        origin = 0x100 if kind == 'com' else 0
        hits = scan(code, origin)
        print('%-22s %-4s %d hits' % (path, kind, len(hits)))
        for direction, port, offset, how in hits:
            by_port[port].append((direction, path.rsplit('\\', 1)[-1], offset, how))
    print()
    for port in sorted(by_port):
        hits = by_port[port]
        summary = collections.Counter('%s %s' % (d, f) for d, f, _, _ in hits)
        where = ', '.join('%s+%04X' % (f, o) for _, f, o, _ in hits[:6])
        print('%04Xh  %s  [%s]' % (port, ', '.join('%s x%d' % kv for kv in sorted(summary.items())), where))


if __name__ == '__main__':
    main(sys.argv[1:])
