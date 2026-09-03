WASM_CC     ?= zig cc
WASM_TARGET ?= -target wasm32-freestanding
CC          ?= cc

SIM_SRC  := $(wildcard sim/*.c sim/vignettes/*.c)
SIM_HDR  := $(wildcard sim/*.h sim/vignettes/*.h)
TEST_SRC := $(wildcard test/*.c)
TEST_HDR := $(wildcard test/*.h)

# -Isim: sources under sim/vignettes/ include "world.h" and "vignettes/x.h" relative to sim/
WASM_FLAGS := $(WASM_TARGET) -std=c11 -nostdlib -ffreestanding -fno-builtin -fvisibility=hidden \
              -mbulk-memory -O2 -g0 -Wall -Wextra -Isim -Wl,--no-entry
TEST_FLAGS := -std=c11 -O0 -g -Wall -Wextra -fsanitize=address,undefined \
              -fno-omit-frame-pointer -Isim -Itest

all: sim.wasm

sim.wasm: $(SIM_SRC) $(SIM_HDR)
	$(WASM_CC) $(WASM_FLAGS) -o $@ $(SIM_SRC)
	@ls -l $@

build/test: $(SIM_SRC) $(SIM_HDR) $(TEST_SRC) $(TEST_HDR)
	@mkdir -p build
	$(CC) $(TEST_FLAGS) -o $@ $(SIM_SRC) $(TEST_SRC)

test: build/test
	./build/test

smoke: sim.wasm
	node test/wasm_smoke.mjs

serve:
	python3 -m http.server 8000

clean:
	rm -rf build sim.wasm

.PHONY: all test smoke serve clean
