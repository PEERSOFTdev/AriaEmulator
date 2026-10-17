#ifndef ARIA_FLOPPY_IMAGE_H
#define ARIA_FLOPPY_IMAGE_H

#include <filesystem>
#include <string>
#include <vector>

#include "fdc82077.h"

// A diskette whose home is an image file (HANDOFF 6.7, ar-l5v): the raw
// sectors as other emulators and tools keep them, read whole when it goes
// into the Backpack drive and written whole when it is saved. Foreign
// images are welcome; one cut short is padded and saved whole (user's
// choice, 11. 10. 2026), see Fdc82077::Disk::FromImage.
namespace floppy_image {

// The extension a new image gets, and what the open dialog looks for: the
// names raw diskette images go by.
inline constexpr const wchar_t* kExtension = L"img";
inline constexpr const wchar_t* kPattern = L"*.img;*.ima;*.vfd;*.flp;*.dsk";

// False with a Czech message when the file cannot be read or is no 3.5"
// diskette the drive can take. A read-only file is a write-protected disk.
bool Read(const std::filesystem::path& file, Fdc82077::Disk& disk, std::wstring& error);
// Through a temporary file and a rename, as card images are written.
bool Write(const std::filesystem::path& file, const std::vector<uint8_t>& bytes, std::wstring& error);

}  // namespace floppy_image

#endif
