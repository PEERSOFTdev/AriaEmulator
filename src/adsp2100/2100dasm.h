// license:BSD-3-Clause
// copyright-holders:Aaron Giles
//
// ARIA: MAME's adsp21xx_disassembler declaration (2100dasm.h at commit
// 2aa75c50, LICENSE.mame-adsp2100.txt) with shims for the MAME types
// 2100dasm.cpp uses: util::disasm_interface, data_buffer, offs_t, the step
// flags and util::stream_format.

#ifndef ARIA_ADSP2100_2100DASM_H
#define ARIA_ADSP2100_2100DASM_H

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ostream>

#include "adsp2100/adsp2100.h"

namespace mame_adsp {

using offs_t = uint32_t;

// ARIA: program words for the disassembler, indexed by word address.
class data_buffer
{
public:
	data_buffer(const uint32_t *words, uint32_t count) : m_words(words), m_count(count) { }
	uint32_t r32(offs_t pc) const { return pc < m_count ? m_words[pc] : 0; }

private:
	const uint32_t *m_words;
	uint32_t m_count;
};

namespace util {

class disasm_interface
{
public:
	// Flags MAME's disassemblers return with the instruction length.
	static constexpr uint32_t SUPPORTED = 0x80000000;
	static constexpr uint32_t STEP_OUT = 0x40000000;
	static constexpr uint32_t STEP_OVER = 0x20000000;
	static constexpr uint32_t STEP_COND = 0x10000000;

	virtual ~disasm_interface() = default;
	virtual uint32_t opcode_alignment() const = 0;
	virtual offs_t disassemble(std::ostream &stream, offs_t pc, const data_buffer &opcodes, const data_buffer &params) = 0;
};

// MAME's printf-style formatting into a stream.
inline void stream_format(std::ostream &stream, const char *format, ...)
{
	char text[256];
	va_list args;
	va_start(args, format);
	std::vsnprintf(text, sizeof text, format, args);
	va_end(args);
	stream << text;
}

}  // namespace util

// ---- verbatim from 2100dasm.h lines 9-41 ----
class adsp21xx_disassembler : public util::disasm_interface
{
public:
	adsp21xx_disassembler() = default;
	virtual ~adsp21xx_disassembler() = default;

	virtual u32 opcode_alignment() const override;
	virtual offs_t disassemble(std::ostream &stream, offs_t pc, const data_buffer &opcodes, const data_buffer &params) override;

private:
	static const char *const flag_change[];
	static const char *const mode_change[];
	static const char *const alu_xop[];
	static const char *const alu_yop[];
	static const char *const alu_dst[];
	static const char *const mac_xop[];
	static const char *const mac_yop[];
	static const char *const mac_dst[];
	static const char *const shift_xop[];
	static const char *const reg_grp[][16];
	static const char *const dual_xreg[];
	static const char *const dual_yreg[];
	static const char *const condition[];
	static const char *const do_condition[];
	static const char *const alumac_op[][2];
	static const char *const shift_op[];
	static const char *const shift_by_op[];
	static const char *const constants[];

	void alumac(std::ostream &stream, int dest, uint32_t op);
	void aluconst(std::ostream &stream, int dest, uint32_t op);

};

}  // namespace mame_adsp

#endif
