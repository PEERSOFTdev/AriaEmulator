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
#define ID_KEYBOARD_RELEASE  40023
#define ID_KEYBOARD_PASSONCE 40024
#define ID_HELP_KEYS         40040
#define ID_HELP_ABOUT        40041
// F12: the menu bar, on a key the Aria has no use for (aria.rc).
#define ID_ACTIVATE_MENU     40050

#endif
