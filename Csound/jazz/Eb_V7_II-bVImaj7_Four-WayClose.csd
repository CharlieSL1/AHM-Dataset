<CsoundSynthesizer>
<CsOptions>
-o output.wav -W
</CsOptions>
<CsInstruments>
; p4 = MIDI note number (60 = C4)   p5 = amplitude 0-1

sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

gitab  ftgen 0, 0, 4096, 10, 1   ; sine wave table (orchestra-level, Csound 6+/7)

; Instrument 1: FM piano-like synthesis
instr 1
  ifreq  cpsmidinn p4
  iamp   = p5 * 0dbfs

  ; Amplitude envelope: sharp attack, exponential decay
  aenv   expseg 1, p3 * 0.01, 0.5, p3 * 0.89, 0.001, p3 * 0.1, 0.001

  ; FM synthesis — carrier:modulator ratio 1:2, mod index 2.5
  asig   foscili aenv * iamp, ifreq, 1, 2, 2.5, gitab

         outs asig, asig
endin
</CsInstruments>
<CsScore>
; Genre: jazz
; Key: Eb
; Chord_Progression: V7/II-bVImaj7
; ChordName: C7-Bmaj7
; Voicing: Four-Way Close

t 0 60

; beat 0.00 : C7 [Four-Way Close]
i1  0.000  1.000  60  0.70  ; C4
i1  0.000  1.000  64  0.70  ; E4
i1  0.000  1.000  67  0.70  ; G4
i1  0.000  1.000  70  0.70  ; Bb4

; beat 1.00 : Bmaj7 [Four-Way Close]
i1  1.000  1.000  71  0.70  ; B4
i1  1.000  1.000  75  0.70  ; Eb5
i1  1.000  1.000  78  0.70  ; F#5
i1  1.000  1.000  82  0.70  ; Bb5

e
</CsScore>
</CsoundSynthesizer>
