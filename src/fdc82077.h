#ifndef ARIA_FDC82077_H
#define ARIA_FDC82077_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

// The PC floppy controller (82077 family) behind the Backpack's parallel
// chip (HANDOFF 6.7, ar-l5v), as far as BACKPACK.SYS uses it; offsets are
// in BACKPACK.SYS the file. Registers 2 DOR, 4 MSR/DSR, 5 FIFO, 7 DIR/CCR.
// One drive is wired, the Backpack's own; the execution phase is instant:
// the last command byte moves the sectors and raises the interrupt.
class Fdc82077 {
 public:
  // A 3.5" disk as a raw sector image, 512-byte sectors in cylinder,
  // head, sector order (the usual .img).
  struct Disk {
    std::vector<uint8_t> bytes;
    bool write_protected = false;
    bool modified = false;  // Written since inserted.

    int cylinders = 0, heads = 0, sectors = 0;
    uint8_t rate = 0;  // CCR value the disk is read at.

    // 720 KB (250 kbps) or 1.44 MB (500 kbps), the two densities of the
    // "3.5 inch 1.4MB" drive. The layout comes from the BPB in the boot
    // sector when there is one, else from the size: images cut short
    // after the last used sector are common, and the rest is padded with
    // F6h, as FORMAT leaves it, so a save writes the whole disk. An image
    // whose BPB names another layout (360 KB, 1.2 MB...) or that is
    // longer than its layout is refused.
    static std::optional<Disk> FromImage(std::vector<uint8_t> bytes, bool write_protected);
  };

  // Where the data of READ, WRITE and FORMAT goes: the Backpack chip's
  // buffer stands in for the PC's DMA channel 2.
  struct Dma {
    virtual ~Dma() = default;
    // Moves up to `size` bytes to (to_host) or from the host side; returns
    // how many, 0 when the channel is off or set the other way.
    virtual size_t Transfer(uint8_t* data, size_t size, bool to_host) = 0;
    virtual bool TerminalCount() const = 0;
  };

  Fdc82077(int drive, Dma& dma) : drive_(drive), dma_(dma) {}

  uint8_t Read(int reg);
  void Write(int reg, uint8_t value);
  bool Interrupt() const { return interrupt_; }

  // A new disk raises the disk-change line until the next seek.
  void Insert(Disk disk);
  std::optional<Disk> Eject();
  const Disk* disk() const { return disk_ ? &*disk_ : nullptr; }

  std::function<void(const std::string&)> log;

 private:
  void Reset();
  void Execute();
  void ReadWrite(bool write);
  void ReadId();
  void Format();
  void Seek(int drive, int head, int cylinder);
  void Result(std::initializer_list<uint8_t> bytes);
  // The disk in `drive`, readable at the current data rate, or nullptr.
  Disk* Readable(int drive);

  const int drive_;
  Dma& dma_;
  std::optional<Disk> disk_;
  bool changed_ = true;  // DIR bit 7; also while no disk is in.

  uint8_t dor_ = 0;
  uint8_t rate_ = 0;
  bool interrupt_ = false;
  bool stuck_ = false;  // In an execution phase that never ends (no disk).
  std::vector<uint8_t> command_;
  std::deque<uint8_t> result_;
  // SENSE INTERRUPT answers: ST0 and the present cylinder.
  std::deque<std::array<uint8_t, 2>> pending_;
  std::array<uint8_t, 4> cylinder_{};
};

#endif
