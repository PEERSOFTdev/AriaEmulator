"""Shows and checks the headers of a serial update (ARIAA4.1-3).

  python ariaa4.py SERIAL_ZIP_OR_FOLDER

Each part is a 256-byte header ("ARIA", part number, payload length,
CRC-32 of the payload) followed by the payload; the payloads join into
the whole 2 MB flash (HANDOFF 1).
"""

import os
import sys
import zipfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ariafw  # noqa: E402


def parts_of(path):
    path = ariafw._native(path)
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            return [(n, archive.read(n)) for n in sorted(archive.namelist())
                    if n.upper().startswith('ARIAA4.')]
    folder = path if os.path.isdir(path) else os.path.dirname(path)
    return [(n, open(os.path.join(folder, n), 'rb').read()) for n in sorted(os.listdir(folder))
            if n.upper().startswith('ARIAA4.')]


def main(argv):
    if not argv:
        sys.exit(__doc__)
    parts = parts_of(argv[0])
    for name, data in parts:
        header = ariafw.serial_header(data)
        if header is None:
            print('%s: no ARIA header' % name)
            continue
        number, length, crc = header
        payload = data[ariafw.SERIAL_HEADER:]
        padding_ok = not any(data[14:ariafw.SERIAL_HEADER])
        print('%s: part=%d length=%06Xh (%s) crc=%08X (%s) padding=%s erased_tail=%d' % (
            name, number, length, 'ok' if len(payload) == length else 'MISMATCH %d' % len(payload),
            crc, 'ok' if zlib.crc32(payload) == crc else 'MISMATCH',
            'zero' if padding_ok else 'NONZERO',
            len(payload) - len(payload.rstrip(b'\xff'))))
    try:
        flash = ariafw.join_serial([data for _, data in parts])
        print('joined: %d bytes, used up to %06Xh' % (len(flash), len(flash.rstrip(b'\xff'))))
    except ValueError as error:
        print('join failed: %s' % error)


if __name__ == '__main__':
    main(sys.argv[1:])
