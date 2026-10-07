# Sestaveni emulatoru a testu. Vstupni body jsou build.sh / build.bat,
# build-tests.sh / build-tests.bat a run-tests.sh -- ty si pripravi prekladac
# a zavolaji tento soubor. Vzor je Makefile projektu Eureka A4.
#
# POZOR: make si shell vybira podle PATH -- z cmd.exe je to cmd, z bashe sh.
# Recepty proto nesmi pouzivat nic, co je jen v jednom z nich: zadne `copy`,
# `if not exist`, `rem`, `mkdir -p`.

MINGW64 ?= C:/msys64/mingw64
CXX := $(MINGW64)/bin/g++.exe
CC  := $(MINGW64)/bin/gcc.exe
RC  := $(MINGW64)/bin/windres.exe

BUILD := build
BIN   := bin

WARN     := -Wall -Wextra -Wno-unused-parameter
DEPFLAGS  = -MMD -MP
STATIC   := -static -static-libgcc -static-libstdc++
SECTIONS := -ffunction-sections -fdata-sections

CXXFLAGS  := -std=c++20 -O2 $(WARN) $(SECTIONS) -Isrc -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00
TESTFLAGS := -std=c++20 -O2 $(WARN) -Isrc -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00

EMU_NAMES  := main text_codec zip_reader fat12 flash_image cpu80188
# Jadro CPU prevzate z MAME (src/i86/LICENSE.mame-i86.txt, HANDOFF 6.12).
I86_NAMES  := i86 i186
I86_OBJS   := $(addprefix $(BUILD)/i86_,$(addsuffix .o,$(I86_NAMES)))
EMU_OBJS   := $(addprefix $(BUILD)/,$(addsuffix .o,$(EMU_NAMES))) $(I86_OBJS)
# Objekty, proti kterym se linkuji testy. Bez main.o (ma vlastni wWinMain).
CORE_OBJS  := $(BUILD)/text_codec.o $(BUILD)/zip_reader.o $(BUILD)/fat12.o \
              $(BUILD)/flash_image.o
CPU_OBJS   := $(BUILD)/cpu80188.o $(I86_OBJS)
# Jadro DSP ADSP-2105 prevzate z MAME (src/adsp2100/LICENSE.mame-adsp2100.txt,
# HANDOFF 6.3) a jeho obal s pameti cipu a zavadenim.
DSP_OBJS   := $(BUILD)/adsp2105.o $(BUILD)/adsp_adsp2100.o
# Stroj Arie s BIOSem emulatoru (HANDOFF 6.10, 6.11).
MACHINE_OBJS := $(BUILD)/aria_machine.o $(BUILD)/hle_bios.o $(BUILD)/rtc146818.o \
                $(CPU_OBJS) $(DSP_OBJS) $(CORE_OBJS)

EMU        := $(BIN)/AriaEmulator.exe
TEST_EXES  := $(BIN)/codec_test.exe $(BIN)/flash_selftest.exe $(BIN)/firmware_test.exe \
              $(BIN)/cpu_test.exe $(BIN)/cpu_selftest.exe $(BIN)/pklite_unpack.exe \
              $(BIN)/boot_test.exe $(BIN)/dsp_selftest.exe $(BIN)/dsp_disasm.exe

.PHONY: all tests check clean
.DEFAULT_GOAL := all

all: $(EMU)
tests: $(EMU) $(TEST_EXES)

$(BUILD) $(BIN):
	mkdir $@

# Kazde pravidlo si zada i tento soubor: jsou v nem prepinace prekladu a .d
# soubory sleduji jen hlavicky, takze zmena prepinacu by jinak nic
# neprelozila (zkusenost z Eureky).
$(BUILD)/%.o: src/%.cpp Makefile | $(BUILD)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/test_%.o: tests/%.cpp Makefile | $(BUILD)
	$(CXX) $(TESTFLAGS) $(DEPFLAGS) -c $< -o $@

# Kod z MAME zustava doslovny, aby sel porovnat s originalem; jedine
# varovani, ktere v nem GCC hlasi (int proti uint32_t ve smycce ENTER,
# i186.cpp), se proto vypina jen tady, ne opravou kodu.
$(BUILD)/i86_%.o: src/i86/%.cpp Makefile | $(BUILD)
	$(CXX) $(CXXFLAGS) -Wno-sign-compare $(DEPFLAGS) -c $< -o $@

# Totez pro jadro DSP z MAME (porovnani uint32_t s int v shift_op_imm,
# 2100ops.hxx).
$(BUILD)/adsp_%.o: src/adsp2100/%.cpp Makefile | $(BUILD)
	$(CXX) $(CXXFLAGS) -Wno-sign-compare $(DEPFLAGS) -c $< -o $@

# -municode: vstupni bod je sirokoznakovy. -mwindows: GUI subsystem, bez okna
# konzole. shell32 kvuli CommandLineToArgvW.
$(EMU): $(EMU_OBJS) | $(BIN)
	$(CXX) -municode -mwindows $(STATIC) -Wl,--gc-sections -s -o $@ $(EMU_OBJS) -lshell32

$(BIN)/codec_test.exe: $(BUILD)/test_codec_test.o $(BUILD)/text_codec.o | $(BIN)
	$(CXX) $(STATIC) -o $@ $^

$(BIN)/flash_selftest.exe: $(BUILD)/test_flash_selftest.o $(CORE_OBJS) | $(BIN)
	$(CXX) $(STATIC) -o $@ $^

# wmain, proto -municode.
$(BIN)/firmware_test.exe: $(BUILD)/test_firmware_test.o $(CORE_OBJS) | $(BIN)
	$(CXX) $(STATIC) -municode -o $@ $^

# Instrukcni testy jadra proti SingleStepTests (HANDOFF 6.12). wmain.
$(BIN)/cpu_test.exe: $(BUILD)/test_cpu_test.o $(CPU_OBJS) $(BUILD)/zip_reader.o | $(BIN)
	$(CXX) $(STATIC) -municode -o $@ $^

# Rozbaleni programu PKLITE z disku B: behem jejich vlastniho stubu
# (HANDOFF 6.1); vystup jen pro analyzu. wmain.
$(BIN)/pklite_unpack.exe: $(BUILD)/test_pklite_unpack.o $(CPU_OBJS) $(CORE_OBJS) | $(BIN)
	$(CXX) $(STATIC) -municode -o $@ $^

# Start jadra FreeDOS z disku B: na BIOSu emulatoru (ar-4cu). wmain.
$(BIN)/boot_test.exe: $(BUILD)/test_boot_test.o $(MACHINE_OBJS) | $(BIN)
	$(CXX) $(STATIC) -municode -o $@ $^

# Jadro DSP bez firmwaru: poradi bajtu zavadeci stranky a kratky program.
$(BIN)/dsp_selftest.exe: $(BUILD)/test_dsp_selftest.o $(DSP_OBJS) | $(BIN)
	$(CXX) $(STATIC) -o $@ $^

# Disasembler programu DSP (zavadeci stranka, napr. LPC.DSP) pro analyzu;
# disasembler je z MAME. wmain.
$(BIN)/dsp_disasm.exe: $(BUILD)/test_dsp_disasm.o $(BUILD)/adsp_2100dasm.o | $(BIN)
	$(CXX) $(STATIC) -municode -o $@ $^

# Jadro CPU bez dat testu: kratke programy (PCB, preruseni, past ESC).
$(BIN)/cpu_selftest.exe: $(BUILD)/test_cpu_selftest.o $(CPU_OBJS) | $(BIN)
	$(CXX) $(STATIC) -o $@ $^

# ---------------------------------------------------------------------------
# Testy
#
# Kazdy test je samostatny cil, aby je make -j pustil naraz. Firmware neni
# v repozitari (ROM-NOTICE.txt); kdyz ho run-tests.sh nenajde, nastavi
# v PROSTREDI SKIP_FIRMWARE=1 a testu s firmwarem se vynechaji.
#
# Seznam rezimu je jen tady. Druha kopie jinde by se s touto rozesla potichu.

ARIAFW ?= D:/aria/update
FW     ?= $(ARIAFW)

FW_MODES := cc0705 ariav699 csr0687 csr0705 ecrd0704 eser0704 tc0690 tsr0690
ifeq ($(SKIP_FIRMWARE),1)
MODES :=
else
MODES := $(FW_MODES)
endif
CHECKS := check-codec check-flash check-cpu check-dsp $(addprefix check-fw-,$(MODES))
# Rozbaleni PKLITE potrebuje firmware i jadro; jeden beh na ceskou 7.05.
ifneq ($(SKIP_FIRMWARE),1)
CHECKS += check-unpack
endif

# Data instrukcnich testu (MIT, ale velka) lezi mimo strom v ARIA_CPUTESTS
# (HANDOFF 6.12). Bez nich run-tests.sh nastavi SKIP_CPU=1.
ARIA_CPUTESTS ?= D:/aria/cputests
CPUTESTS      ?= $(ARIA_CPUTESTS)
ifneq ($(SKIP_CPU),1)
CHECKS += check-cpu-8088 check-cpu-v20
endif

# Jadro FreeDOS (GPL, samostatny soubor vedle emulatoru, HANDOFF 6.10) lezi
# mimo strom v ARIA_FREEDOS. Bez nej nebo bez firmwaru run-tests.sh nastavi
# SKIP_FREEDOS=1.
ARIA_FREEDOS ?= D:/aria/freedos
FREEDOS      ?= $(ARIA_FREEDOS)
ifneq ($(SKIP_FREEDOS),1)
ifneq ($(SKIP_FIRMWARE),1)
CHECKS += check-boot check-boot-687
endif
endif

.PHONY: $(CHECKS)

check: $(CHECKS)

check-codec: $(BIN)/codec_test.exe
	$(BIN)/codec_test.exe

check-flash: $(BIN)/flash_selftest.exe
	$(BIN)/flash_selftest.exe

check-dsp: $(BIN)/dsp_selftest.exe
	$(BIN)/dsp_selftest.exe

check-cpu: $(BIN)/cpu_selftest.exe
	$(BIN)/cpu_selftest.exe

check-unpack: $(BIN)/pklite_unpack.exe
	$(BIN)/pklite_unpack.exe "$(FW)/CARD/CC0705.ZIP" --check

check-boot: $(BIN)/boot_test.exe
	$(BIN)/boot_test.exe "$(FW)/CARD/CC0705.ZIP" "$(FREEDOS)/KERNEL.SYS"

# 6.87 v Kamenickych, kvuli ktere je disk B: volitelny (README).
check-boot-687: $(BIN)/boot_test.exe
	$(BIN)/boot_test.exe "$(FW)/SERIAL/CSR0687.ZIP" "$(FREEDOS)/KERNEL.SYS"

check-cpu-8088: $(BIN)/cpu_test.exe
	$(BIN)/cpu_test.exe "$(CPUTESTS)" 8088

check-cpu-v20: $(BIN)/cpu_test.exe
	$(BIN)/cpu_test.exe "$(CPUTESTS)" v20

# Vyslovna pravidla, ne vzorove `check-fw-%`: make na .PHONY cilech vzorova
# pravidla nehleda a rezimy by tise prosly bez receptu (zkusenost z Eureky).
# FW je v UVOZOVKACH, aby cestu se zpetnymi lomitky nesnedl sh.
define CHECK_RULE
check-fw-$(1): $$(BIN)/firmware_test.exe
	$$(BIN)/firmware_test.exe "$$(FW)" $(1)
endef
$(foreach m,$(MODES),$(eval $(call CHECK_RULE,$(m))))

CLEANFILES := $(BUILD)/*.o $(BUILD)/*.d $(TEST_EXES) $(EMU)
ifeq (,$(findstring sh,$(notdir $(SHELL))))
clean:
	-del /q $(subst /,\,$(CLEANFILES))
else
clean:
	rm -f $(CLEANFILES)
endif

-include $(wildcard $(BUILD)/*.d)
