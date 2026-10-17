#ifndef ARIA_CARD_DRIVE_H
#define ARIA_CARD_DRIVE_H

#include "drive.h"

// The PC card slot, drive C: (HANDOFF 6.8, ar-aj1): SRAM cards from a
// folder or an image, flash cards from a .flash image (ar-ibv).
class CardDrive : public Drive {
 public:
  CardDrive(EmulatorThread& emulator, Settings& settings, TextEncoding encoding);

  void InsertFromFolder();
  // Karta -> Vložit novou kartu: an empty folder, or an image formatted or
  // not, made and put in.
  void InsertNew();
  std::wstring TitlePart() const override;

 protected:
  const Texts& texts() const override { return texts_; }
  void Insert(AriaMachine& machine, std::vector<uint8_t> image, bool locked) override;
  std::optional<std::vector<uint8_t>> Take(AriaMachine& machine) override;
  bool Writes(AriaMachine& machine, uint64_t& writes, bool& modified) override;
  std::vector<uint8_t> Contents(AriaMachine& machine) override;
  void ClearModified(AriaMachine& machine, uint64_t writes) override;
  void SetProtected(AriaMachine& machine, bool locked) override;
  bool ReadImage(const std::filesystem::path& file, std::vector<uint8_t>& image, bool& readOnly,
                 std::wstring& error) override;
  bool WriteImage(const std::filesystem::path& file, const std::vector<uint8_t>& image, std::wstring& error) override;
  std::wstring SizeText() const override;
  bool KnownLocked(const std::wstring& path) const override;
  void RememberLock(const std::wstring& path, bool locked) override;
  void RememberMedium(const std::wstring& path, uint32_t size, bool locked) override;
  std::wstring LastPath() const override { return settings_.last_card(); }
  void SetLastPath(const std::wstring& path) override { settings_.SetLastCard(path); }
  std::wstring ImageFolder() const override { return settings_.image_folder(); }
  void SetImageFolder(const std::wstring& folder) override { settings_.SetImageFolder(folder); }
  std::wstring Slot(int number) const override { return settings_.slot(number); }
  void SetSlot(int number, const std::wstring& path) override { settings_.SetSlot(number, path); }
  void InsertFolderSlot(const std::wstring& folder, bool locked) override;
  bool MountRememberedFolder(const std::wstring& folder, bool locked) override;

 private:
  // Asks for the card's size, among those `folder` fits on, and puts it in.
  void InsertFolderAskingSize(const std::wstring& folder);
  bool MountFolder(const std::wstring& folder, uint32_t size, bool locked);

  Texts texts_;
};

#endif
