# Makefile — build chord_play Csound opcode plugin (macOS)

CC      = gcc
CSOUND  = /Library/Frameworks/CsoundLib64.framework
# Build universal binary (x86_64 for CsoundQt-Rosetta + arm64 for native Csound)
CFLAGS  = -shared -fPIC -O2 \
           -arch x86_64 -arch arm64 \
           -I$(CSOUND)/Headers \
           -F/Library/Frameworks \
           -framework CsoundLib64
TARGET  = libchord_play.dylib

$(TARGET): chord_play.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
