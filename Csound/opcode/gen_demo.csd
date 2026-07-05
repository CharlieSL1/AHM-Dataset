<CsoundSynthesizer>
<CsOptions>
; Load the gen plugin before anything else.
; Replace the path below with the actual location of libgen.dylib.
-o dac
</CsOptions>
<CsInstruments>

sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

gitab ftgen 0, 0, 4096, 10, 1

; Load ONNX model and lookup table once at startup.
emoChord_init "/path/to/AHM-Dataset/Csound/opcode/gen_model.onnx", \
              "/path/to/AHM-Dataset/Csound/opcode/gen_data.tsv"

; ── Synthesis instrument ──────────────────────────────────────────────────────
; p4 = MIDI note number, p5 = amplitude 0–1
instr 2
  ifreq cpsmidinn p4
  iamp  = p5 * 0dbfs
  aenv  expseg 1, p3*0.01, 0.5, p3*0.89, 0.001, p3*0.1, 0.001
  asig  foscili aenv * iamp, ifreq, 1, 2, 2.5, gitab
        outs asig, asig
endin

; ── Emotion-driven instrument ─────────────────────────────────────────────────
; p4 = emotion string (passed from <CsScore>)
; emoChord runs inference, prints the chosen chord name, and triggers instr 2.
instr 1
  Sem strget p4
  emoChord Sem, 2, p2, 1, 0.7
endin

</CsInstruments>
<CsScore>
; User writes: instrument  start  duration  "emotion"
; emoChord prints the chord progression name to the console and plays it.

i1   0   4   "soft"
i1   6   4   "depressive"
i1  12   4   "fantasy"
i1  18   4   "soft"
e
</CsScore>
</CsoundSynthesizer>
















<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>492</x>
 <y>250</y>
 <width>320</width>
 <height>211</height>
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
