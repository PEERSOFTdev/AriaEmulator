#include "zip_reader.h"

#include <array>
#include <cstring>

namespace {

uint16_t Read16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
uint32_t Read32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

class BitReader {
 public:
  BitReader(const uint8_t* data, std::size_t length) : data_(data), length_(length) {}

  // Returns false when the stream runs out; DEFLATE packs bits LSB first.
  bool Bits(int count, uint32_t& value) {
    value = 0;
    for (int index = 0; index < count; ++index) {
      if (position_ >= length_) return false;
      const uint32_t bit = (data_[position_] >> bit_) & 1u;
      value |= bit << index;
      if (++bit_ == 8) {
        bit_ = 0;
        ++position_;
      }
    }
    return true;
  }

  void AlignToByte() {
    if (bit_ != 0) {
      bit_ = 0;
      ++position_;
    }
  }

  bool Bytes(std::size_t count, std::vector<uint8_t>& output) {
    if (length_ - position_ < count || position_ > length_) return false;
    output.insert(output.end(), data_ + position_, data_ + position_ + count);
    position_ += count;
    return true;
  }

 private:
  const uint8_t* data_;
  std::size_t length_;
  std::size_t position_ = 0;
  int bit_ = 0;
};

// Canonical Huffman decoding table in the counts/symbols form of RFC 1951
// section 3.2.2: decoding walks code lengths one bit at a time.
struct Huffman {
  std::array<uint16_t, 16> counts{};
  std::array<uint16_t, 320> symbols{};
};

bool Build(Huffman& table, const uint8_t* lengths, int count) {
  table.counts.fill(0);
  for (int symbol = 0; symbol < count; ++symbol) ++table.counts[lengths[symbol]];
  table.counts[0] = 0;
  std::array<uint16_t, 16> offsets{};
  int left = 1;
  for (int length = 1; length < 16; ++length) {
    left = (left << 1) - table.counts[length];
    if (left < 0) return false;  // Over-subscribed code.
    offsets[length] = static_cast<uint16_t>(
        length == 1 ? 0 : offsets[length - 1] + table.counts[length - 1]);
  }
  for (int symbol = 0; symbol < count; ++symbol) {
    if (lengths[symbol] != 0) table.symbols[offsets[lengths[symbol]]++] = static_cast<uint16_t>(symbol);
  }
  return true;
}

bool DecodeSymbol(BitReader& reader, const Huffman& table, int& symbol) {
  int code = 0;
  int first = 0;
  int index = 0;
  for (int length = 1; length < 16; ++length) {
    uint32_t bit = 0;
    if (!reader.Bits(1, bit)) return false;
    code |= static_cast<int>(bit);
    const int count = table.counts[length];
    if (code - count < first) {
      symbol = table.symbols[index + (code - first)];
      return true;
    }
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  return false;
}

constexpr uint16_t kLengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                                      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr uint8_t kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                    6145, 8193, 12289, 16385, 24577};
constexpr uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool InflateBlock(BitReader& reader, const Huffman& lengths, const Huffman& distances,
                  std::vector<uint8_t>& output) {
  for (;;) {
    int symbol = 0;
    if (!DecodeSymbol(reader, lengths, symbol)) return false;
    if (symbol < 256) {
      output.push_back(static_cast<uint8_t>(symbol));
      continue;
    }
    if (symbol == 256) return true;
    symbol -= 257;
    if (symbol >= 29) return false;
    uint32_t extra = 0;
    if (!reader.Bits(kLengthExtra[symbol], extra)) return false;
    const std::size_t length = kLengthBase[symbol] + extra;
    int distance_symbol = 0;
    if (!DecodeSymbol(reader, distances, distance_symbol) || distance_symbol >= 30) return false;
    if (!reader.Bits(kDistExtra[distance_symbol], extra)) return false;
    const std::size_t distance = kDistBase[distance_symbol] + extra;
    if (distance > output.size()) return false;
    // Byte by byte on purpose: an overlapping copy (distance < length) is
    // how DEFLATE encodes runs.
    for (std::size_t copied = 0; copied < length; ++copied) {
      output.push_back(output[output.size() - distance]);
    }
  }
}

bool DynamicTables(BitReader& reader, Huffman& lengths, Huffman& distances) {
  uint32_t hlit = 0, hdist = 0, hclen = 0;
  if (!reader.Bits(5, hlit) || !reader.Bits(5, hdist) || !reader.Bits(4, hclen)) return false;
  hlit += 257;
  hdist += 1;
  hclen += 4;
  if (hlit > 286 || hdist > 30) return false;
  static constexpr uint8_t kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                         11, 4, 12, 3, 13, 2, 14, 1, 15};
  uint8_t code_lengths[19]{};
  for (uint32_t index = 0; index < hclen; ++index) {
    uint32_t value = 0;
    if (!reader.Bits(3, value)) return false;
    code_lengths[kOrder[index]] = static_cast<uint8_t>(value);
  }
  Huffman code_table;
  if (!Build(code_table, code_lengths, 19)) return false;

  uint8_t all[316]{};
  uint32_t index = 0;
  while (index < hlit + hdist) {
    int symbol = 0;
    if (!DecodeSymbol(reader, code_table, symbol)) return false;
    if (symbol < 16) {
      all[index++] = static_cast<uint8_t>(symbol);
      continue;
    }
    uint8_t repeated = 0;
    uint32_t repeat = 0;
    if (symbol == 16) {
      if (index == 0 || !reader.Bits(2, repeat)) return false;
      repeated = all[index - 1];
      repeat += 3;
    } else if (symbol == 17) {
      if (!reader.Bits(3, repeat)) return false;
      repeat += 3;
    } else {
      if (!reader.Bits(7, repeat)) return false;
      repeat += 11;
    }
    if (index + repeat > hlit + hdist) return false;
    while (repeat-- > 0) all[index++] = repeated;
  }
  if (all[256] == 0) return false;  // No end-of-block code.
  return Build(lengths, all, static_cast<int>(hlit)) &&
         Build(distances, all + hlit, static_cast<int>(hdist));
}

void FixedTables(Huffman& lengths, Huffman& distances) {
  uint8_t all[288];
  int symbol = 0;
  for (; symbol < 144; ++symbol) all[symbol] = 8;
  for (; symbol < 256; ++symbol) all[symbol] = 9;
  for (; symbol < 280; ++symbol) all[symbol] = 7;
  for (; symbol < 288; ++symbol) all[symbol] = 8;
  Build(lengths, all, 288);
  uint8_t dist[30];
  std::memset(dist, 5, sizeof dist);
  Build(distances, dist, 30);
}

}  // namespace

uint32_t Crc32(const uint8_t* data, std::size_t length) {
  static const auto kTable = [] {
    std::array<uint32_t, 256> table{};
    for (uint32_t n = 0; n < 256; ++n) {
      uint32_t c = n;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[n] = c;
    }
    return table;
  }();
  uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t index = 0; index < length; ++index) {
    crc = kTable[(crc ^ data[index]) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu;
}

bool Inflate(const uint8_t* input, std::size_t length, std::vector<uint8_t>& output) {
  BitReader reader(input, length);
  uint32_t last = 0;
  do {
    uint32_t type = 0;
    if (!reader.Bits(1, last) || !reader.Bits(2, type)) return false;
    if (type == 0) {
      reader.AlignToByte();
      std::vector<uint8_t> header;
      if (!reader.Bytes(4, header)) return false;
      const uint16_t len = Read16(header.data());
      const uint16_t nlen = Read16(header.data() + 2);
      if (static_cast<uint16_t>(~nlen) != len) return false;
      if (!reader.Bytes(len, output)) return false;
    } else if (type == 1) {
      Huffman lengths, distances;
      FixedTables(lengths, distances);
      if (!InflateBlock(reader, lengths, distances, output)) return false;
    } else if (type == 2) {
      Huffman lengths, distances;
      if (!DynamicTables(reader, lengths, distances)) return false;
      if (!InflateBlock(reader, lengths, distances, output)) return false;
    } else {
      return false;
    }
  } while (last == 0);
  return true;
}

bool Gunzip(const std::vector<uint8_t>& file, std::vector<uint8_t>& output) {
  output.clear();
  if (file.size() < 18 || file[0] != 0x1F || file[1] != 0x8B || file[2] != 8) return false;
  const uint8_t flags = file[3];
  std::size_t position = 10;
  if (flags & 0x04) {  // FEXTRA
    if (position + 2 > file.size()) return false;
    position += 2 + Read16(&file[position]);
  }
  for (const uint8_t text_flag : {uint8_t{0x08}, uint8_t{0x10}}) {  // FNAME, FCOMMENT
    if (flags & text_flag) {
      while (position < file.size() && file[position] != 0) ++position;
      ++position;
    }
  }
  if (flags & 0x02) position += 2;  // FHCRC
  if (position + 8 > file.size()) return false;
  const uint32_t crc = Read32(&file[file.size() - 8]);
  const uint32_t size = Read32(&file[file.size() - 4]);
  output.reserve(size);
  if (!Inflate(&file[position], file.size() - 8 - position, output)) return false;
  return static_cast<uint32_t>(output.size()) == size && Crc32(output.data(), output.size()) == crc;
}

bool ReadZip(const std::vector<uint8_t>& archive, std::vector<ZipMember>& members,
             std::wstring& error) {
  members.clear();
  // Walk local headers from the start. The update archives are plain PKZIP
  // 2 output with sizes in the local header (no data descriptors), which
  // the check below enforces instead of assuming.
  std::size_t position = 0;
  while (position + 30 <= archive.size() && Read32(&archive[position]) == 0x04034b50u) {
    const uint8_t* header = &archive[position];
    const uint16_t flags = Read16(header + 6);
    const uint16_t method = Read16(header + 8);
    const uint32_t crc = Read32(header + 14);
    const uint32_t packed = Read32(header + 18);
    const uint32_t size = Read32(header + 22);
    const uint16_t name_length = Read16(header + 26);
    const uint16_t extra_length = Read16(header + 28);
    const std::size_t data = position + 30 + name_length + extra_length;
    if ((flags & 0x0009) != 0) {
      error = L"Archiv používá šifrování nebo dodatečné záznamy velikosti, které emulátor nečte.";
      return false;
    }
    if (data > archive.size() || archive.size() - data < packed) {
      error = L"Archiv je zkrácený nebo poškozený.";
      return false;
    }
    ZipMember member;
    member.name.assign(reinterpret_cast<const char*>(header + 30), name_length);
    if (method == 0) {
      member.data.assign(archive.begin() + static_cast<std::ptrdiff_t>(data),
                         archive.begin() + static_cast<std::ptrdiff_t>(data + packed));
    } else if (method == 8) {
      member.data.reserve(size);
      if (!Inflate(&archive[data], packed, member.data)) {
        error = L"Soubor v archivu se nepodařilo rozbalit.";
        return false;
      }
    } else {
      error = L"Archiv používá kompresní metodu, kterou emulátor nezná.";
      return false;
    }
    if (member.data.size() != size || Crc32(member.data.data(), member.data.size()) != crc) {
      error = L"Kontrolní součet souboru v archivu nesedí, archiv je poškozený.";
      return false;
    }
    members.push_back(std::move(member));
    position = data + packed;
  }
  if (members.empty()) {
    error = L"Soubor není archiv ZIP.";
    return false;
  }
  return true;
}
