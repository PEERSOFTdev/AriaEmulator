#ifndef ARIA_ZIP_READER_H
#define ARIA_ZIP_READER_H

#include <cstdint>
#include <string>
#include <vector>

// Minimal reader for the firmware update archives (HANDOFF 1): the user
// selects the ZIP exactly as Robotron distributed it, so there is no
// separate unpacking step. Supports the stored and deflate methods, which
// is all PKZIP 2 produced; every member is checked against its CRC-32.
struct ZipMember {
  std::string name;  // As stored in the archive (8.3 DOS names).
  std::vector<uint8_t> data;
};

// Returns false and fills `error` (Czech, for the user) on any problem.
bool ReadZip(const std::vector<uint8_t>& archive,
             std::vector<ZipMember>& members, std::wstring& error);

// Raw DEFLATE stream (RFC 1951), exposed for the self-test.
bool Inflate(const uint8_t* input, std::size_t length,
             std::vector<uint8_t>& output);

uint32_t Crc32(const uint8_t* data, std::size_t length);

#endif
