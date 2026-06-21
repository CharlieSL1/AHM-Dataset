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
; Key: C
; Chord_Progression: bVImaj7-bVII7-Imaj7
; ChordName: Abmaj7-Bb7-Cmaj7
; Voicing: Drop 2+4

t 0 60

; beat 0.00 : Abmaj7 [Drop 2+4]
i1  0.000  1.000  56  0.70  ; Ab3
i1  0.000  1.000  63  0.70  ; Eb4
i1  0.000  1.000  72  0.70  ; C5
i1  0.000  1.000  79  0.70  ; G5

; beat 1.00 : Bb7 [Drop 2+4]
i1  1.000  1.000  58  0.70  ; Bb3
i1  1.000  1.000  65  0.70  ; F4
i1  1.000  1.000  74  0.70  ; D5
i1  1.000  1.000  80  0.70  ; Ab5

; beat 2.00 : Cmaj7 [Drop 2+4]
i1  2.000  1.000  48  0.70  ; C3
i1  2.000  1.000  55  0.70  ; G3
i1  2.000  1.000  64  0.70  ; E4
i1  2.000  1.000  71  0.70  ; B4

e
</CsScore>
</CsoundSynthesizer>
