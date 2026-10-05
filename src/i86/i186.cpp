// license:BSD-3-Clause
// copyright-holders:Carl
// Peripheral code from rmnimbus driver by Phill Harvey-Smith which is
// based on the Leland sound driver by Aaron Giles and Paul Leaman
//
// ARIA: assembled from MAME src/devices/cpu/i86/i186.cpp at commit
// 2aa75c509dca2bebd8cbb3c0e27b9e75d3dcb597 (LICENSE.mame-i86.txt). The
// parts below the marker lines are copied verbatim, except where an
// "ARIA:" comment says otherwise. Left out for now: the PCB peripherals
// (interrupt controller, timers, DMA, chip selects) apart from the
// relocation register; their state is kept so execute_run() stays
// verbatim, and they follow in a later step (HANDOFF 6.12).

#include "i86/i86.h"

namespace mame_i86 {

#include "i86inline.h"

#define LOGMASKED(mask, ...) logerror(__VA_ARGS__)

// ---- verbatim from i186.cpp lines 27-56 ----
/* external int priority masks */

#define EXTINT_CTRL_PRI_MASK    0x07
#define EXTINT_CTRL_MSK         0x08
#define EXTINT_CTRL_LTM         0x10
#define EXTINT_CTRL_CASCADE     0x20
#define EXTINT_CTRL_SFNM        0x40

/* DMA control register */

#define DEST_MIO                0x8000
#define DEST_DECREMENT          0x4000
#define DEST_INCREMENT          0x2000
#define DEST_NO_CHANGE          (DEST_DECREMENT | DEST_INCREMENT)
#define DEST_INCDEC_MASK        (DEST_DECREMENT | DEST_INCREMENT)
#define SRC_MIO                 0x1000
#define SRC_DECREMENT           0x0800
#define SRC_INCREMENT           0x0400
#define SRC_NO_CHANGE           (SRC_DECREMENT | SRC_INCREMENT)
#define SRC_INCDEC_MASK         (SRC_DECREMENT | SRC_INCREMENT)
#define TERMINATE_ON_ZERO       0x0200
#define INTERRUPT_ON_ZERO       0x0100
#define SYNC_MASK               0x00C0
#define SYNC_SOURCE             0x0040
#define SYNC_DEST               0x0080
#define CHANNEL_PRIORITY        0x0020
#define TIMER_DRQ               0x0010
#define CHG_NOCHG               0x0004
#define ST_STOP                 0x0002
#define BYTE_WORD               0x0001

// ---- verbatim from i186.cpp lines 58-129 ----
/* these come from the Intel 80186 datasheet */
const uint8_t i80186_cpu_device::m_i80186_timing[] =
{
	45,28,          /* exception, IRET */
		0, 2, 4, 3, /* INTs */
		2,              /* segment overrides */
		2, 2, 3,        /* flag operations */
		8, 7,19,15, /* arithmetic adjusts */
		4, 4,           /* decimal adjusts */
		2, 4,           /* sign extension */
		2,18, 6, 2, 6,11,   /* misc */

	14,14,14,       /* direct JMPs */
	11,17,26,       /* indirect JMPs */
	15,23,          /* direct CALLs */
	13,19,38,       /* indirect CALLs */
	16,22,18,25,    /* returns */
		4,13, 5,15, /* conditional JMPs */
		6,16, 6,16, /* loops */

	10,10, 8, 8,    /* port reads */
		9, 9, 7, 7, /* port writes */

		2, 9,12,        /* move, 8-bit */
		3,12,           /* move, 8-bit immediate */
		2, 9,12,        /* move, 16-bit */
		4,13,           /* move, 16-bit immediate */
		8, 8, 9, 9, /* move, AL/AX memory */
		2,11, 2,11, /* move, segment registers */
		4,17,           /* exchange, 8-bit */
		4,17, 3,        /* exchange, 16-bit */

	10,16, 9, 9,    /* pushes */
	10,20, 8, 8,    /* pops */

		3,10,10,        /* ALU ops, 8-bit */
		4,16,10,        /* ALU ops, 8-bit immediate */
		3,10,10,        /* ALU ops, 16-bit */
		4,16,10,        /* ALU ops, 16-bit immediate */
		4,16,10,        /* ALU ops, 16-bit w/8-bit immediate */
	26,35,32,41,    /* MUL */
	25,34,31,40,    /* IMUL */
	29,38,35,44,    /* DIV */
	44,53,50,59,    /* IDIV */
		3, 3,15,15, /* INC/DEC */
		3, 3,10,10, /* NEG/NOT */

		2, 5, 1,        /* reg shift/rotate */
	15,17, 1,       /* m8 shift/rotate */
	15,17, 1,       /* m16 shift/rotate */

	22, 5,22,       /* CMPS 8-bit */
	22, 5,22,       /* CMPS 16-bit */
	15, 5,15,       /* SCAS 8-bit */
	15, 5,15,       /* SCAS 16-bit */
	12, 6,11,       /* LODS 8-bit */
	12, 6,11,       /* LODS 16-bit */
	10, 6, 9,       /* STOS 8-bit */
	10, 6, 9,       /* STOS 16-bit */
	14, 8, 8,       /* MOVS 8-bit */
	14, 8, 8,       /* MOVS 16-bit */

	14, 8, 8,       /* (80186) INS 8-bit */
	14, 8, 8,       /* (80186) INS 16-bit */
	14, 8, 8,       /* (80186) OUTS 8-bit */
	14, 8, 8,       /* (80186) OUTS 16-bit */
	14,68,83,       /* (80186) PUSH immediate, PUSHA/POPA */
	22,29,          /* (80186) IMUL immediate 8-bit */
	25,32,          /* (80186) IMUL immediate 16-bit */
	15,25,4,16, 8,  /* (80186) ENTER/LEAVE */
	33,             /* (80186) BOUND */
};

// ARIA: MAME's constructor took the device configuration and bus width;
// this is the 80188 one with the same timing setup (i186.cpp 169-172).
i80186_cpu_device::i80186_cpu_device()
{
	memcpy(m_timing, m_i80186_timing, sizeof(m_i80186_timing));
	// The Effective Address calculation times are already included in the
	// instruction timings for the 80186, so fill the table with zeros.
	memset(m_ea_timing, 0, sizeof(m_ea_timing));
}

// ---- verbatim from i186.cpp lines 191-196 ----
uint8_t i80186_cpu_device::fetch()
{
	uint8_t data = m_or8(update_pc());
	m_ip++;
	return data;
}

// ---- verbatim from i186.cpp lines 198-674 ----
void i80186_cpu_device::execute_run()
{
	while (m_icount > 0)
	{
		if ((m_dma[0].drq_state && (m_dma[0].control & ST_STOP)) || (m_dma[1].drq_state && (m_dma[1].control & ST_STOP)))
		{
			// A real '186 has some latency after writing to the DMA registers before a transfer will fire.
			// mpc60 has a continuous RAM 0 to RAM 0 transfer running on channel 0 for unknown reasons, and
			// without this latency a transfer can fire between the IRQ handler resetting the source address
			// and destination address, resulting in the DMA turning from an expensive NOP into a memmove.
			if (!m_dma_latency)
			{
				int channel = m_last_dma ? 0 : 1;
				m_last_dma = !m_last_dma;
				if (!(m_dma[1].drq_state && (m_dma[1].control & ST_STOP)))
					channel = 0;
				else if (!(m_dma[0].drq_state && (m_dma[0].control & ST_STOP)))
					channel = 1;
				else if ((m_dma[0].control & CHANNEL_PRIORITY) && !(m_dma[1].control & CHANNEL_PRIORITY))
					channel = 0;
				else if ((m_dma[1].control & CHANNEL_PRIORITY) && !(m_dma[0].control & CHANNEL_PRIORITY))
					channel = 1;
				m_icount--;
				drq_callback(channel);
				continue;
			}
		}
		if (m_dma_latency > 0)
		{
			m_dma_latency--;
		}
		if (m_seg_prefix_next)
		{
			m_seg_prefix = true;
			m_seg_prefix_next = false;
		}
		else
		{
			m_prev_ip = m_ip;
			m_seg_prefix = false;

			/* Dispatch IRQ */
			if (m_pending_irq && m_no_interrupt == 0)
			{
				if (m_pending_irq & NMI_IRQ)
				{
					interrupt(2);
					m_pending_irq &= ~NMI_IRQ;
					m_halt = false;
				}
				else if (m_IF)
				{
					/* the actual vector is retrieved after pushing flags */
					/* and clearing the IF */
					interrupt(-1);
					m_halt = false;
				}
			}

			if (m_halt)
			{
				debugger_wait_hook();
				m_icount = 0;
				return;
			}

			/* No interrupt allowed between last instruction and this one */
			if (m_no_interrupt)
			{
				m_no_interrupt--;
			}

			/* trap should allow one instruction to be executed */
			if (m_fire_trap)
			{
				if (m_fire_trap >= 2)
				{
					interrupt(1);
					m_fire_trap = 0;
				}
				else
				{
					m_fire_trap++;
				}
			}
		}

		debugger_instruction_hook( update_pc() );

		uint8_t op = fetch_op();

		switch(op)
		{
			case 0x60: // i_pusha
				{
					uint32_t tmp = m_regs.w[SP];

					PUSH(m_regs.w[AX]);
					PUSH(m_regs.w[CX]);
					PUSH(m_regs.w[DX]);
					PUSH(m_regs.w[BX]);
					PUSH(tmp);
					PUSH(m_regs.w[BP]);
					PUSH(m_regs.w[SI]);
					PUSH(m_regs.w[DI]);
					CLK(PUSHA);
				}
				break;

			case 0x61: // i_popa
				m_regs.w[DI] = POP();
				m_regs.w[SI] = POP();
				m_regs.w[BP] = POP();
				POP();
				m_regs.w[BX] = POP();
				m_regs.w[DX] = POP();
				m_regs.w[CX] = POP();
				m_regs.w[AX] = POP();
				CLK(POPA);
				break;

			case 0x62: // i_bound
				{
					m_modrm = fetch();
					int16_t low = (int16_t)GetRMWord();
					int16_t high = (int16_t)GetnextRMWord();
					int16_t idx = (int16_t)RegWord();
					if (idx < low || idx > high) {
						interrupt(5);
					}
					CLK(BOUND);
					logerror("%06x: bound %i <= %i <= %i\n", m_pc, low, idx, high);
				}
				break;

			case 0x68: // i_push_d16
				PUSH(fetch_word());
				CLK(PUSH_IMM);
				break;

			case 0x69: // i_imul_d16
				{
					DEF_r16w();
					uint32_t tmp = fetch_word();
					m_dst = (int32_t)((int16_t)m_src) * (int32_t)((int16_t)tmp);
					m_CarryVal = m_OverVal = (((int32_t)m_dst) >> 15 != 0) && (((int32_t)m_dst) >> 15 != -1);
					RegWord(m_dst);
					CLKM(IMUL_RRI16, IMUL_RMI16);
				}
				break;

			case 0x6a: // i_push_d8
				PUSH((uint16_t)((int16_t)((int8_t)fetch())));
				CLK(PUSH_IMM);
				break;

			case 0x6b: // i_imul_d8
				{
					DEF_r16w();
					uint32_t src2 = (uint16_t)(int8_t)fetch();
					m_dst = (int32_t)((int16_t)m_src) * (int32_t)((int16_t)src2);
					m_CarryVal = m_OverVal = (((int32_t)m_dst) >> 15 != 0) && (((int32_t)m_dst) >> 15 != -1);
					RegWord(m_dst);
					CLKM(IMUL_RRI8, IMUL_RMI8);
				}
				break;

			case 0x6c: // i_insb
				i_insb();
				break;

			case 0x6d: // i_insw
				i_insw();
				break;

			case 0x6e: // i_outsb
				i_outsb();
				break;

			case 0x6f: // i_outsw
				i_outsw();
				break;

			case 0x8e: // i_mov_sregw
				m_modrm = fetch();
				m_src = GetRMWord();
				CLKM(MOV_SR, MOV_SM);
				switch (m_modrm & 0x38)
				{
				case 0x00:  /* mov es,ew */
					m_sregs[ES] = m_src;
					break;
				case 0x10:  /* mov ss,ew */
					m_sregs[SS] = m_src;
					m_no_interrupt = 1;
					break;
				case 0x18:  /* mov ds,ew */
					m_sregs[DS] = m_src;
					break;
				default:
					logerror("%06x: Mov Sreg - Invalid register\n", m_pc);
					interrupt(6);
					break;
				}
				break;

			case 0xc0: // i_rotshft_bd8
				{
					m_modrm = fetch();
					m_src = GetRMByte();
					m_dst = m_src;
					uint8_t c = fetch() & 0x1f;
					const uint8_t count = c;  // ARIA: for SetShiftOverflow
					CLKM(ROT_REG_BASE, ROT_M8_BASE);
					m_icount -= m_timing[ROT_REG_BIT] * c;
					if (c)
					{
						switch (m_modrm & 0x38)
						{
						case 0x00: do { ROL_BYTE();  c--; } while (c > 0); PutbackRMByte(m_dst); break;
						case 0x08: do { ROR_BYTE();  c--; } while (c > 0); PutbackRMByte(m_dst); break;
						case 0x10: do { ROLC_BYTE(); c--; } while (c > 0); PutbackRMByte(m_dst); break;
						case 0x18: do { RORC_BYTE(); c--; } while (c > 0); PutbackRMByte(m_dst); break;
						case 0x30:
						case 0x20: SHL_BYTE(c); break;
						case 0x28: SHR_BYTE(c); break;
						case 0x38: SHRA_BYTE(c); break;
						}
						SetShiftOverflow(m_modrm & 0x38, count, false);  // ARIA
					}
				}
				break;

			case 0xc1: // i_rotshft_wd8
				{
					m_modrm = fetch();
					m_src = GetRMWord();
					m_dst = m_src;
					uint8_t c = fetch() & 0x1f;
					const uint8_t count = c;  // ARIA: for SetShiftOverflow
					CLKM(ROT_REG_BASE, ROT_M16_BASE);
					m_icount -= m_timing[ROT_REG_BIT] * c;
					if (c)
					{
						switch (m_modrm & 0x38)
						{
						case 0x00: do { ROL_WORD();  c--; } while (c > 0); PutbackRMWord(m_dst); break;
						case 0x08: do { ROR_WORD();  c--; } while (c > 0); PutbackRMWord(m_dst); break;
						case 0x10: do { ROLC_WORD(); c--; } while (c > 0); PutbackRMWord(m_dst); break;
						case 0x18: do { RORC_WORD(); c--; } while (c > 0); PutbackRMWord(m_dst); break;
						case 0x30:
						case 0x20: SHL_WORD(c); break;
						case 0x28: SHR_WORD(c); break;
						case 0x38: SHRA_WORD(c); break;
						}
						SetShiftOverflow(m_modrm & 0x38, count, true);  // ARIA
					}
				}
				break;

			case 0xc8: // i_enter
				{
					uint16_t nb = fetch();
					nb |= fetch() << 8;

					// ARIA: rewritten after Intel's ENTER algorithm. MAME set BP and
					// subtracted the frame size first and then copied the display
					// from below the new frame. Intel: push BP, remember the frame
					// pointer, copy level-1 pointers walking down the *old* BP, push
					// the frame pointer, then BP = frame, SP -= size; the level is
					// taken mod 32. SingleStepTests V20 C8: 1941 of 2000 failed
					// before. HANDOFF 6.12.
					uint32_t level = fetch() & 0x1f;
					CLK(!level ? ENTER0 : (level == 1) ? ENTER1 : ENTER_BASE);
					if (level > 1)
						m_icount -= level * m_timing[ENTER_COUNT];
					PUSH(m_regs.w[BP]);
					const uint16_t frame = m_regs.w[SP];
					if (level)
					{
						uint16_t old_bp = m_regs.w[BP];
						for (uint32_t i = 1; i < level; i++)
						{
							old_bp -= 2;
							PUSH(GetMemW(SS, old_bp));
						}
						PUSH(frame);
					}
					m_regs.w[BP] = frame;
					m_regs.w[SP] -= nb;
				}
				break;

			case 0xc9: // i_leave
				m_regs.w[SP] = m_regs.w[BP];
				m_regs.w[BP] = POP();
				CLK(LEAVE);
				break;

			case 0xd2: // i_rotshft_bcl
				{
					m_modrm = fetch();
					m_src = GetRMByte();
					m_dst = m_src;
					uint8_t c = m_regs.b[CL] & 0x1f;
					const uint8_t count = c;  // ARIA: for SetShiftOverflow
					CLKM(ROT_REG_BASE, ROT_M16_BASE);
					m_icount -= m_timing[ROT_REG_BIT] * c;
					if (c)
					{
						switch (m_modrm & 0x38)
						{
						case 0x00: do { ROL_BYTE();  c--; } while (c > 0); PutbackRMByte(m_dst); break;
						case 0x08: do { ROR_BYTE();  c--; } while (c > 0); PutbackRMByte(m_dst); break;
						case 0x10: do { ROLC_BYTE(); c--; } while (c > 0); PutbackRMByte(m_dst); break;
						case 0x18: do { RORC_BYTE(); c--; } while (c > 0); PutbackRMByte(m_dst); break;
						case 0x30:
						case 0x20: SHL_BYTE(c); break;
						case 0x28: SHR_BYTE(c); break;
						case 0x38: SHRA_BYTE(c); break;
						}
						SetShiftOverflow(m_modrm & 0x38, count, false);  // ARIA
					}
				}
				break;

			case 0xd3: // i_rotshft_wcl
				{
					m_modrm = fetch();
					m_src = GetRMWord();
					m_dst = m_src;
					uint8_t c = m_regs.b[CL] & 0x1f;
					const uint8_t count = c;  // ARIA: for SetShiftOverflow
					CLKM(ROT_REG_BASE, ROT_M16_BASE);
					m_icount -= m_timing[ROT_REG_BIT] * c;
					if (c)
					{
						switch (m_modrm & 0x38)
						{
							case 0x00: do { ROL_WORD();  c--; } while (c > 0); PutbackRMWord(m_dst); break;
							case 0x08: do { ROR_WORD();  c--; } while (c > 0); PutbackRMWord(m_dst); break;
							case 0x10: do { ROLC_WORD(); c--; } while (c > 0); PutbackRMWord(m_dst); break;
							case 0x18: do { RORC_WORD(); c--; } while (c > 0); PutbackRMWord(m_dst); break;
							case 0x30:
							case 0x20: SHL_WORD(c); break;
							case 0x28: SHR_WORD(c); break;
							case 0x38: SHRA_WORD(c); break;
						}
						SetShiftOverflow(m_modrm & 0x38, count, true);  // ARIA
					}
				}
				break;

			case 0xd8: // i_esc
			case 0xd9:
			case 0xda:
			case 0xdb:
			case 0xdc:
			case 0xdd:
			case 0xde:
			case 0xdf:
				if (m_reloc & 0x8000)
				{
					m_ip = m_prev_ip;
					interrupt(7);
					break;
				}
				m_modrm = fetch();
				GetRMByte();
				CLK(NOP);
				// The 80187 has the FSTSW AX instruction
				if ((m_modrm == 0xe0) && (op == 0xdf))
					m_regs.w[AX] = 0xffff;  // FPU not present
				break;

			case 0xe6: // i_outal
				write_port_byte_al(fetch());
				CLK(OUT_IMM8);
				break;

			case 0xee: // i_outdxal
				write_port_byte_al(m_regs.w[DX]);
				CLK(OUT_DX8);
				break;

			case 0xf2: // i_repne
			case 0xf3:
				{
					bool pass = false;
					uint8_t next = repx_op();
					uint16_t c = m_regs.w[CX];

					switch (next)
					{
					case 0x6c:
						CLK(OVERRIDE);
						if (c)
						{
							do
							{
								i_insb();
								if (m_io_stall)
								{
									m_io_stall = false;
									break;
								}
								c--;
							} while (c > 0 && m_icount > 0);
						}
						m_regs.w[CX] = c;
						m_seg_prefix = false;
						m_seg_prefix_next = false;
						break;
					case 0x6d:
						CLK(OVERRIDE);
						if (c)
						{
							do
							{
								i_insw();
								if (m_io_stall)
								{
									m_io_stall = false;
									break;
								}
								c--;
							} while (c > 0 && m_icount > 0);
						}
						m_regs.w[CX] = c;
						m_seg_prefix = false;
						m_seg_prefix_next = false;
						break;
					case 0x6e:
						CLK(OVERRIDE);
						if (c)
						{
							do
							{
								i_outsb();
								if (m_io_stall)
								{
									m_io_stall = false;
									break;
								}
								c--;
							} while (c > 0 && m_icount > 0);
						}
						m_regs.w[CX] = c;
						m_seg_prefix = false;
						m_seg_prefix_next = false;
						break;
					case 0x6f:
						CLK(OVERRIDE);
						if (c)
						{
							do
							{
								i_outsw();
								if (m_io_stall)
								{
									m_io_stall = false;
									break;
								}
								c--;
							} while (c > 0 && m_icount > 0);
						}
						m_regs.w[CX] = c;
						m_seg_prefix = false;
						m_seg_prefix_next = false;
						break;
					default:
						// Decrement IP and pass on
						m_ip -= 1 + (m_seg_prefix_next ? 1 : 0);
						pass = true;
						break;
					}
					if (!pass)
					{
						if (c)
							m_ip = m_prev_ip;
						break;
					}
				}
				[[fallthrough]];
			default:
				if (!common_op(op))
				{
					m_icount -= 10; // UD fault timing?
					logerror("%06x: Invalid Opcode %02x\n", m_pc, op);
					interrupt(6); // 80186 has #UD
					break;
				}
		}
		// ARIA: single-instruction stepping. A segment prefix is executed
		// as its own pass through this loop, so only stop once the
		// instruction it prefixes has run too.
		if (m_single_step && !m_seg_prefix_next)
			break;
	}
}

// ---- verbatim from i186.cpp lines 775-925 ----
void i80186_cpu_device::device_reset()
{
	i8086_common_cpu_device::device_reset();
	/* reset the interrupt state */
	m_intr.priority_mask    = 0x0007;
	m_intr.timer[0]         = 0x000f;
	m_intr.timer[1]         = 0x000f;
	m_intr.timer[2]         = 0x000f;
	m_intr.dma[0]           = 0x000f;
	m_intr.dma[1]           = 0x000f;
	m_intr.ext[0]           = 0x000f;
	m_intr.ext[1]           = 0x000f;
	m_intr.ext[2]           = 0x000f;
	m_intr.ext[3]           = 0x000f;
	m_intr.in_service       = 0x0000;

	m_intr.pending          = 0x0000;
	m_intr.ack_mask         = 0x0000;
	m_intr.request          = 0x0000;
	m_intr.status           = 0x0000;
	m_intr.poll_status      = 0x0000;
	m_intr.ext_state        = 0x00;

	m_reloc = 0x20ff;
	m_mem.upper = 0xfffb;

	for (dma_state &elem : m_dma)
	{
		elem.drq_state = false;
		elem.control = 0;
	}

	for (timer_state &elem : m_timer)
	{
		elem.control = 0;
		elem.maxA = 0;
		elem.maxB = 0;
		elem.count = 0;
	}
}

uint8_t i80186_cpu_device::read_port_byte(uint16_t port)
{
	if (!(m_reloc & 0x1000) && (port >> 8) == (m_reloc & 0xff))
	{
		if (port & 1)
			return internal_port_r((port >> 1) & 0x7f, 0xff00) >> 8;
		else
			return internal_port_r((port >> 1) & 0x7f, 0x00ff) & 0xff;
	}
	return m_io->read_byte(port);
}

uint16_t i80186_cpu_device::read_port_word(uint16_t port)
{
	if (!(m_reloc & 0x1000) && (port >> 8) == (m_reloc & 0xff))
	{
		// Unaligned reads from the internal bus are swapped rather than split
		if (port & 1)
			return swapendian_int16(internal_port_r((port >> 1) & 0x7f));
		else
			return internal_port_r((port >> 1) & 0x7f);
	}
	return m_io->read_word_unaligned(port);
}

void i80186_cpu_device::write_port_byte(uint16_t port, uint8_t data)
{
	if (!(m_reloc & 0x1000) && (port >> 8) == (m_reloc & 0xff))
	{
		if (port & 1)
			internal_port_w((port >> 1) & 0x7f, data << 8);
		else
			internal_port_w((port >> 1) & 0x7f, data);
	}
	else
		m_io->write_byte(port, data);
}

void i80186_cpu_device::write_port_byte_al(uint16_t port)
{
	if (!(m_reloc & 0x1000) && (port >> 8) == (m_reloc & 0xff))
	{
		// Both AH and AL are written onto the internal bus
		if (port & 1)
			internal_port_w((port >> 1) & 0x7f, swapendian_int16(m_regs.w[AX]));
		else
			internal_port_w((port >> 1) & 0x7f, m_regs.w[AX]);
	}
	else
		m_io->write_byte(port, m_regs.w[AL]);
}

void i80186_cpu_device::write_port_word(uint16_t port, uint16_t data)
{
	if (!(m_reloc & 0x1000) && (port >> 8) == (m_reloc & 0xff))
	{
		// Unaligned writes to the internal bus are swapped rather than split
		if (port & 1)
			internal_port_w((port >> 1) & 0x7f, swapendian_int16(data));
		else
			internal_port_w((port >> 1) & 0x7f, data);
	}
	else
		m_io->write_word_unaligned(port, data);
}

uint8_t i80186_cpu_device::read_byte(uint32_t addr)
{
	if ((m_reloc & 0x1000) && ((addr >> 8) & 0xfff) == (m_reloc & 0xfff))
	{
		uint16_t ret = internal_port_r((addr >> 1) & 0x7f, (addr & 1) ? 0xff00 : 0x00ff);
		return (addr & 1) ? (ret >> 8) : (ret & 0xff);
	}
	return m_program->read_byte(addr);
}

uint16_t i80186_cpu_device::read_word(uint32_t addr)
{
	if ((m_reloc & 0x1000) && ((addr >> 8) & 0xfff) == (m_reloc & 0xfff))
	{
		// Unaligned reads from the internal bus are swapped rather than split
		if (addr & 1)
			return swapendian_int16(internal_port_r((addr >> 1) & 0x7f));
		else
			return internal_port_r((addr >> 1) & 0x7f);
	}
	// ARIA: was m_program->read_word_unaligned(addr); the second byte wraps
	// within the segment (m_ea_base in i86.h).
	return m_program->read_byte(addr) | (m_program->read_byte(SegmentNext(addr)) << 8);
}

void i80186_cpu_device::write_byte(uint32_t addr, uint8_t data)
{
	if ((m_reloc & 0x1000) && ((addr >> 8) & 0xfff) == (m_reloc & 0xfff))
		internal_port_w((addr >> 1) & 0x7f, (addr & 1) ? (data << 8) : data);
	else
		m_program->write_byte(addr, data);
}

void i80186_cpu_device::write_word(uint32_t addr, uint16_t data)
{
	if ((m_reloc & 0x1000) && ((addr >> 8) & 0xfff) == (m_reloc & 0xfff))
	{
		// Unaligned writes from the internal bus are swapped rather than split
		if (addr & 1)
			internal_port_w((addr >> 1) & 0x7f, swapendian_int16(data));
		else
			internal_port_w((addr >> 1) & 0x7f, data);
	}
	else
	{
		// ARIA: was m_program->write_word_unaligned(addr, data); the second
		// byte wraps within the segment (m_ea_base in i86.h).
		m_program->write_byte(addr, data & 0xff);
		m_program->write_byte(SegmentNext(addr), data >> 8);
	}
}

// ARIA: PCB placeholders until the peripherals are ported (HANDOFF 6.12).
// Only the relocation register (offset 7Fh, port base+FEh) works, with
// MAME's semantics from internal_port_r/w; it is what moves the PCB to
// I/O 0000h on the Aria and holds the ESC trap bit (HANDOFF 6.1).

void i80186_cpu_device::drq_callback(int which)
{
	// DRQ lines are never raised yet, so execute_run() does not get here.
	m_dma[which].drq_state = false;
}

uint16_t i80186_cpu_device::internal_port_r(offs_t offset, uint16_t mem_mask)
{
	switch (offset)
	{
		case 0x7f:
			return m_reloc;

		default:
			LOGMASKED(LOG_PORTS, "%05X:read 80186 port %02X\n", m_pc, offset);
			break;
	}
	return 0x0000;
}

void i80186_cpu_device::internal_port_w(offs_t offset, uint16_t data)
{
	switch (offset)
	{
		case 0x7f:
			// 80188 byte output to this port is *not* masked!
			LOGMASKED(LOG_PORTS, "%05X:80186 relocation register = %04X\n", m_pc, data);
			m_reloc = data;
			break;

		default:
			LOGMASKED(LOG_PORTS, "%05X:80186 port %02X = %04X\n", m_pc, offset, data);
			break;
	}
}

}  // namespace mame_i86
