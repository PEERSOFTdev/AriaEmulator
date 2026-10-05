"""Lists drive B: of an Aria update, or extracts it to a folder.

  python flashdisk.py [IMAGE]                 list files, label, encoding
  python flashdisk.py [IMAGE] --extract DIR   extract all files into DIR

IMAGE as in ariafw.load(); without it $ARIAFLASH / $ARIAFW is used.
Extracted files are third-party firmware (ROM-NOTICE.txt): DIR must lie
outside the repository, and the tool refuses a DIR inside it.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ariafw  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main(argv):
    extract = None
    if '--extract' in argv:
        at = argv.index('--extract')
        extract = argv[at + 1]
        del argv[at:at + 2]
    image, kind = ariafw.load(argv[0] if argv else None)
    volume = ariafw.Fat12(image)
    encoding = ariafw.detect_encoding(volume)
    print('kind=%s bytes=%d label=%r encoding=%s' % (kind, len(image), volume.label.decode('cp437'),
                                                     encoding or 'unknown'))
    if extract:
        target = os.path.abspath(extract)
        if os.path.commonpath([target, REPO]) == REPO:
            sys.exit('refusing to extract firmware into the repository: ' + target)
    for path, attributes, cluster, size in volume.walk():
        if attributes & 0x10:
            print('%-28s <DIR>' % path)
            continue
        print('%-28s %8d  attr=%02x  cluster=%d' % (path, size, attributes, cluster))
        if extract:
            out = os.path.join(target, *path.split('\\'))
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, 'wb') as file:
                file.write(volume.read(path))


if __name__ == '__main__':
    main(sys.argv[1:])
