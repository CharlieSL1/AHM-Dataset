# Makefile — build chord_play, chord_gen, and gen Csound opcode plugins (macOS)

CC     = gcc
CSOUND = /Library/Frameworks/CsoundLib64.framework

# ONNX Runtime universal binary (x86_64 + arm64) bundled in this directory
ORT_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CFLAGS = -shared -fPIC -O2 \
          -arch x86_64 -arch arm64 \
          -I$(CSOUND)/Headers \
          -F/Library/Frameworks \
          -framework CsoundLib64

CFLAGS_ORT = $(CFLAGS) \
              -I$(ORT_DIR) \
              -L$(ORT_DIR) -lonnxruntime \
              -Wl,-rpath,$(ORT_DIR) \
              -lm

all: libchord_play.dylib libchord_gen.dylib libgen.dylib

libchord_play.dylib: chord_play.c
	$(CC) $(CFLAGS) -o $@ $<

libchord_gen.dylib: chord_gen.c
	$(CC) $(CFLAGS) -lm -o $@ $<

libgen.dylib: gen.c onnxruntime_c_api.h libonnxruntime.dylib
	$(CC) $(CFLAGS_ORT) -o $@ gen.c

clean:
	rm -f libchord_play.dylib libchord_gen.dylib libgen.dylib
