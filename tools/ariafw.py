"""Shared helpers for the read-only firmware analysis tools.

The B: image is chosen exactly like the emulator chooses it (src/main.cpp,
src/flash_image.cpp): an explicit path, else $ARIAFLASH, else
CARD/CC0705.ZIP under $ARIAFW (default D:\\aria\\update). Accepted forms are
a card or serial update ZIP, FLASHDSK.IMG, one of ARIAA4.1-3, or a folder
holding either. Nothing here writes into the repository.
"""

import os
import struct
import zipfile
import zlib

FLASH_SIZE = 2 * 1024 * 1024
SERIAL_HEADER = 256


def _native(path):
    # Windows-style paths also work from WSL, where D:\ is /mnt/d.
    if os.name != 'nt' and len(path) > 2 and path[1] == ':' and path[2] in '\\/':
        return '/mnt/' + path[0].lower() + path[2:].replace('\\', '/')
    return path


def default_path():
    path = os.environ.get('ARIAFLASH')
    if path:
        return _native(path)
    folder = os.environ.get('ARIAFW', r'D:\aria\update')
    return os.path.join(_native(folder), 'CARD', 'CC0705.ZIP')


def serial_header(part):
    """(number, length, crc) of an ARIAA4.n part, or None (HANDOFF 1)."""
    if len(part) < SERIAL_HEADER or part[:4] != b'ARIA':
        return None
    number, length, crc = struct.unpack_from('<HII', part, 4)
    return number, length, crc


def join_serial(parts):
    """Joins ARIAA4.n parts, checking numbering, length, CRC-32 and 2 MB."""
    ordered = {}
    for part in parts:
        header = serial_header(part)
        if header is None:
            raise ValueError('part without ARIA header')
        number, length, crc = header
        payload = part[SERIAL_HEADER:]
        if len(payload) != length:
            raise ValueError('part %d: length %d, header says %d' % (number, len(payload), length))
        if zlib.crc32(payload) != crc:
            raise ValueError('part %d: CRC-32 mismatch' % number)
        ordered[number] = payload
    if sorted(ordered) != list(range(1, len(ordered) + 1)):
        raise ValueError('parts not numbered 1..n: %s' % sorted(ordered))
    flash = b''.join(ordered[n] for n in sorted(ordered))
    if len(flash) != FLASH_SIZE:
        raise ValueError('parts give %d bytes, not the 2 MB flash' % len(flash))
    return flash


def _serial_folder(folder):
    parts = []
    for number in range(1, 10):
        path = os.path.join(folder, 'ARIAA4.%d' % number)
        if not os.path.exists(path):
            break
        parts.append(open(path, 'rb').read())
    return parts


def load(path=None):
    """Returns (image bytes, kind) with kind 'card' or 'serial'."""
    path = _native(path) if path else default_path()
    if os.path.isdir(path):
        image = os.path.join(path, 'FLASHDSK.IMG')
        if os.path.exists(image):
            return open(image, 'rb').read(), 'card'
        return join_serial(_serial_folder(path)), 'serial'
    name = os.path.basename(path).upper()
    if name.startswith('ARIAA4.'):
        return join_serial(_serial_folder(os.path.dirname(path) or '.')), 'serial'
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            names = {n.upper(): n for n in archive.namelist()}
            if 'FLASHDSK.IMG' in names:
                return archive.read(names['FLASHDSK.IMG']), 'card'
            parts = [archive.read(n) for u, n in names.items() if u.startswith('ARIAA4.')]
            return join_serial(parts), 'serial'
    return open(path, 'rb').read(), 'card'


class Fat12:
    """Read-only FAT12 with geometry from the BPB (Datalight DLRDISK: one
    FAT, 16 root entries), mirroring src/fat12.cpp."""

    def __init__(self, image):
        self.image = image
        (self.bps, self.spc, reserved, fats, self.root_entries, total,
         _media, spf) = struct.unpack_from('<HBHBHHBH', image, 11)
        self.fat = reserved * self.bps
        self.root = self.fat + fats * spf * self.bps
        self.data = self.root + self.root_entries * 32
        self.total = total
        self.label = b''
        for entry in self._raw(image[self.root:self.data]):
            if entry[11] == 0x08:
                self.label = entry[:11].rstrip(b' ')

    @staticmethod
    def _raw(data):
        for at in range(0, len(data) - 31, 32):
            entry = data[at:at + 32]
            if entry[0] == 0:
                return
            if entry[0] != 0xE5 and entry[11] != 0x0F:
                yield entry

    def _next(self, cluster):
        value = struct.unpack_from('<H', self.image, self.fat + cluster * 3 // 2)[0]
        return value >> 4 if cluster & 1 else value & 0xFFF

    def chain(self, cluster):
        seen = set()
        while 2 <= cluster < 0xFF0 and cluster not in seen:
            seen.add(cluster)
            yield cluster
            cluster = self._next(cluster)

    def cluster_offset(self, cluster):
        return self.data + (cluster - 2) * self.spc * self.bps

    def _read(self, cluster, size=None):
        size_cluster = self.spc * self.bps
        data = b''.join(self.image[self.cluster_offset(c):self.cluster_offset(c) + size_cluster]
                        for c in self.chain(cluster))
        return data if size is None else data[:size]

    def entries(self, directory=b''):
        """Yields (name, attributes, first cluster, size) of a directory."""
        if directory:
            data = self._read(directory)
        else:
            data = self.image[self.root:self.data]
        for entry in self._raw(data):
            if entry[11] & 0x08:
                continue
            name = entry[:8].rstrip(b' ').decode('cp437')
            ext = entry[8:11].rstrip(b' ').decode('cp437')
            if name in ('.', '..'):
                continue
            cluster, size = struct.unpack_from('<HI', entry, 26)
            yield (name + '.' + ext if ext else name), entry[11], cluster, size

    def walk(self, cluster=b'', prefix=''):
        """Yields (path, attributes, first cluster, size) recursively."""
        for name, attributes, first, size in self.entries(cluster):
            path = prefix + name
            yield path, attributes, first, size
            if attributes & 0x10:
                yield from self.walk(first, path + '\\')

    def read(self, path):
        for name, attributes, first, size in self.walk():
            if name.upper() == path.upper() and not attributes & 0x10:
                return self._read(first, size)
        raise FileNotFoundError(path)


# Encoding detection, the same letters and thresholds as
# src/flash_image.cpp (measured in HANDOFF 1).
_KAMENICKY = b'\x87\x88\xa9\xa8\x91\x96\x98'
_CP852 = b'\x9f\xd8\xfd\xe7\xa7\x85\xec'

KAMENICKY_HIGH = ('ČüéďäĎŤčěĚĹÍľĺÄÁÉžŽôöÓůÚýÖÜŠĽÝŘť'
                  'áíóúňŇŮÔšřŕŔ¼§«»'
                  '░▒▓│┤╡╢╖╕╣║╗╝╜╛┐'
                  '└┴┬├─┼╞╟╚╔╩╦╠═╬╧'
                  '╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀'
                  'αßΓπΣσµτΦΘΩδ∞φε∩'
                  '≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ')


def detect_encoding(volume):
    try:
        text = volume.read('SYSTEM\\MESSAGES.DAT')
    except FileNotFoundError:
        return None
    kamenicky = sum(text.count(bytes([b])) for b in _KAMENICKY)
    cp852 = sum(text.count(bytes([b])) for b in _CP852)
    if kamenicky >= 100 and kamenicky >= 4 * cp852:
        return 'kamenicky'
    if cp852 >= 100 and cp852 >= 4 * kamenicky:
        return 'cp852'
    return None


def decode(data, encoding):
    if encoding == 'kamenicky':
        return ''.join(chr(b) if b < 0x80 else KAMENICKY_HIGH[b - 0x80] for b in data)
    return data.decode('cp852' if encoding == 'cp852' else 'cp437')


def boot_order(volume):
    """Programs in the order the Aria starts them: CONFIG.SYS device=
    lines, then AUTOEXEC.BAT commands (HANDOFF 2). Returns file names as
    they appear on B:, e.g. 'SYSTEM\\VOICE.SYS'."""
    order = []
    known = {path.upper(): path for path, attributes, _, _ in volume.walk() if not attributes & 0x10}

    def add(command):
        word = command.split()[0].lower().replace('b:\\', '').replace('%sysdir%\\', 'system\\')
        candidates = [word, 'system\\' + word]
        if '.' not in word.rsplit('\\', 1)[-1]:
            candidates = [c + ext for c in candidates for ext in ('.com', '.exe')]
        for candidate in candidates:
            if candidate.upper() in known and known[candidate.upper()] not in order:
                order.append(known[candidate.upper()])
                return

    for line in volume.read('CONFIG.SYS').decode('cp437').splitlines():
        line = line.strip()
        if line.lower().startswith('device') and '=' in line:
            add(line.split('=', 1)[1].strip())
    for line in volume.read('AUTOEXEC.BAT').decode('cp437').splitlines():
        line = line.strip()
        first = line.split()[0].lower() if line else ''
        if not first or first.startswith(':') or first in ('rem', 'set', 'if', 'cd', 'prompt', 'path', 'b:'):
            continue
        add(line)
    return order


def disassemble(code, origin=0):
    """Linear sweep with ndisasm (package nasm), as (offset, hex, text).

    A linear sweep also decodes data, so callers report offsets for
    checking by hand rather than treating every hit as code."""
    import shutil
    import subprocess
    import tempfile
    if shutil.which('ndisasm') is None:
        raise SystemExit('ndisasm not found; on Ubuntu/WSL it comes with the nasm package')
    with tempfile.NamedTemporaryFile(delete=False) as file:
        file.write(code)
        name = file.name
    try:
        output = subprocess.run(['ndisasm', '-b16', '-o', str(origin), name],
                                capture_output=True, text=True, check=True).stdout
    finally:
        os.unlink(name)
    result = []
    for line in output.splitlines():
        fields = line.split(None, 2)
        if len(fields) == 3 and not line.startswith(' '):
            result.append((int(fields[0], 16), fields[1], fields[2]))
    return result


def programs(volume):
    """Every executable on B:, boot order first (HANDOFF 2)."""
    order = boot_order(volume)
    rest = sorted(path for path, attributes, _, _ in volume.walk()
                  if not attributes & 0x10 and path.upper().rsplit('.', 1)[-1] in ('EXE', 'COM', 'SYS')
                  and path not in order)
    return order + rest


def program_bytes(volume, path):
    """A program from B:, or its unpacked form when $ARIA_UNPACKED names a
    folder written by bin/pklite_unpack.exe and holds that file (HANDOFF
    6.1). The unpacked folder must match the image's firmware build."""
    folder = os.environ.get('ARIA_UNPACKED')
    if folder:
        candidate = os.path.join(_native(folder), path.rsplit('\\', 1)[-1])
        if os.path.exists(candidate):
            return open(candidate, 'rb').read()
    return volume.read(path)


def code_of(data):
    """(code bytes, kind) of a DOS program: MZ header stripped, PKLITE
    detected. kind is 'exe', 'pklite', 'com' or 'sys'."""
    if data[:2] in (b'MZ', b'ZM'):
        header = struct.unpack_from('<H', data, 8)[0] * 16
        # PKLITE puts 'PK' after its version word at offset 1Ch and its
        # copyright text right behind the header.
        kind = 'pklite' if data[30:32] == b'PK' or b'PKLITE' in data[:0x100] else 'exe'
        code = data[header:]
        # In the Borland and Microsoft-style memory models the programs on B:
        # use, the initial SS is DGROUP, the data, which follows the code.
        # Scanning only below it keeps data bytes from being read as
        # "int xx" or "in/out" (seen with the unpacked AWP.EXE, HANDOFF 6.1).
        ss = struct.unpack_from('<H', data, 14)[0]
        if kind == 'exe' and 0 < ss * 16 < len(code):
            code = code[:ss * 16]
        return code, kind
    if data[:4] == b'\xff\xff\xff\xff' or data[:2] == b'\xff\xff':
        return data, 'sys'
    return data, 'com'
