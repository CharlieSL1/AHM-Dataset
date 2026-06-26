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
; Key: Bb
; Chord_Progression: IVm7-bVII7-Imaj7
; ChordName: Ebm7-Ab7-Bbmaj7
; Voicing: Four-Way Close

t 0 60

; beat 0.00 : Ebm7 [Four-Way Close]
i1  0.000  1.000  63  0.70  ; Eb4
i1  0.000  1.000  66  0.70  ; F#4
i1  0.000  1.000  70  0.70  ; Bb4
i1  0.000  1.000  73  0.70  ; C#5

; beat 1.00 : Ab7 [Four-Way Close]
i1  1.000  1.000  68  0.70  ; Ab4
i1  1.000  1.000  72  0.70  ; C5
i1  1.000  1.000  75  0.70  ; Eb5
i1  1.000  1.000  78  0.70  ; F#5

; beat 2.00 : Bbmaj7 [Four-Way Close]
i1  2.000  1.000  70  0.70  ; Bb4
i1  2.000  1.000  74  0.70  ; D5
i1  2.000  1.000  77  0.70  ; F5
i1  2.000  1.000  81  0.70  ; A5

e
</CsScore>
</CsoundSynthesizer>
