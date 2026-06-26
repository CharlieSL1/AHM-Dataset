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
; Key: B
; Chord_Progression: IIm7-V7-Imaj7
; ChordName: Dbm7-F#7-Bmaj7
; Voicing: Drop 2+4

t 0 60

; beat 0.00 : Dbm7 [Drop 2+4]
i1  0.000  1.000  49  0.70  ; C#3
i1  0.000  1.000  56  0.70  ; Ab3
i1  0.000  1.000  64  0.70  ; E4
i1  0.000  1.000  71  0.70  ; B4

; beat 1.00 : F#7 [Drop 2+4]
i1  1.000  1.000  54  0.70  ; F#3
i1  1.000  1.000  61  0.70  ; C#4
i1  1.000  1.000  70  0.70  ; Bb4
i1  1.000  1.000  76  0.70  ; E5

; beat 2.00 : Bmaj7 [Drop 2+4]
i1  2.000  1.000  59  0.70  ; B3
i1  2.000  1.000  66  0.70  ; F#4
i1  2.000  1.000  75  0.70  ; Eb5
i1  2.000  1.000  82  0.70  ; Bb5

e
</CsScore>
</CsoundSynthesizer>
