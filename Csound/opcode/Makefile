# Makefile — build chord_play and chord_gen Csound opcode plugins (macOS)

CC     = gcc
CSOUND = /Library/Frameworks/CsoundLib64.framework
CFLAGS = -shared -fPIC -O2 \
          -arch x86_64 -arch arm64 \
          -I$(CSOUND)/Headers \
          -F/Library/Frameworks \
          -framework CsoundLib64

all: libchord_play.dylib libchord_gen.dylib

libchord_play.dylib: chord_play.c
	$(CC) $(CFLAGS) -o $@ $<

libchord_gen.dylib: chord_gen.c
	$(CC) $(CFLAGS) -lm -o $@ $<

clean:
	rm -f libchord_play.dylib libchord_gen.dylib
