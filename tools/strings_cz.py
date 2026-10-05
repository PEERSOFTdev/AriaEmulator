"""Strings of a file on drive B:, decoded in the image's own encoding.

  python strings_cz.py [IMAGE] PATH [MIN] [--encoding kamenicky|cp852]

PATH as on the Aria, e.g. SYSTEM\\MESSAGES.DAT. The encoding is detected
from the image like the emulator does (6.87 Kamenicky, 6.99 and 7.05
CP852, HANDOFF 1); --encoding overrides it. PKLITE programs give mostly
noise until phase 1 unpacks them. Offsets are file offsets.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ariafw  # noqa: E402


def main(argv):
    encoding = None
    if '--encoding' in argv:
        at = argv.index('--encoding')
        encoding = argv[at + 1]
        del argv[at:at + 2]
    numbers = [a for a in argv if a.isdigit()]
    argv = [a for a in argv if not a.isdigit()]
    minimum = int(numbers[0]) if numbers else 5
    if len(argv) == 2:
        source, path = argv
    elif len(argv) == 1:
        source, path = None, argv[0]
    else:
        sys.exit(__doc__)
    image, _ = ariafw.load(source)
    volume = ariafw.Fat12(image)
    encoding = encoding or ariafw.detect_encoding(volume) or 'cp852'
    data = volume.read(path)

    def printable(byte):
        return 0x20 <= byte < 0x7F or byte >= 0x80 and byte not in (0xFF,)

    start = None
    for offset, byte in enumerate(data + b'\0'):
        if printable(byte):
            if start is None:
                start = offset
            continue
        if start is not None and offset - start >= minimum:
            print('%06X  %s' % (start, ariafw.decode(data[start:offset], encoding)))
        start = None


if __name__ == '__main__':
    main(sys.argv[1:])
