// license:BSD-3-Clause
// copyright-holders:Carl
//
// ARIA: declarations for the MAME i86/i186 core (LICENSE.mame-i86.txt),
// rewritten so that the instruction code in i86inline.h, i86.cpp and
// i186.cpp compiles verbatim without the MAME framework. Member names and
// enums are MAME's; the device, address-space, debugger and save-state
// plumbing is replaced by the small shims below and by the I86Bus
// interface. Only what the Aria needs is kept: the 80186/80188 path with
// its PCB peripherals (interrupt controller, timers, DMA, chip selects);
// MAME's scheduler is replaced by the cycle timers declared below
// (HANDOFF 6.12).

#ifndef ARIA_I86_I86_H
#define ARIA_I86_I86_H

#include <cstdint>
#include <cstring>

namespace mame_i86 {

// ARIA: the bus the core runs on. The Aria's 80C188 has an 8-bit data bus,
// so word accesses are always split into two byte accesses here.
class I86Bus {
 public:
  virtual ~I86Bus() = default;
  virtual uint8_t ReadMemory(uint32_t address) = 0;  // 20-bit physical.
  virtual void WriteMemory(uint32_t address, uint8_t value) = 0;
  virtual uint8_t ReadIo(uint16_t port) = 0;
  virtual void WriteIo(uint16_t port, uint8_t value) = 0;
  // INTA from an external controller cascaded on INT0/INT1 (cascade mode
  // of the PCB interrupt controller): returns the vector number.
  virtual uint8_t AcknowledgeInterrupt() { return 0; }
  // TMROUT0/TMROUT1 pin levels.
  virtual void TimerOutput(int timer, int state) {}
  // A chip-select register was written: 0 UMCS, 1 LMCS, 2 PACS, 3 MMCS,
  // 4 MPCS, with the value as the PCB stores it.
  virtual void ChipSelect(int which, uint16_t value) {}
};

// ---- ARIA: shims for the few MAME facilities the core uses ----
using offs_t = uint32_t;
using u8 = uint8_t;
using u16 = uint16_t;
#define NATIVE_ENDIAN_VALUE_LE_BE(le, be) (le)
#define BIT(x, n) (((x) >> (n)) & 1)
constexpr int CLEAR_LINE = 0;
constexpr int ASSERT_LINE = 1;
constexpr int INPUT_LINE_IRQ0 = 0;
constexpr int INPUT_LINE_NMI = 32;
#define INPUT_LINE_INT0 INPUT_LINE_IRQ0
#define INPUT_LINE_TEST 20

inline uint16_t swapendian_int16(uint16_t value) {
  return static_cast<uint16_t>((value >> 8) | (value << 8));
}

// Logging and debugger hooks of MAME are compiled out.
template <typename... Args>
inline void logerror(const char*, Args&&...) {}

// A MAME address space reduced to what the core calls, routed to I86Bus.
// Addresses wrap at 20 bits for memory and 16 bits for I/O, like the
// address lines do.
class AddressSpace {
 public:
  AddressSpace(I86Bus*& bus, bool io) : bus_(bus), io_(io) {}
  uint8_t read_byte(offs_t address) {
    return io_ ? bus_->ReadIo(static_cast<uint16_t>(address))
               : bus_->ReadMemory(address & 0xFFFFF);
  }
  void write_byte(offs_t address, uint8_t value) {
    if (io_) {
      bus_->WriteIo(static_cast<uint16_t>(address), value);
    } else {
      bus_->WriteMemory(address & 0xFFFFF, value);
    }
  }
  uint16_t read_word_unaligned(offs_t address) {
    const uint16_t low = read_byte(address);
    return static_cast<uint16_t>(low | read_byte(address + 1) << 8);
  }
  void write_word_unaligned(offs_t address, uint16_t value) {
    write_byte(address, static_cast<uint8_t>(value));
    write_byte(address + 1, static_cast<uint8_t>(value >> 8));
  }
  // The 80188 bus is 8 bits wide; DMA does byte transfers only (i186.cpp).
  int data_width() const { return 8; }
  // Masked word write as used by write_port_byte_al: only the selected
  // half reaches the 8-bit bus.
  void write_word(offs_t address, uint16_t value, uint16_t mask) {
    if (mask & 0x00FF) write_byte(address, static_cast<uint8_t>(value));
    if (mask & 0xFF00) write_byte(address + 1, static_cast<uint8_t>(value >> 8));
  }

 private:
  I86Bus*& bus_;
  bool io_;
};

using address_space = AddressSpace;  // MAME's name, used by the DMA code.

// MAME's devcb line callback where the line goes nowhere (LOCK, IRQA, the
// iRMX interrupt output).
struct LineCallback {
  void operator()(int) const {}
};

// MAME's scheduler reduced to CPU cycles: attotime carries a cycle count,
// emu_timer is a one-shot that the CPU fires between instructions
// (i80186_cpu_device::ServiceTimers). HANDOFF 6.12.
struct attotime {
  int64_t cycles;
  static const attotime never;
  bool operator==(const attotime& other) const { return cycles == other.cycles; }
};
inline const attotime attotime::never{INT64_MAX};

class emu_timer {
 public:
  explicit emu_timer(const int64_t* now = nullptr) : now_(now) {}
  void bind(const int64_t* now) { now_ = now; }
  void adjust(attotime delay, int param = 0) {
    param_ = param;
    if (delay == attotime::never) {
      enabled_ = false;
    } else {
      enabled_ = true;
      due_ = *now_ + delay.cycles;
    }
  }
  bool enabled() const { return enabled_; }
  attotime remaining() const { return attotime{enabled_ && due_ > *now_ ? due_ - *now_ : 0}; }
  int64_t due() const { return due_; }
  int param() const { return param_; }
  void disable() { enabled_ = false; }

 private:
  const int64_t* now_;
  bool enabled_ = false;
  int64_t due_ = 0;
  int param_ = 0;
};

/////////////////////////////////////////////////////////////////

enum
{
	I8086_PC = 0,
	I8086_IP = 1, I8086_AX, I8086_CX, I8086_DX, I8086_BX, I8086_SP, I8086_BP, I8086_SI, I8086_DI,
	I8086_AL, I8086_AH, I8086_CL, I8086_CH, I8086_DL, I8086_DH, I8086_BL, I8086_BH,
	I8086_FLAGS, I8086_ES, I8086_CS, I8086_SS, I8086_DS,
	I8086_VECTOR, I8086_HALT
};

class i8086_common_cpu_device
{
protected:
	enum
	{
		EXCEPTION, IRET,                                /* EXCEPTION, iret */
		INT3, INT_IMM, INTO_NT, INTO_T,                 /* intS */
		OVERRIDE,                                       /* SEGMENT OVERRIDES */
		FLAG_OPS, LAHF, SAHF,                           /* FLAG OPERATIONS */
		AAA, AAS, AAM, AAD,                             /* ARITHMETIC ADJUSTS */
		DAA, DAS,                                       /* DECIMAL ADJUSTS */
		CBW, CWD,                                       /* SIGN EXTENSION */
		HLT, LOAD_PTR, LEA, NOP, WAIT, XLAT,            /* MISC */
		JMP_SHORT, JMP_NEAR, JMP_FAR,                   /* DIRECT jmpS */
		JMP_R16, JMP_M16, JMP_M32,                      /* INDIRECT jmpS */
		CALL_NEAR, CALL_FAR,                            /* DIRECT callS */
		CALL_R16, CALL_M16, CALL_M32,                   /* INDIRECT callS */
		RET_NEAR, RET_FAR, RET_NEAR_IMM, RET_FAR_IMM,   /* RETURNS */
		JCC_NT, JCC_T, JCXZ_NT, JCXZ_T,                 /* CONDITIONAL jmpS */
		LOOP_NT, LOOP_T, LOOPE_NT, LOOPE_T,             /* LOOPS */
		IN_IMM8, IN_IMM16, IN_DX8, IN_DX16,             /* PORT READS */
		OUT_IMM8, OUT_IMM16, OUT_DX8, OUT_DX16,         /* PORT WRITES */
		MOV_RR8, MOV_RM8, MOV_MR8,                      /* MOVE, 8-BIT */
		MOV_RI8, MOV_MI8,                               /* MOVE, 8-BIT IMMEDIATE */
		MOV_RR16, MOV_RM16, MOV_MR16,                   /* MOVE, 16-BIT */
		MOV_RI16, MOV_MI16,                             /* MOVE, 16-BIT IMMEDIATE */
		MOV_AM8, MOV_AM16, MOV_MA8, MOV_MA16,           /* MOVE, al/ax MEMORY */
		MOV_SR, MOV_SM, MOV_RS, MOV_MS,                 /* MOVE, SEGMENT REGISTERS */
		XCHG_RR8, XCHG_RM8,                             /* EXCHANGE, 8-BIT */
		XCHG_RR16, XCHG_RM16, XCHG_AR16,                /* EXCHANGE, 16-BIT */
		PUSH_R16, PUSH_M16, PUSH_SEG, PUSHF,            /* PUSHES */
		POP_R16, POP_M16, POP_SEG, POPF,                /* POPS */
		ALU_RR8, ALU_RM8, ALU_MR8,                      /* alu OPS, 8-BIT */
		ALU_RI8, ALU_MI8, ALU_MI8_RO,                   /* alu OPS, 8-BIT IMMEDIATE */
		ALU_RR16, ALU_RM16, ALU_MR16,                   /* alu OPS, 16-BIT */
		ALU_RI16, ALU_MI16, ALU_MI16_RO,                /* alu OPS, 16-BIT IMMEDIATE */
		ALU_R16I8, ALU_M16I8, ALU_M16I8_RO,             /* alu OPS, 16-BIT W/8-BIT IMMEDIATE */
		MUL_R8, MUL_R16, MUL_M8, MUL_M16,               /* mul */
		IMUL_R8, IMUL_R16, IMUL_M8, IMUL_M16,           /* imul */
		DIV_R8, DIV_R16, DIV_M8, DIV_M16,               /* div */
		IDIV_R8, IDIV_R16, IDIV_M8, IDIV_M16,           /* idiv */
		INCDEC_R8, INCDEC_R16, INCDEC_M8, INCDEC_M16,   /* inc/dec */
		NEGNOT_R8, NEGNOT_R16, NEGNOT_M8, NEGNOT_M16,   /* neg/not */
		ROT_REG_1, ROT_REG_BASE, ROT_REG_BIT,           /* REG SHIFT/ROTATE */
		ROT_M8_1, ROT_M8_BASE, ROT_M8_BIT,              /* M8 SHIFT/ROTATE */
		ROT_M16_1, ROT_M16_BASE, ROT_M16_BIT,           /* M16 SHIFT/ROTATE */
		CMPS8, REP_CMPS8_BASE, REP_CMPS8_COUNT,         /* cmps 8-BIT */
		CMPS16, REP_CMPS16_BASE, REP_CMPS16_COUNT,      /* cmps 16-BIT */
		SCAS8, REP_SCAS8_BASE, REP_SCAS8_COUNT,         /* scas 8-BIT */
		SCAS16, REP_SCAS16_BASE, REP_SCAS16_COUNT,      /* scas 16-BIT */
		LODS8, REP_LODS8_BASE, REP_LODS8_COUNT,         /* lods 8-BIT */
		LODS16, REP_LODS16_BASE, REP_LODS16_COUNT,      /* lods 16-BIT */
		STOS8, REP_STOS8_BASE, REP_STOS8_COUNT,         /* stos 8-BIT */
		STOS16, REP_STOS16_BASE, REP_STOS16_COUNT,      /* stos 16-BIT */
		MOVS8, REP_MOVS8_BASE, REP_MOVS8_COUNT,         /* movs 8-BIT */
		MOVS16, REP_MOVS16_BASE, REP_MOVS16_COUNT,      /* movs 16-BIT */
		INS8, REP_INS8_BASE, REP_INS8_COUNT,            /* (80186) ins 8-BIT */
		INS16, REP_INS16_BASE, REP_INS16_COUNT,         /* (80186) ins 16-BIT */
		OUTS8, REP_OUTS8_BASE, REP_OUTS8_COUNT,         /* (80186) outs 8-BIT */
		OUTS16, REP_OUTS16_BASE, REP_OUTS16_COUNT,      /* (80186) outs 16-BIT */
		PUSH_IMM, PUSHA, POPA,                          /* (80186) push IMMEDIATE, pusha/popa */
		IMUL_RRI8, IMUL_RMI8,                           /* (80186) imul IMMEDIATE 8-BIT */
		IMUL_RRI16, IMUL_RMI16,                         /* (80186) imul IMMEDIATE 16-BIT */
		ENTER0, ENTER1, ENTER_BASE, ENTER_COUNT, LEAVE, /* (80186) enter/leave */
		BOUND                                           /* (80186) bound */
	};

	enum SREGS { ES=0, CS, SS, DS };
	enum WREGS { AX=0, CX, DX, BX, SP, BP, SI, DI };

	// ARIA: replaces the MAME device constructor.
	i8086_common_cpu_device();
	virtual ~i8086_common_cpu_device() = default;

	void device_reset();
	void execute_set_input(int inputnum, int state);

	virtual void interrupt(int int_num, int trap = 1);
	virtual bool common_op(uint8_t op);

	// Accessing memory and io
	virtual uint8_t read_byte(uint32_t addr);
	virtual uint16_t read_word(uint32_t addr);
	virtual void write_byte(uint32_t addr, uint8_t data);
	virtual void write_word(uint32_t addr, uint16_t data);
	virtual uint8_t read_port_byte(uint16_t port);
	virtual uint16_t read_port_word(uint16_t port);
	virtual void write_port_byte(uint16_t port, uint8_t data);
	virtual void write_port_byte_al(uint16_t port);
	virtual void write_port_word(uint16_t port, uint16_t data);

	// Executing instructions
	uint8_t fetch_op() { return fetch(); }
	virtual uint8_t fetch() = 0;
	inline uint16_t fetch_word();
	inline uint8_t repx_op();

	// Cycles passed while executing instructions
	inline void CLK(uint8_t op);
	inline void CLKM(uint8_t op_reg, uint8_t op_mem);

	// Memory handling while executing instructions
	virtual uint32_t calc_addr(int seg, uint16_t offset, int size, int op, bool override = true);
	inline uint32_t get_ea(int size, int op);
	inline void PutbackRMByte(uint8_t data);
	inline void PutbackRMWord(uint16_t data);
	inline void RegByte(uint8_t data);
	inline void RegWord(uint16_t data);
	inline uint8_t RegByte();
	inline uint16_t RegWord();
	inline uint16_t GetRMWord();
	inline uint16_t GetnextRMWord();
	inline uint8_t GetRMByte();
	inline void PutMemB(int seg, uint16_t offset, uint8_t data);
	inline void PutMemW(int seg, uint16_t offset, uint16_t data);
	inline uint8_t GetMemB(int seg, uint16_t offset);
	inline uint16_t GetMemW(int seg, uint16_t offset);
	inline void PutImmRMWord();
	inline void PutRMWord(uint16_t val);
	inline void PutRMByte(uint8_t val);
	inline void PutImmRMByte();
	inline void DEF_br8();
	inline void DEF_wr16();
	inline void DEF_r8b();
	inline void DEF_r16w();
	inline void DEF_ald8();
	inline void DEF_axd16();

	// Flags
	inline void set_CFB(uint32_t x);
	inline void set_CFW(uint32_t x);
	inline void set_AF(uint32_t x,uint32_t y,uint32_t z);
	inline void set_SF(uint32_t x);
	inline void set_ZF(uint32_t x);
	inline void set_PF(uint32_t x);
	inline void set_SZPF_Byte(uint32_t x);
	inline void set_SZPF_Word(uint32_t x);
	inline void set_OFW_Add(uint32_t x,uint32_t y,uint32_t z);
	inline void set_OFB_Add(uint32_t x,uint32_t y,uint32_t z);
	inline void set_OFW_Sub(uint32_t x,uint32_t y,uint32_t z);
	inline void set_OFB_Sub(uint32_t x,uint32_t y,uint32_t z);
	inline uint16_t CompressFlags() const;
	inline void ExpandFlags(uint16_t f);

	// rep instructions
	inline void i_insb();
	inline void i_insw();
	inline void i_outsb();
	inline void i_outsw();
	inline void i_movsb();
	inline void i_movsw();
	inline void i_cmpsb();
	inline void i_cmpsw();
	inline void i_stosb();
	inline void i_stosw();
	inline void i_lodsb();
	inline void i_lodsw();
	inline void i_scasb();
	inline void i_scasw();
	inline void i_popf();

	// sub implementations
	inline uint32_t ADDB(uint8_t c = 0);
	inline uint32_t ADDX(uint8_t c = 0);
	inline uint32_t SUBB(uint8_t b = 0);
	inline uint32_t SUBX(uint8_t b = 0);
	inline void ORB();
	inline void ORW();
	inline void ANDB();
	inline void ANDX();
	inline void XORB();
	inline void XORW();
	inline void ROL_BYTE();
	inline void ROL_WORD();
	inline void ROR_BYTE();
	inline void ROR_WORD();
	inline void ROLC_BYTE();
	inline void ROLC_WORD();
	inline void RORC_BYTE();
	inline void RORC_WORD();
	inline void SHL_BYTE(uint8_t c);
	inline void SHL_WORD(uint8_t c);
	inline void SHR_BYTE(uint8_t c);
	inline void SHR_WORD(uint8_t c);
	inline void SHRA_BYTE(uint8_t c);
	inline void SHRA_WORD(uint8_t c);
	inline void XchgAXReg(uint8_t reg);
	inline void IncWordReg(uint8_t reg);
	inline void DecWordReg(uint8_t reg);
	inline void PUSH(uint16_t data);
	inline uint16_t POP();
	inline void JMP(bool cond);
	inline void ADJ4(int8_t param1, int8_t param2);
	inline void ADJB(int8_t param1, int8_t param2);

	// ARIA: OF after a shift or rotate by CL or by an immediate (C0h, C1h,
	// D2h, D3h), which MAME's 80186 path never set, not even for a count of
	// 1 where Intel defines it. Applies the single-bit rule of D0h/D1h to the
	// last step, as the 8088 does for any count (SingleStepTests 8088 D2,
	// D3: about 46 % failed without it). For counts above 1 Intel leaves OF
	// undefined and the 80186 is not measured. HANDOFF 6.12.
	void SetShiftOverflow(uint8_t operation, uint8_t count, bool word)
	{
		const uint32_t msb = word ? 0x8000 : 0x80;
		const bool top = (m_dst & msb) != 0;
		const bool next = (m_dst & (msb >> 1)) != 0;
		switch (operation)
		{
		case 0x00: case 0x10: case 0x20: case 0x30:  // ROL RCL SHL (SAL)
			m_OverVal = top != (m_CarryVal != 0);
			break;
		case 0x08: case 0x18:                        // ROR RCR
			m_OverVal = top != next;
			break;
		case 0x28:                                   // SHR
			m_OverVal = (count == 1) ? (m_src & msb) : 0;
			break;
		case 0x38:                                   // SAR
			m_OverVal = 0;
			break;
		}
	}

	// ARIA: framework hooks reduced to what the core needs.
	bool access_to_be_redone() const { return false; }  // No wait-stated I/O.
	void debugger_instruction_hook(uint32_t) {}
	void debugger_wait_hook() {}
	void debugger_exception_hook(int) {}
	uint64_t total_cycles() const { return 1; }  // Only "not at reset" matters.
	// INTA cycle. The 80186 overrides it with its interrupt controller.
	virtual int standard_irq_callback(int, uint32_t) { return bus_->AcknowledgeInterrupt(); }
	void set_input_line(int line, int state) { execute_set_input(line, state); }

	union
	{                   /* eight general registers */
		uint16_t w[8];    /* viewed as 16 bits registers */
		uint8_t  b[16];   /* or as 8 bit registers */
	} m_regs;

	enum BREGS {
		AL = NATIVE_ENDIAN_VALUE_LE_BE(0x0, 0x1),
		AH = NATIVE_ENDIAN_VALUE_LE_BE(0x1, 0x0),
		CL = NATIVE_ENDIAN_VALUE_LE_BE(0x2, 0x3),
		CH = NATIVE_ENDIAN_VALUE_LE_BE(0x3, 0x2),
		DL = NATIVE_ENDIAN_VALUE_LE_BE(0x4, 0x5),
		DH = NATIVE_ENDIAN_VALUE_LE_BE(0x5, 0x4),
		BL = NATIVE_ENDIAN_VALUE_LE_BE(0x6, 0x7),
		BH = NATIVE_ENDIAN_VALUE_LE_BE(0x7, 0x6),
		SPL = NATIVE_ENDIAN_VALUE_LE_BE(0x8, 0x9),
		SPH = NATIVE_ENDIAN_VALUE_LE_BE(0x9, 0x8),
		BPL = NATIVE_ENDIAN_VALUE_LE_BE(0xa, 0xb),
		BPH = NATIVE_ENDIAN_VALUE_LE_BE(0xb, 0xa),
		SIL = NATIVE_ENDIAN_VALUE_LE_BE(0xc, 0xd),
		SIH = NATIVE_ENDIAN_VALUE_LE_BE(0xd, 0xc),
		DIL = NATIVE_ENDIAN_VALUE_LE_BE(0xe, 0xf),
		DIH = NATIVE_ENDIAN_VALUE_LE_BE(0xf, 0xe)
	};

	enum {
		I8086_READ,
		I8086_WRITE,
		I8086_FETCH,
		I8086_NONE
	};

	uint16_t  m_sregs[4];

	uint16_t  m_ip;
	uint16_t  m_prev_ip;
	bool      m_io_stall = false;   // I/O wait-state (IOCHRDY) restart pending

	int32_t   m_SignVal;
	uint32_t  m_AuxVal, m_OverVal, m_ZeroVal, m_CarryVal, m_ParityVal; /* 0 or non-0 valued flags */
	uint8_t   m_TF, m_IF, m_DF;     /* 0 or 1 valued flags */
	uint8_t   m_IOPL, m_NT, m_MF;
	uint32_t  m_int_vector;
	uint32_t  m_pending_irq;
	uint32_t  m_nmi_state;
	uint8_t   m_no_interrupt;
	uint8_t   m_fire_trap;
	uint8_t   m_test_state;

	// ARIA: the bus and MAME-style views onto it.
	I86Bus* bus_ = nullptr;
	AddressSpace m_program_space{bus_, false};
	AddressSpace m_io_space{bus_, true};
	AddressSpace* m_program = &m_program_space;
	AddressSpace* m_opcodes = &m_program_space;
	AddressSpace* m_io = &m_io_space;

	int m_icount;

	uint32_t m_prefix_seg;   /* the latest prefix segment */
	bool m_seg_prefix;      /* prefix segment indicator */
	bool m_seg_prefix_next; /* prefix segment for next instruction */

	uint32_t m_ea;
	uint16_t m_eo;
	int m_easeg;
	// ARIA: physical base of the segment of the last calc_addr(). A word at
	// offset FFFFh takes its second byte from offset 0000h of the same
	// segment, not from the next physical address as MAME had it; found by
	// SingleStepTests 8088 (POP with SP=FFFFh, LDS at offset FFFFh).
	// HANDOFF 6.12.
	uint32_t m_ea_base = 0;
	uint32_t SegmentNext(uint32_t addr) const { return m_ea_base + (((addr - m_ea_base) + 1) & 0xffff); }

	// Used during execution of instructions
	uint8_t   m_modrm;
	uint32_t  m_dst;
	uint32_t  m_src;
	uint32_t  m_pc;

	// Lookup tables
	uint8_t m_parity_table[256];
	struct {
		struct {
			int w[256];
			int b[256];
		} reg;
		struct {
			int w[256];
			int b[256];
		} RM;
	} m_Mod_RM;

	uint8_t m_timing[200];
	uint8_t m_ea_timing[200];
	bool m_halt;

	bool m_lock;
	LineCallback m_lock_handler;
};

class i80186_cpu_device : public i8086_common_cpu_device
{
protected:
	// ARIA: replaces the MAME device constructor; 80188 only.
	i80186_cpu_device();
	i80186_cpu_device(const i80186_cpu_device&) = delete;
	i80186_cpu_device& operator=(const i80186_cpu_device&) = delete;

	void execute_run();
	void device_reset();

	virtual uint8_t fetch() override;
	uint32_t update_pc() { return m_pc = (m_sregs[CS] << 4) + m_ip; }

	virtual uint8_t read_port_byte(uint16_t port) override;
	virtual uint16_t read_port_word(uint16_t port) override;
	virtual void write_port_byte(uint16_t port, uint8_t data) override;
	virtual void write_port_byte_al(uint16_t port) override;
	virtual void write_port_word(uint16_t port, uint16_t data) override;
	virtual uint8_t read_byte(uint32_t addr) override;
	virtual uint16_t read_word(uint32_t addr) override;
	virtual void write_byte(uint32_t addr, uint8_t data) override;
	virtual void write_word(uint32_t addr, uint16_t data) override;

	static const uint8_t m_i80186_timing[200];

	// ARIA: stop after one whole instruction (prefixes included), for the
	// instruction tests and for stepping.
	bool m_single_step = false;

	// ARIA: opcode fetch, MAME's opcode cache reduced to the bus.
	uint8_t m_or8(uint32_t address) { return m_opcodes->read_byte(address); }

	// ---- PCB peripherals, MAME's functions (i186.cpp) ----
	// ARIA: MAME's IRQ_CALLBACK_MEMBER / TIMER_CALLBACK_MEMBER signatures.
#define IRQ_CALLBACK_MEMBER(name) int name(i8086_common_cpu_device &device, int irqline)
#define TIMER_CALLBACK_MEMBER(name) void name(int param)
	IRQ_CALLBACK_MEMBER(int_callback);
	IRQ_CALLBACK_MEMBER(inta_callback);
	virtual int standard_irq_callback(int, uint32_t) override { return inta_callback(*this, 0); }

	void update_interrupt_state();
	void handle_eoi(int data);
	void external_int(uint16_t intno, int state);
	void restart_timer(int which);
	void internal_timer_sync(int which);
	void internal_timer_update(int which, int new_count, int new_maxA, int new_maxB, int new_control);
	void external_tmrin(int which, int state);
	void update_dma_control(int which, int new_control);
	void drq_callback(int which);
	void inc_timer(int which);
	uint16_t internal_port_r(offs_t offset, uint16_t mem_mask = static_cast<uint16_t>(~0));
	void internal_port_w(offs_t offset, uint16_t data);
	TIMER_CALLBACK_MEMBER(timer_elapsed);

	// ARIA: CPU cycle clock for the timers. m_icount counts down within one
	// execute_run(); m_clock_base and m_icount_base turn that into an
	// absolute cycle number. While a timer fires, the clock reads its due
	// time, so that a periodic timer reprimes without drift.
	int64_t m_clock = 0;           // Absolute cycle, see UpdateClock().
	int64_t m_clock_base = 0;
	int m_icount_base = 0;
	void UpdateClock() { m_clock = m_clock_base + (m_icount_base - m_icount); }
	void BeginRun(int cycles) { m_icount = m_icount_base = cycles; UpdateClock(); }
	void EndRun() { m_clock_base += m_icount_base - m_icount; m_icount_base = m_icount; UpdateClock(); }
	attotime cycles_to_attotime(int64_t cycles) const { return attotime{cycles}; }
	static int64_t attotime_to_cycles(attotime time) { return time.cycles; }
	emu_timer m_int_timers[3];
	// Fires the timers that are due; returns true when any fired.
	bool ServiceTimers();
	// HLT: skip ahead to the next timer event if it falls within the
	// current budget; returns false when nothing is due before the end.
	bool HaltUntilTimer();

	// ARIA: MAME's devcb callbacks, routed to I86Bus.
	struct SlaveAck {
		I86Bus*& bus;
		uint8_t operator()(int) const { return bus->AcknowledgeInterrupt(); }
	};
	struct TimerOut {
		I86Bus*& bus;
		int timer;
		void operator()(int state) const { bus->TimerOutput(timer, state); }
	};
	struct ChipSelectOut {
		I86Bus*& bus;
		void operator()(int which, uint16_t value, uint16_t) const { bus->ChipSelect(which, value); }
	};
	struct IrqAck {
		int operator()(i8086_common_cpu_device&, int) const { return 0; }
	};
	SlaveAck m_read_slave_ack_func{bus_};
	ChipSelectOut m_out_chip_select_func{bus_};
	TimerOut m_out_tmrout0_func{bus_, 0};
	TimerOut m_out_tmrout1_func{bus_, 1};
	LineCallback m_irmx_irq_cb;
	LineCallback m_irqa_cb;
	IrqAck m_irmx_irq_ack;

	struct mem_state
	{
		uint16_t    lower;
		uint16_t    upper;
		uint16_t    middle;
		uint16_t    middle_size;
		uint16_t    peripheral;
	};

	struct timer_state
	{
		uint16_t    control;
		uint16_t    maxA;
		uint16_t    maxB;
		uint16_t    count;
		emu_timer   *int_timer;
	};

	struct dma_state
	{
		bool        drq_state;
		uint32_t    source;
		uint32_t    dest;
		uint16_t    count;
		uint16_t    control;
	};

	struct intr_state
	{
		uint8_t     vector;
		uint8_t     pending;
		uint16_t    ack_mask;
		uint16_t    priority_mask;
		uint16_t    in_service;
		uint16_t    request;
		uint16_t    status;
		uint16_t    poll_status;
		uint16_t    timer[3];
		uint16_t    dma[2];
		uint16_t    ext[4];
		uint8_t     ext_state;
	};

	timer_state     m_timer[3];
	dma_state       m_dma[2];
	intr_state      m_intr;
	mem_state       m_mem;
	bool            m_last_dma = false;
	int m_dma_latency = 0;

	uint16_t m_reloc;
};

}  // namespace mame_i86

#endif
