#ifndef ARIA_SNAPSHOT_H
#define ARIA_SNAPSHOT_H

// What the Aria's battery keeps -- RAM with the RAM disk D:, and the clock
// chip -- saved between runs of the emulator (HANDOFF 6.9, ar-ram), after
// the Eureka A4 emulator's pamat.bin: written when the machine is switched
// off and the window closes, read back and deleted at the next start, so a
// run that ends any other way starts the next one with empty memory, as a
// cut battery would.
//
// One file per firmware image, named by its CRC-32: the memory belongs to
// one build (a RAM disk written under 6.87 holds Kamenicky names, under 7.05
// Latin 2), and switching builds then keeps each one's memory instead of
// asking which to throw away.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace snapshot {

// A `config` folder beside the EXE when one exists (the portable mode,
// created only by the user), %APPDATA%\Aria otherwise. Empty when the shell
// will not say where roaming data goes.
std::filesystem::path ConfigDirectory();

// The snapshot of the firmware image whose CRC-32 is `firmware`; empty when
// ConfigDirectory is.
std::filesystem::path FileFor(uint32_t firmware);

enum class Result { kOk, kMissing, kCorrupt };

// On kOk `state` holds AriaMachine::kStateSize bytes. kCorrupt comes with a
// Czech message in `error`.
Result Load(const std::filesystem::path& path, uint32_t firmware, std::size_t size, std::vector<uint8_t>& state,
            std::wstring& error);

// Writes through a temporary file, so a failed write leaves no half a
// snapshot. Creates %APPDATA%\Aria if needed, never the portable folder.
bool Save(const std::filesystem::path& path, uint32_t firmware, const std::vector<uint8_t>& state,
          std::wstring& error);

}  // namespace snapshot

#endif
