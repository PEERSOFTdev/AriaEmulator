// license:BSD-3-Clause
// copyright-holders:Aaron Giles
//
// ARIA: declarations for the MAME ADSP-21xx core (LICENSE.mame-adsp2100.txt),
// rewritten so that the instruction code in 2100ops.hxx and adsp2100.cpp
// compiles verbatim without the MAME framework. Member names and constants
// are MAME's; the device, address-space, callback, debugger and save-state
// plumbing is replaced by the shims below and by the DspBus interface. Only
// the ADSP-2101 family path is kept (the Aria has an ADSP-2105, HANDOFF
// 6.3); the 2181 members stay where the shared code names them. As in MAME,
// the on-chip memories and control registers (SPORT, timer, boot) are not
// part of the core: the board model provides them through DspBus.

#ifndef ARIA_ADSP2100_ADSP2100_H
#define ARIA_ADSP2100_ADSP2100_H

#include <cstdint>
#include <cstdio>
#include <cstring>

// From MAME's adsp2100.h: no hotspot profiling.
#define ADSP_TRACK_HOTSPOTS     0

namespace mame_adsp {

// ARIA: the buses and pins the core reaches. Program memory words are 24
// bits, data memory words 16 bits, both with 14-bit word addresses.
class DspBus {
 public:
  virtual ~DspBus() = default;
  virtual uint32_t ReadProgram(uint32_t address) = 0;
  virtual void WriteProgram(uint32_t address, uint32_t data) = 0;
  virtual uint16_t ReadData(uint32_t address) = 0;
  virtual void WriteData(uint32_t address, uint16_t data) = 0;
  // TX0/TX1 and RX0/RX1 register accesses (2100ops.hxx write_reg3/read_reg3).
  virtual void SportTransmit(int port, uint16_t data) {}
  virtual uint16_t SportReceive(int port) { return 0; }
  // MSTAT bit 5 changed (2100ops.hxx update_mstat).
  virtual void TimerEnable(bool enabled) {}
  // MAME's logerror: writes to invalid registers and the like.
  virtual void Log(const char* text) {}
};

// ---- ARIA: shims for the MAME framework names the verbatim code uses ----

constexpr int CLEAR_LINE = 0;
constexpr int ASSERT_LINE = 1;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using s16 = int16_t;
using s32 = int32_t;

template <typename T>
constexpr T BIT(T x, int n) { return (x >> n) & T(1); }
template <typename T>
constexpr T BIT(T x, int n, int w) { return (x >> n) & ((T(1) << w) - 1); }

namespace util {
template <typename T>
constexpr int32_t sext(T value, int width) {
  const uint32_t mask = (uint32_t(1) << width) - 1;
  const uint32_t v = uint32_t(value) & mask;
  return int32_t(v ^ (uint32_t(1) << (width - 1))) - int32_t(uint32_t(1) << (width - 1));
}
}  // namespace util

// Stand-ins for memory_access<>::specific and ::cache.
struct ProgramSpace {
  DspBus*& bus;
  uint32_t read_dword(uint32_t address) { return bus->ReadProgram(address & 0x3fff); }
  void write_dword(uint32_t address, uint32_t data) { bus->WriteProgram(address & 0x3fff, data); }
};
struct DataSpace {
  DspBus*& bus;
  uint16_t read_word(uint32_t address) { return bus->ReadData(address & 0x3fff); }
  void write_word(uint32_t address, uint16_t data) { bus->WriteData(address & 0x3fff, data); }
};
// The 2181's I/O space; the 2105 has none (adsp2100.cpp checks the chip type).
struct NoSpace {
  uint16_t read_word(uint32_t) { return 0; }
  void write_word(uint32_t, uint16_t) {}
};

// Stand-ins for the devcb callbacks.
struct SportTxCallback {
  DspBus*& bus;
  void operator()(int port, uint32_t data, uint32_t /*mask*/) { bus->SportTransmit(port, uint16_t(data)); }
};
struct SportRxCallback {
  DspBus*& bus;
  uint32_t operator()(int port) { return bus->SportReceive(port); }
};
struct TimerCallback {
  DspBus*& bus;
  void operator()(bool enabled) { bus->TimerEnable(enabled); }
};
struct DmovlayCallback {
  void operator()(uint32_t) {}  // 2181 only.
};

// ---- verbatim from adsp2100.h lines 27-89 ----
// ADSP-2100 IRQs
const int ADSP2100_IRQ0         = 0;        // IRQ0
const int ADSP2100_SPORT1_RX    = 0;        // SPORT1 receive IRQ
const int ADSP2100_IRQ1         = 1;        // IRQ1
const int ADSP2100_SPORT1_TX    = 1;        // SPORT1 transmit IRQ
const int ADSP2100_IRQ2         = 2;        // IRQ2
const int ADSP2100_IRQ3         = 3;        // IRQ3

// ADSP-2101 IRQs
const int ADSP2101_IRQ0         = 0;        // IRQ0
const int ADSP2101_SPORT1_RX    = 0;        // SPORT1 receive IRQ
const int ADSP2101_IRQ1         = 1;        // IRQ1
const int ADSP2101_SPORT1_TX    = 1;        // SPORT1 transmit IRQ
const int ADSP2101_IRQ2         = 2;        // IRQ2
const int ADSP2101_SPORT0_RX    = 3;        // SPORT0 receive IRQ
const int ADSP2101_SPORT0_TX    = 4;        // SPORT0 transmit IRQ
const int ADSP2101_TIMER        = 5;        // internal timer IRQ

// ADSP-2104 IRQs
const int ADSP2104_IRQ0         = 0;        // IRQ0
const int ADSP2104_SPORT1_RX    = 0;        // SPORT1 receive IRQ
const int ADSP2104_IRQ1         = 1;        // IRQ1
const int ADSP2104_SPORT1_TX    = 1;        // SPORT1 transmit IRQ
const int ADSP2104_IRQ2         = 2;        // IRQ2
const int ADSP2104_SPORT0_RX    = 3;        // SPORT0 receive IRQ
const int ADSP2104_SPORT0_TX    = 4;        // SPORT0 transmit IRQ
const int ADSP2104_TIMER        = 5;        // internal timer IRQ

// ADSP-2105 IRQs
const int ADSP2105_IRQ0         = 0;        // IRQ0
const int ADSP2105_SPORT1_RX    = 0;        // SPORT1 receive IRQ
const int ADSP2105_IRQ1         = 1;        // IRQ1
const int ADSP2105_SPORT1_TX    = 1;        // SPORT1 transmit IRQ
const int ADSP2105_IRQ2         = 2;        // IRQ2
const int ADSP2105_TIMER        = 5;        // internal timer IRQ

// ADSP-2115 IRQs
const int ADSP2115_IRQ0         = 0;        // IRQ0
const int ADSP2115_SPORT1_RX    = 0;        // SPORT1 receive IRQ
const int ADSP2115_IRQ1         = 1;        // IRQ1
const int ADSP2115_SPORT1_TX    = 1;        // SPORT1 transmit IRQ
const int ADSP2115_IRQ2         = 2;        // IRQ2
const int ADSP2115_SPORT0_RX    = 3;        // SPORT0 receive IRQ
const int ADSP2115_SPORT0_TX    = 4;        // SPORT0 transmit IRQ
const int ADSP2115_TIMER        = 5;        // internal timer IRQ

// ADSP-2181 IRQs
const int ADSP2181_IRQ0         = 0;        // IRQ0
const int ADSP2181_SPORT1_RX    = 0;        // SPORT1 receive IRQ
const int ADSP2181_IRQ1         = 1;        // IRQ1
const int ADSP2181_SPORT1_TX    = 1;        // SPORT1 transmit IRQ
const int ADSP2181_IRQ2         = 2;        // IRQ2
const int ADSP2181_SPORT0_RX    = 3;        // SPORT0 receive IRQ
const int ADSP2181_SPORT0_TX    = 4;        // SPORT0 transmit IRQ
const int ADSP2181_TIMER        = 5;        // internal timer IRQ
const int ADSP2181_IRQE         = 6;        // IRQE
const int ADSP2181_BDMA         = 7;        // BDMA
const int ADSP2181_IRQL1        = 8;        // IRQL1
const int ADSP2181_IRQL0        = 9;        // IRQL0

// ARIA: MAME's adsp21xx_device without the framework (declarations from
// adsp2100.h lines 183-484, framework members replaced as noted).
class adsp21xx_device
{
public:
	virtual ~adsp21xx_device() = default;

	// Returns base address for circular dag
	uint32_t get_ibase(int index) { return m_base[index]; }

protected:
	enum
	{
		CHIP_TYPE_ADSP2100,
		CHIP_TYPE_ADSP2101,
		CHIP_TYPE_ADSP2104,
		CHIP_TYPE_ADSP2105,
		CHIP_TYPE_ADSP2115,
		CHIP_TYPE_ADSP2181
	};

	// ARIA: no machine_config, tag, owner or clock.
	explicit adsp21xx_device(uint32_t chiptype);

	void device_reset();
	void execute_run();
	void execute_set_input(int inputnum, int state);
	// ARIA: for SPORT autobuffering in the board model (end of adsp2100.cpp).
	uint16_t autobuffer_read(int ireg, int mreg, bool &wrapped);

	// ARIA: MAME's logerror, routed to DspBus::Log.
	template <typename... Args>
	void logerror(const char* format, Args... args)
	{
		char text[160];
		std::snprintf(text, sizeof text, format, args...);
		m_bus->Log(text);
	}

	// helpers
	void create_tables();
	inline void update_mstat();
	inline uint32_t pc_stack_top();
	inline void set_pc_stack_top(uint32_t top);
	inline void pc_stack_push();
	inline void pc_stack_push_val(uint32_t val);
	inline void pc_stack_pop();
	inline uint32_t pc_stack_pop_val();
	inline uint32_t cntr_stack_top();
	inline void cntr_stack_push();
	inline void cntr_stack_pop();
	inline uint32_t loop_stack_top();
	inline void loop_stack_push(uint32_t value);
	inline void loop_stack_pop();
	inline void stat_stack_push();
	inline void stat_stack_pop();
	int slow_condition();
	inline void modify_address(uint32_t ireg, uint32_t mreg);
	inline void data_write_dag1(uint32_t op, int32_t val);
	inline uint32_t data_read_dag1(uint32_t op);
	inline void data_write_dag2(uint32_t op, int32_t val);
	inline uint32_t data_read_dag2(uint32_t op);
	inline void pgm_write_dag2(uint32_t op, int32_t val);
	inline uint32_t pgm_read_dag2(uint32_t op);
	void alu_op_ar(uint32_t op);
	void alu_op_ar_const(uint32_t op);
	void alu_op_af(uint32_t op);
	void alu_op_af_const(uint32_t op);
	void alu_op_none(uint32_t op);
	void mac_op_mr(uint32_t op);
	void mac_op_mr_xop(uint32_t op);
	void mac_op_mf(uint32_t op);
	void mac_op_mf_xop(uint32_t op);
	void shift_op(uint32_t op);
	void shift_op_imm(uint32_t op);

	// memory access
	inline uint16_t data_read(uint32_t addr);
	inline void data_write(uint32_t addr, uint16_t data);
	inline uint16_t io_read(uint32_t addr);
	inline void io_write(uint32_t addr, uint16_t data);
	inline uint32_t program_read(uint32_t addr);
	inline void program_write(uint32_t addr, uint32_t data);
	inline uint32_t opcode_read();

	// register read/write
	inline void update_i(int which);
	inline void update_l(int which);
	inline void update_dmovlay();
	void write_reg0(int regnum, int32_t val);
	void write_reg1(int regnum, int32_t val);
	void write_reg2(int regnum, int32_t val);
	void write_reg3(int regnum, int32_t val);
	int32_t read_reg0(int regnum);
	int32_t read_reg1(int regnum);
	int32_t read_reg2(int regnum);
	int32_t read_reg3(int regnum);

	// interrupts
	virtual bool generate_irq(int which, int indx = 0) = 0;
	virtual void check_irqs() = 0;

	// internal state
	static const int PC_STACK_DEPTH     = 16;
	static const int CNTR_STACK_DEPTH   = 4;
	static const int STAT_STACK_DEPTH   = 4;
	static const int LOOP_STACK_DEPTH   = 4;

	// 16-bit registers that can be loaded signed or unsigned
	union adsp_reg16
	{
		uint16_t  u;
		int16_t   s;
	};

	// ARIA: MAME picks the field order with LSB_FIRST; the Windows targets
	// are little-endian.
	// the SHIFT result register is 32 bits
	union adsp_shift
	{
		struct { adsp_reg16 sr0, sr1; } srx;
		uint32_t sr;
	};

	// the MAC result register is 40 bits
	union adsp_mac
	{
		struct { adsp_reg16 mr0, mr1, mr2, mrzero; } mrx;
		struct { uint32_t mr0, mr1; } mry;
		uint64_t mr;
	};

	// core registers which are replicated
	struct adsp_core
	{
		// ALU registers
		adsp_reg16  ax0, ax1;
		adsp_reg16  ay0, ay1;
		adsp_reg16  ar;
		adsp_reg16  af;

		// MAC registers
		adsp_reg16  mx0, mx1;
		adsp_reg16  my0, my1;
		adsp_mac    mr;
		adsp_reg16  mf;

		// SHIFT registers
		adsp_reg16  si;
		adsp_reg16  se;
		adsp_reg16  sb;
		adsp_shift  sr;

		// dummy registers
		adsp_reg16  zero;
	};

	// ARIA: the bus behind the address spaces and callbacks below.
	DspBus*             m_bus = nullptr;

	// configuration
	uint32_t            m_chip_type;

	// other CPU registers
	uint32_t            m_pc;
	uint32_t            m_ppc;
	uint32_t            m_loop;
	uint32_t            m_loop_condition;
	uint32_t            m_cntr;

	// status registers
	uint32_t            m_astat;
	uint32_t            m_sstat;
	uint32_t            m_mstat;
	uint32_t            m_mstat_prev;
	uint32_t            m_astat_clear;
	uint32_t            m_idle;

	// live set of core registers
	adsp_core           m_core;

	// memory addressing registers
	uint32_t            m_i[8];
	int32_t             m_m[8];
	uint32_t            m_l[8];
	uint32_t            m_lmask[8];
	uint32_t            m_base[8];
	uint8_t             m_px;
	uint32_t            m_pmovlay; // External Program Space overlay
	uint32_t            m_dmovlay; // External Data Space overlay

	// stacks
	uint32_t            m_loop_stack[LOOP_STACK_DEPTH];
	uint32_t            m_cntr_stack[CNTR_STACK_DEPTH];
	uint32_t            m_pc_stack[PC_STACK_DEPTH];
	uint16_t            m_stat_stack[STAT_STACK_DEPTH][3];
	int32_t             m_pc_sp;
	int32_t             m_cntr_sp;
	int32_t             m_stat_sp;
	int32_t             m_loop_sp;

	// external I/O
	uint8_t             m_flagout;
	uint8_t             m_flagin;
	uint8_t             m_fl0;
	uint8_t             m_fl1;
	uint8_t             m_fl2;
	uint16_t            m_idma_addr;
	uint16_t            m_idma_cache;
	uint8_t             m_idma_offs;

	// interrupt handling
	uint16_t            m_imask;
	uint8_t             m_icntl;
	uint16_t            m_ifc;
	uint8_t             m_irq_state[10];
	uint8_t             m_irq_latch[10];

	// other internal states
	int                 m_icount;
	int                 m_idle_icount = 0;  // ARIA: cycles left when IDLE ended the slice.
	int                 m_mstat_mask;
	int                 m_imask_mask;

	// register maps
	int16_t *           m_read0_ptr[16];
	uint32_t *          m_read1_ptr[16];
	uint32_t *          m_read2_ptr[16];
	void *              m_alu_xregs[8];
	void *              m_alu_yregs[4];
	void *              m_mac_xregs[8];
	void *              m_mac_yregs[4];
	void *              m_shift_xregs[8];

	// alternate core registers (at end for performance)
	adsp_core           m_alt;

	// address spaces (ARIA: shims over DspBus)
	ProgramSpace        m_cache{m_bus};
	ProgramSpace        m_program{m_bus};
	DataSpace           m_data{m_bus};
	NoSpace             m_io;

	// tables
	uint8_t               m_condition_table[0x1000];
	uint16_t              m_mask_table[0x4000];
	uint16_t              m_reverse_table[0x4000];

	// ARIA: callbacks as shims over DspBus.
	SportRxCallback       m_sport_rx_cb{m_bus};    // callback for serial receive
	SportTxCallback       m_sport_tx_cb{m_bus};    // callback for serial transmit
	TimerCallback         m_timer_fired_cb{m_bus}; // callback for timer fired
	DmovlayCallback       m_dmovlay_cb;            // callback for DMOVLAY instruction

	// flag definitions
	static const int SSFLAG     = 0x80;
	static const int MVFLAG     = 0x40;
	static const int QFLAG      = 0x20;
	static const int SFLAG      = 0x10;
	static const int CFLAG      = 0x08;
	static const int VFLAG      = 0x04;
	static const int NFLAG      = 0x02;
	static const int ZFLAG      = 0x01;
};

// ARIA: MAME's adsp2101_device (also the 2105, which MAME derives from it).
class adsp2101_device : public adsp21xx_device
{
protected:
	explicit adsp2101_device(uint32_t chiptype) : adsp21xx_device(chiptype) { }

	// interrupts
	bool generate_irq(int which, int indx) override;
	void check_irqs() override;
};

}  // namespace mame_adsp

#endif
