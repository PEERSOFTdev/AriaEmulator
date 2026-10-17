#ifndef ARIA_RESOURCE_H
#define ARIA_RESOURCE_H

// Ids as in the Eureka A4 emulator's resource.h, so that the two programs'
// menus and shortcuts can be compared line by line.

#define IDR_MAIN_MENU        100
// Two accelerator tables: the first always applies, the second only while the
// keyboard belongs to the host.  See the comments in aria.rc.
#define IDR_ACCELERATORS     101
#define IDR_ACCELERATORS_HOST 102

// Menu and accelerator commands.
#define ID_FILE_EXIT         40002
// Reset is the battery cut: RAM, the RAM disk D: and the clock's CMOS are
// lost.  The warm reset keeps them; it switches an off machine on and
// resets a running one.  See AriaMachine::PowerOn and BatteryCut.
#define ID_MACHINE_RESET     40010
#define ID_MACHINE_POWEROFF  40011
#define ID_MACHINE_POWERON   40012
#define ID_KEYBOARD_RELEASE  40023
#define ID_KEYBOARD_PASSONCE 40024
// Keys the host keyboard cannot play, sent to the Aria from the menu, as
// Eureka's "Poslať Eureke kláves" (its 40025-40028).
#define ID_KEYBOARD_SEND_KONEC 40029
// The external keyboard (HANDOFF 6.6, ar-w6w): the adapter in or out, and
// F11, Ctrl+K between it and the braille keys.
#define ID_KEYBOARD_EXTERNAL      40030
#define ID_KEYBOARD_EXTERNAL_TYPE 40031
#define ID_HELP_KEYS         40040
#define ID_HELP_ABOUT        40041
// F12: the menu bar, on a key the Aria has no use for (aria.rc).
#define ID_ACTIVATE_MENU     40050
// The card slot, drive C: (HANDOFF 6.8), with the Eureka emulator's
// Disketa shortcuts: Ctrl+I insert, Ctrl+M new, Ctrl+0 eject, Ctrl+Z write
// protect; Ctrl+O for an image, which the Eureka has not.
#define ID_CARD_INSERT       40060
#define ID_CARD_EJECT        40061
#define ID_CARD_PROTECT      40062
#define ID_CARD_INSERT_IMAGE 40063
#define ID_CARD_NEW          40064
// The nine slots, Ctrl+1 to Ctrl+9, and the dialog that fills them.
#define ID_CARD_SLOTS        40065
#define ID_CARD_SLOT1        40071
#define ID_CARD_SLOT9        40079
#define ID_CARD_SLOT_FIRST   ID_CARD_SLOT1
#define ID_CARD_SLOT_LAST    ID_CARD_SLOT9
// The Backpack floppy drive (HANDOFF 6.7, ar-l5v).
#define ID_DISK_INSERT_IMAGE 40080
#define ID_DISK_NEW          40081
#define ID_DISK_EJECT        40082
#define ID_DISK_PROTECT      40083
#define ID_DISK_CONNECT      40084
#define ID_DISK_SLOTS        40085
#define ID_DISK_INSERT       40086
#define ID_DISK_SLOT1        40091
#define ID_DISK_SLOT9        40099
#define ID_DISK_SLOT_FIRST   ID_DISK_SLOT1
#define ID_DISK_SLOT_LAST    ID_DISK_SLOT9

// Dialogs. IDC_STATIC as winres.h has it, which aria.rc does not include.
#ifndef IDC_STATIC
#define IDC_STATIC           (-1)
#endif
#define IDD_CARD_SIZE        200
#define IDC_CARD_SIZE_LABEL  201
#define IDC_CARD_SIZES       202
#define IDD_CARD_NEW         210
#define IDC_CARD_NEW_FOLDER  211
#define IDC_CARD_NEW_IMAGE   212
#define IDC_CARD_NEW_BLANK   213
// The four kinds are consecutive: CheckRadioButton takes them as a range.
#define IDC_CARD_NEW_FLASH   214
#define IDC_CARD_NEW_SIZES   215
#define IDD_CARD_SLOTS       220
#define IDC_SLOT_LIST        221
#define IDC_SLOT_FOLDER      222
#define IDC_SLOT_IMAGE       223
#define IDC_SLOT_CURRENT     224
#define IDC_SLOT_CLEAR       225
#define IDC_SLOT_LOCK        226
#define IDD_DISK_NEW         230
// Consecutive, for CheckRadioButton.
#define IDC_DISK_NEW_HIGH    231
#define IDC_DISK_NEW_LOW     232
#define IDC_DISK_NEW_BLANK   233
#define IDC_DISK_NEW_FOLDER  234

#endif
