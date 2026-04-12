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

smolnes: deobfuscated.c bbk/bbk.c bbk/nes_fdc.c bbk/nes_joy.c
	$(CC) -O2 -o $@ $^$> ${SDLFLAGS} -g ${WARN}

clean:
	rm -f smolnes deobfuscated
