<CsoundSynthesizer>
<CsOptions>
; If chord_play is not globally installed, add:
; --opcode-lib=/absolute/path/to/AHM-Dataset/Csound/opcode/libchord_play.dylib
-o dac
</CsOptions>
<CsInstruments>
; p4 = MIDI note number   p5 = amplitude 0-1

sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

gitab  ftgen 0, 0, 4096, 10, 1

; Synth instrument (instr 2) — triggered by chord_play
instr 2
  ifreq  cpsmidinn p4
  iamp   = p5 * 0dbfs
  aenv   expseg 1, p3 * 0.01, 0.5, p3 * 0.89, 0.001, p3 * 0.1, 0.001
  asig   foscili aenv * iamp, ifreq, 1, 2, 2.5, gitab
         outs asig, asig
endin

; Trigger instrument — calls chord_play to schedule chords
instr 1
  ; chord_play  SChord,    iInstr, iStart, iDur, iAmp, iOctave
  chord_play "C-G-Am-F",       2,    p2,    1,   0.7,    4
endin

; Jazz progression (with 7th chords)
instr 3
  chord_play "F7-Em7b5-Ebm7-Dm7", 2, p2, 1, 0.7, 4
endin

</CsInstruments>
<CsScore>
; Play the pop progression at t=0
i1  0  4

; Play the jazz progression at t=5
i3  5  4

e
</CsScore>
</CsoundSynthesizer>




<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>487</x>
 <y>139</y>
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
