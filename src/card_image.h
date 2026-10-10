#ifndef ARIA_CARD_IMAGE_H
#define ARIA_CARD_IMAGE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// A card whose home is an image file (HANDOFF 6.8, ar-aj1): the card's
// common memory byte for byte, read whole when it goes in and written whole
// when it is saved. Nothing in it is interpreted, so an image may hold any
// format the Aria's drivers know, or none yet: an unformatted card is an
// image of FFh, which the Aria formats itself.
//
// Two kinds, told apart by the extension and nothing else, so that both
// stay plain images other tools can read (user's choice, 10. 10. 2026):
// .flash is a flash card (ar-ibv), anything else SRAM.
namespace card_image {

// The extensions a new image gets and the open dialog looks for.
inline constexpr const wchar_t* kExtension = L"img";
inline constexpr const wchar_t* kFlashExtension = L"flash";
// Both kinds, for the open dialog.
inline constexpr const wchar_t* kPattern = L"*.img;*.flash";

bool IsFlash(const std::filesystem::path& file);

// True for a size a card can have. SRAM: a power of two from
// FolderCard::kMinSize to kMaxSize, because PcCard mirrors above its size
// and FORMAT.EXE measures a card by that wrap (FORMAT.EXE+25D0h). Flash: a
// whole number of 2 MB chip pairs up to 64 MB, the window's reach. An
// image of another size is refused rather than padded: padding would grow
// the user's file on the first save.
bool ValidSize(uint64_t bytes);
bool ValidFlashSize(uint64_t bytes);

// False with a Czech message when the file cannot be read or has a size no
// card of its kind has.
bool Read(const std::filesystem::path& file, std::vector<uint8_t>& image, std::wstring& error);
// Through a temporary file beside it and a rename, so a failed write leaves
// the previous image whole.
bool Write(const std::filesystem::path& file, const std::vector<uint8_t>& image, std::wstring& error);

}  // namespace card_image

#endif
