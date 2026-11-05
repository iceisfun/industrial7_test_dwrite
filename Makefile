# Each directory under tests/ builds to build/<dir>.exe (all .c in it + the common layer).
# The DLL under test is deliberately NOT linked (no -ldwrite); everything resolves via GetProcAddress.
# common/: t.c harness, dw.c DirectWrite helpers, dwx.c COM test doubles, ttfgen.c synthetic font builder.
# gdi32/user32 are linked only for the memory-DC scaffolding in dwx.c (they are not the code under test).
CC      = x86_64-w64-mingw32-gcc
CFLAGS  ?= -O1 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format -Wno-incompatible-pointer-types -Wno-misleading-indentation
AREAS   := $(notdir $(wildcard tests/*))
EXES    := $(addprefix build/,$(addsuffix .exe,$(AREAS)))
COMMON  := t dw dwx ttfgen
COBJS   := $(addprefix build/obj/,$(addsuffix .o,$(COMMON)))
HDRS    := $(wildcard common/*.h common/*.inc)
LIBS    := -static -lkernel32 -ladvapi32 -lntdll -lgdi32 -luser32
all: $(EXES)
build/obj/%.o: common/%.c $(HDRS)
	@mkdir -p build/obj
	$(CC) $(CFLAGS) -Icommon -c -o $@ $<
.SECONDEXPANSION:
build/%.exe: $$(wildcard tests/$$*/*.c) $$(wildcard tests/$$*/*.h) $(COBJS) $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -Icommon -o $@ $(wildcard tests/$*/*.c) $(COBJS) $(LIBS)
clean: ; rm -rf build
# Validation under Wine in a headless sandbox (see tools/wine_run.sh). Windows is the authority.
wine-run: all
	@for e in $(EXES); do echo "== $$e"; tools/wine_run.sh $$e | grep -E '^(FAIL|SKIP|SUMMARY)|^  (FAIL|NOTE)'; done
coverage:
	python3 tools/dw_coverage.py .
.PHONY: all clean wine-run coverage
