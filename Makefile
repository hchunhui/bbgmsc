.PHONY: all clean
all: smolnes

WARN=-std=c99 \
		 -Wall \
     -Wno-parentheses \
		 -Wno-misleading-indentation \
		 -Wno-bool-operation \
		 -Wno-discarded-qualifiers \
		 -Wno-incompatible-pointer-types-discards-qualifiers \
		 -Wno-unknown-warning-option \
		 -Wno-switch-outside-range \
		 -Wno-unused-value \
		 -Wno-char-subscripts \
		 -Wno-switch
SDLFLAGS=$(shell pkg-config --cflags --libs sdl2)

smolnes: deobfuscated.c apu.c bbk/bbk.c bbk/nes_fdc.c bbk/nes_joy.c bbk/LPC_D6_SYNTH.c
	$(CC) -O2 -o $@ $^$> ${SDLFLAGS} -g ${WARN}

smolnes.wasm: deobfuscated.c apu.c bbk/bbk.c bbk/nes_fdc.c bbk/nes_joy.c bbk/LPC_D6_SYNTH.c wasm/libc/libc.c
	clang --target=wasm32 -O3 -nostdlib -fno-builtin -ffunction-sections -fdata-sections -fvisibility=default -I wasm/libc ${CFLAGS} -Wl,--gc-sections -Wl,--export-dynamic -Wl,--no-entry -Wl,--strip-all -Wl,--allow-undefined -o $@ $^$> ${WARN}

clean:
	rm -f smolnes deobfuscated
