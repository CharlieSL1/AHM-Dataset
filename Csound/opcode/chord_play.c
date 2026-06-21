/*
 * chord_play.c — Csound plugin opcode
 *
 * Opcode:
 *   chord_play  SChord, iInstr, iStart, iDur, iAmp [, iOctave]
 *
 * Parameters:
 *   SChord  — chord name ("F7") or dash-separated progression ("C-G-Am-F")
 *   iInstr  — instrument number to schedule for synthesis
 *   iStart  — score start time (seconds) for the first chord
 *   iDur    — duration per chord (seconds)
 *   iAmp    — amplitude 0–1
 *   iOctave — root octave for close voicing (optional, default 4 → C4=60)
 *
 * Example (orchestra):
 *   instr 1
 *     chord_play "C-G-Am-F", 2, p2, 1, 0.7
 *   endin
 *
 * Build:
 *   make -C Csound/opcode/
 *
 * Install:
 *   Edit → Preferences → Environment → Opcode dir (in CsoundQt)
 *   or add --opcode-lib=/abs/path/to/libchord_play.dylib to <CsOptions>
 */

#include <csdl.h>
#include <string.h>

/* ── Root note → pitch class (2-char names checked before 1-char) ─────────── */
static const struct { const char *name; int pc; } NOTE_MAP[] = {
    {"C#", 1}, {"Cb",11}, {"D#", 3}, {"Db", 1},
    {"Eb", 3}, {"Fb", 4}, {"F#", 6}, {"Gb", 6},
    {"G#", 8}, {"Ab", 8}, {"A#",10}, {"Bb",10},
    {"C",  0}, {"D",  2}, {"E",  4}, {"F",  5},
    {"G",  7}, {"A",  9}, {"B", 11},
    {NULL, -1}
};

/* ── Chord quality → semitone intervals (longer strings checked first) ──────── */
static const struct { const char *qual; int n; int iv[5]; } QUAL_MAP[] = {
    {"m7b5", 4, {0, 3, 6,10, 0}},   /* half-diminished */
    {"maj7", 4, {0, 4, 7,11, 0}},   /* major 7th       */
    {"dim7", 4, {0, 3, 6, 9, 0}},   /* diminished 7th  */
    {"m7",   4, {0, 3, 7,10, 0}},   /* minor 7th       */
    {"m6",   4, {0, 3, 7, 9, 0}},   /* minor 6th       */
    {"7",    4, {0, 4, 7,10, 0}},   /* dominant 7th    */
    {"sus",  3, {0, 5, 7, 0, 0}},   /* suspended 4th   */
    {"dim",  3, {0, 3, 6, 0, 0}},   /* diminished      */
    {"m",    3, {0, 3, 7, 0, 0}},   /* minor           */
    {"",     3, {0, 4, 7, 0, 0}},   /* major — last    */
    {NULL,   0, {0, 0, 0, 0, 0}}
};

/* Parse one chord token into MIDI note numbers.
   Returns note count, or -1 on unrecognised input. */
static int parse_chord(const char *tok, int octave, int out[8])
{
    int root_pc = -1, root_len = 0;
    for (int i = 0; NOTE_MAP[i].name; i++) {
        int l = (int)strlen(NOTE_MAP[i].name);
        if (strncmp(tok, NOTE_MAP[i].name, l) == 0) {
            root_pc  = NOTE_MAP[i].pc;
            root_len = l;
            break;
        }
    }
    if (root_pc < 0) return -1;

    int root_midi    = 12 * (octave + 1) + root_pc;
    const char *qual = tok + root_len;

    for (int q = 0; QUAL_MAP[q].qual; q++) {
        if (strcmp(qual, QUAL_MAP[q].qual) == 0) {
            for (int k = 0; k < QUAL_MAP[q].n; k++)
                out[k] = root_midi + QUAL_MAP[q].iv[k];
            return QUAL_MAP[q].n;
        }
    }
    return -1;
}

/* ── Opcode data structure ─────────────────────────────────────────────────── */
typedef struct {
    OPDS       h;
    STRINGDAT *SChord;
    MYFLT     *iInstr;
    MYFLT     *iStart;
    MYFLT     *iDur;
    MYFLT     *iAmp;
    MYFLT     *iOctave;   /* optional: j-type → default -1, we map to 4 */
} CHORD_PLAY;

/* ── i-rate init: parse string and schedule one note-event per note ─────────── */
static int chord_play_init(CSOUND *csound, CHORD_PLAY *p)
{
    int    octave = (*p->iOctave < FL(0.0)) ? 4 : (int)*p->iOctave;
    double dur    = (double)*p->iDur;

    /* strtok mutates the buffer, so work on a local copy */
    char buf[512];
    strncpy(buf, p->SChord->data, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    double chord_start = (double)*p->iStart;
    char  *token       = strtok(buf, "-");

    while (token != NULL) {
        int notes[8];
        int n = parse_chord(token, octave, notes);

        if (n < 0) {
            csound->Message(csound,
                "chord_play: WARNING: unrecognised chord '%s', skipping\n", token);
        } else {
            for (int i = 0; i < n; i++) {
                EVTBLK evt;
                memset(&evt, 0, sizeof(EVTBLK));
                evt.opcod  = 'i';
                evt.pcnt   = 5;
                evt.p[1]   = *p->iInstr;
                evt.p[2]   = (MYFLT)chord_start;
                evt.p2orig = (MYFLT)chord_start;
                evt.p[3]   = (MYFLT)dur;
                evt.p3orig = (MYFLT)dur;
                evt.p[4]   = (MYFLT)notes[i];   /* MIDI note number */
                evt.p[5]   = *p->iAmp;
                evt.strarg = NULL;
                evt.scnt   = 0;
                csound->insert_score_event(csound, &evt, FL(0.0));
            }
        }
        chord_start += dur;
        token = strtok(NULL, "-");
    }
    return OK;
}

/* ── Opcode registration table ─────────────────────────────────────────────── */
static OENTRY localops[] = {
    {
        "chord_play",           /* opcode name          */
        sizeof(CHORD_PLAY),     /* data structure size  */
        0,                      /* flags                */
        1,                      /* thread: i-rate only  */
        "",                     /* output types: none   */
        "Siiiij",               /* S=string i=i-rate j=optional(default -1) */
        (SUBR)chord_play_init,
        NULL,
        NULL,
        NULL                    /* useropinfo           */
    },
};

/* ── Module entry points ───────────────────────────────────────────────────── */
PUBLIC int csoundModuleCreate(CSOUND *csound)  { IGN(csound); return 0; }
PUBLIC int csoundModuleDestroy(CSOUND *csound) { IGN(csound); return 0; }

PUBLIC int csoundModuleInit(CSOUND *csound)
{
    return csound->AppendOpcodes(csound, localops,
                                  (int)(sizeof(localops) / sizeof(localops[0])));
}
