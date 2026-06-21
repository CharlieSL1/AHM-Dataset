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
; Key: Db
; Chord_Progression: V7-#IVm7b5-IVm7-IIIm7
; ChordName: Ab7-Gm7b5-F#m7-Fm7
; Voicing: Four-Way Close

t 0 60

; beat 0.00 : Ab7 [Four-Way Close]
i1  0.000  1.000  68  0.70  ; Ab4
i1  0.000  1.000  72  0.70  ; C5
i1  0.000  1.000  75  0.70  ; Eb5
i1  0.000  1.000  78  0.70  ; F#5

; beat 1.00 : Gm7b5 [Four-Way Close]
i1  1.000  1.000  67  0.70  ; G4
i1  1.000  1.000  70  0.70  ; Bb4
i1  1.000  1.000  73  0.70  ; C#5
i1  1.000  1.000  77  0.70  ; F5

; beat 2.00 : F#m7 [Four-Way Close]
i1  2.000  1.000  66  0.70  ; F#4
i1  2.000  1.000  69  0.70  ; A4
i1  2.000  1.000  73  0.70  ; C#5
i1  2.000  1.000  76  0.70  ; E5

; beat 3.00 : Fm7 [Four-Way Close]
i1  3.000  1.000  65  0.70  ; F4
i1  3.000  1.000  68  0.70  ; Ab4
i1  3.000  1.000  72  0.70  ; C5
i1  3.000  1.000  75  0.70  ; Eb5

e
</CsScore>
</CsoundSynthesizer>
