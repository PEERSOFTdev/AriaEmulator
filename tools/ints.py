"""Who provides and who consumes each software interrupt, in boot order.

  python ints.py [IMAGE]

Programs are taken in the order the Aria starts them (CONFIG.SYS device=
lines, then AUTOEXEC.BAT). A vector that some program calls before any
program installs it must have been set up by the missing boot ROM, so it
belongs to the HLE surface (HANDOFF 6.1).

Detected: `int N` (call), INT 21h AH=25h set vector and AH=35h get vector
with AL loaded by `mov ax,0x25NN` / `mov ax,0x35NN` or `mov ah`+`mov al`
within four instructions. Direct writes into the vector table are not
detected; PKLITE programs are skipped until phase 1 unpacks them.
"""

import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ariafw  # noqa: E402

MOV_AX = re.compile(r'mov ax,0x(25|35)([0-9a-f]{2})$')
MOV_AH = re.compile(r'mov ah,0x(25|35)$')
MOV_AL = re.compile(r'mov al,0x([0-9a-f]{1,2})$')
INT = re.compile(r'int 0x([0-9a-f]{1,2})$')


def scan(code, origin):
    """Returns {vector: {'call'|'set'|'get': [offsets]}}."""
    found = collections.defaultdict(lambda: collections.defaultdict(list))
    function = None   # (25h or 35h, vector, index) waiting for int 21h
    ah = al = None
    for index, (offset, _, text) in enumerate(ariafw.disassemble(code, origin)):
        match = MOV_AX.match(text)
        if match:
            function = (match.group(1), int(match.group(2), 16), index)
            continue
        match = MOV_AH.match(text)
        if match:
            ah = (match.group(1), index)
        match = MOV_AL.match(text)
        if match:
            al = (int(match.group(1), 16), index)
        if ah and al and abs(ah[1] - al[1]) <= 4:
            function = (ah[0], al[0], max(ah[1], al[1]))
            ah = al = None
        match = INT.match(text)
        if not match:
            continue
        vector = int(match.group(1), 16)
        if vector == 0x21 and function and index - function[2] <= 4:
            found[function[1]]['set' if function[0] == '25' else 'get'].append(offset)
            function = None
        else:
            found[vector]['call'].append(offset)
    return found


def main(argv):
    image, _ = ariafw.load(argv[0] if argv else None)
    volume = ariafw.Fat12(image)
    boot = ariafw.boot_order(volume)
    events = []  # (position, program, vector, action, offsets)
    skipped = []
    for position, path in enumerate(ariafw.programs(volume)):
        code, kind = ariafw.code_of(ariafw.program_bytes(volume, path))
        if kind == 'pklite':
            skipped.append(path)
            continue
        origin = 0x100 if kind == 'com' else 0
        for vector, actions in scan(code, origin).items():
            for action, offsets in actions.items():
                events.append((position, path.rsplit('\\', 1)[-1], vector, action, offsets))

    print('Boot order: ' + ', '.join(p.rsplit('\\', 1)[-1] for p in boot))
    print('PKLITE, not scanned: ' + ', '.join(p.rsplit('\\', 1)[-1] for p in skipped))
    print()
    # Documented in ARIA-INT.H (ARIATECH); their installers are PKLITE or
    # write the vector table directly, so the scan does not see them.
    documented = {0x60: 'AAPI.EXE', 0x61: 'SPEAK.EXE', 0x62: 'AAPI.EXE'}
    for vector in sorted({e[2] for e in events}):
        mine = [e for e in events if e[2] == vector]
        setters = [e for e in mine if e[3] == 'set']
        first_set = min((e[0] for e in setters), default=None)
        early = [e for e in mine if e[3] in ('call', 'get') and (first_set is None or e[0] < first_set)]
        verdict = ''
        if vector in documented:
            verdict = '  <- documented provider %s [API]' % documented[vector]
        elif early:
            owner = 'BIOS range' if vector < 0x20 else 'DOS range' if vector < 0x30 else 'unknown'
            verdict = '  <- %s, %s; boot ROM unless a PKLITE program installs it' % (
                owner, 'never installed on B:' if first_set is None else 'used before first install')
        print('INT %02Xh%s' % (vector, verdict))
        for position, name, _, action, offsets in sorted(mine):
            print('    %-4s %-13s x%-3d %s' % (action, name, len(offsets),
                                             ' '.join('%04X' % o for o in offsets[:5])))


if __name__ == '__main__':
    main(sys.argv[1:])
