<CsoundSynthesizer>
<CsOptions>
; If opcodes are not globally installed, add one line per library:
; --opcode-lib=/absolute/path/to/AHM-Dataset/Csound/opcode/libchord_play.dylib
; --opcode-lib=/absolute/path/to/AHM-Dataset/Csound/opcode/libchord_gen.dylib
-o dac
</CsOptions>
<CsInstruments>
; p4 = MIDI note number   p5 = amplitude 0-1

sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

gitab  ftgen 0, 0, 4096, 10, 1

; Load chord_gen model data — call once before any instrument uses chord_gen
chord_gen_init "/Users/zhaojinlan/Documents/GitHub/AHM-Dataset/Csound/opcode/chord_gen_data.tsv"

; ── Synthesis instrument (used by both chord_play and chord_gen) ──────────
; p4 = MIDI note number   p5 = amplitude
instr 2
  ifreq  cpsmidinn p4
  iamp   = p5 * 0dbfs
  aenv   expseg 1, p3*0.01, 0.5, p3*0.89, 0.001, p3*0.1, 0.001
  asig   foscili aenv * iamp, ifreq, 1, 2, 2.5, gitab
         outs asig, asig
endin

; ── chord_play demo: explicit chord string ────────────────────────────────
instr 1
  ; chord_play  SChord,           iInstr, iStart, iDur, iAmp
  chord_play    "C-G-Am-F",       2,      p2,     1,    0.7
endin

; ── chord_gen demo: emotion-driven, natural distribution (T=1) ───────────
instr 3
  ; chord_gen  SEmotion,  iInstr, iStart, iDur, iAmp
  chord_gen    "Joyful",  2,      p2,     1,    0.7
endin

; ── chord_gen demo: emotion + temperature control ─────────────────────────
instr 4
  ; iTemp=0  → always picks the highest-probability progression (deterministic)
  ; iTemp=1  → natural model distribution
  ; iTemp=2  → flatter, more varied
  chord_gen "Depressive", 2, p2, 1, 0.6, 1.5
endin

</CsInstruments>
<CsScore>
; chord_play — explicit I-V-vi-IV in C major
i1  0   4

; chord_gen — Joyful (model picks key, scale, progression)
i3  5   4

; chord_gen — Depressive with temperature 1.5
i4  10  4

e
</CsScore>
</CsoundSynthesizer>
<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>100</x>
 <y>100</y>
 <width>320</width>
 <height>240</height>
 <visible>true</visible>
 <uuid/>
 <bgcolor mode="background">
  <r>240</r>
  <g>240</g>
  <b>240</b>
 </bgcolor>
</bsbPanel>
<bsbPresets>
</bsbPresets>
