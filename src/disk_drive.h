#ifndef ARIA_DISK_DRIVE_H
#define ARIA_DISK_DRIVE_H

#include "drive.h"

// The Backpack floppy drive on the printer port (HANDOFF 6.7, ar-l5v),
// drive E: when connected: 3.5" diskettes of 720 KB and 1.44 MB from
// image files. Its cable is a switch of its own, remembered between runs;
// BACKPACK.SYS finds the drive only at start-up.
class DiskDrive : public Drive {
 public:
  DiskDrive(EmulatorThread& emulator, Settings& settings, TextEncoding encoding);

  // Disketa -> Vložit novou disketu: formatted 1.44 MB or 720 KB, or blank.
  void InsertNew();
  // Plugs the cable in or out, says when the Aria will notice, and
  // remembers it.
  void ToggleConnection();
  bool connected() const { return settings_.backpack_connected(); }
  // Before the machine starts: the cable as the last run left it.
  void ConnectRemembered();
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
  bool KnownLocked(const std::wstring& path) const override { return settings_.DiskLocked(path); }
  void RememberLock(const std::wstring& path, bool locked) override { settings_.SetDiskLocked(path, locked); }
  std::wstring LastPath() const override { return settings_.last_disk(); }
  void SetLastPath(const std::wstring& path) override { settings_.SetLastDisk(path); }
  std::wstring ImageFolder() const override { return settings_.disk_folder(); }
  void SetImageFolder(const std::wstring& folder) override { settings_.SetDiskFolder(folder); }
  std::wstring Slot(int number) const override { return settings_.disk_slot(number); }
  void SetSlot(int number, const std::wstring& path) override { settings_.SetDiskSlot(number, path); }
  void InsertFolderSlot(const std::wstring& folder, bool locked) override;
  bool MountRememberedFolder(const std::wstring& folder, bool locked) override;

 private:
  Texts texts_;
};

#endif
