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
; Key: A
; Chord_Progression: V7-VIm7
; ChordName: E7-F#m7
; Voicing: Drop 2

t 0 60

; beat 0.00 : E7 [Drop 2]
i1  0.000  1.000  59  0.70  ; B3
i1  0.000  1.000  64  0.70  ; E4
i1  0.000  1.000  68  0.70  ; Ab4
i1  0.000  1.000  74  0.70  ; D5

; beat 1.00 : F#m7 [Drop 2]
i1  1.000  1.000  61  0.70  ; C#4
i1  1.000  1.000  66  0.70  ; F#4
i1  1.000  1.000  69  0.70  ; A4
i1  1.000  1.000  76  0.70  ; E5

e
</CsScore>
</CsoundSynthesizer>
