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

EMU_NAMES  := main text_codec zip_reader fat12 flash_image
EMU_OBJS   := $(addprefix $(BUILD)/,$(addsuffix .o,$(EMU_NAMES)))
# Objekty, proti kterym se linkuji testy. Bez main.o (ma vlastni wWinMain).
CORE_OBJS  := $(BUILD)/text_codec.o $(BUILD)/zip_reader.o $(BUILD)/fat12.o \
              $(BUILD)/flash_image.o

EMU        := $(BIN)/AriaEmulator.exe
TEST_EXES  := $(BIN)/codec_test.exe $(BIN)/flash_selftest.exe $(BIN)/firmware_test.exe

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
CHECKS := check-codec check-flash $(addprefix check-fw-,$(MODES))

.PHONY: $(CHECKS)

check: $(CHECKS)

check-codec: $(BIN)/codec_test.exe
	$(BIN)/codec_test.exe

check-flash: $(BIN)/flash_selftest.exe
	$(BIN)/flash_selftest.exe

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
