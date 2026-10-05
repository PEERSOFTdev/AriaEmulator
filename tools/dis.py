"""Disassembles a program from drive B: (ndisasm, package nasm).

  python dis.py [IMAGE] PATH [--range FROM[:TO]]

PATH as on the Aria, e.g. SYSTEM\\VOICE.SYS. FROM and TO are hexadecimal
offsets in the code: behind the MZ header for EXE, from 100h for COM,
from 0 for SYS. Refuses PKLITE programs, which must be unpacked first
(phase 1), because their disassembly would be compressed data.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ariafw  # noqa: E402


def main(argv):
    span = None
    if '--range' in argv:
        at = argv.index('--range')
        span = argv[at + 1]
        del argv[at:at + 2]
    if len(argv) == 2:
        source, path = argv
    elif len(argv) == 1:
        source, path = None, argv[0]
    else:
        sys.exit(__doc__)
    image, _ = ariafw.load(source)
    volume = ariafw.Fat12(image)
    code, kind = ariafw.code_of(volume.read(path))
    if kind == 'pklite':
        sys.exit('%s is PKLITE-compressed; unpack it first (phase 1)' % path)
    origin = 0x100 if kind == 'com' else 0
    low, high = origin, origin + len(code)
    if span:
        first, _, last = span.partition(':')
        low = int(first, 16)
        high = int(last, 16) if last else high
    for offset, raw, text in ariafw.disassemble(code, origin):
        if low <= offset < high:
            print('%04X  %-16s %s' % (offset, raw, text))


if __name__ == '__main__':
    main(sys.argv[1:])
