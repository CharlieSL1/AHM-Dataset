/*
 * chord_gen.c — Csound plugin opcode
 *
 * Opcodes:
 *   chord_gen_init  SDataPath
 *   chord_gen       SEmotion, iInstr, iStart, iDur, iAmp [, iTemp, iOctave]
 *
 * Workflow:
 *   1. Run export_model.py once to generate chord_gen_data.tsv
 *   2. Call chord_gen_init with the path to that file (once, in instr 0)
 *   3. Call chord_gen anywhere in the orchestra to sample + play a progression
 *
 * Parameters:
 *   SDataPath — absolute path to chord_gen_data.tsv
 *   SEmotion  — emotion name, case-insensitive (e.g. "Joyful", "joyful")
 *   iInstr    — synthesis instrument number (receives p4=MIDI note, p5=amp)
 *   iStart    — score start time in seconds for the first chord
 *   iDur      — duration per chord in seconds
 *   iAmp      — amplitude 0–1
 *   iTemp     — sampling temperature (optional, default 1.0)
 *                 0 = deterministic (argmax), 1 = natural, >1 = more random
 *   iOctave   — root octave for close voicing (optional, default 4)
 *
 * Example:
 *   chord_gen_init "/path/to/chord_gen_data.tsv"
 *   instr 1
 *     chord_gen "Joyful", 2, p2, 1, 0.7
 *   endin
 *
 * Build:
 *   make -C Csound/opcode/
 */

#include <csdl.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/* ── Limits ──────────────────────────────────────────────────────────────── */
#define MAX_ENTRIES  2048
#define MAX_STR       128
#define MAX_SCALES     16
#define MAX_PAIRS     256

/* ── Data row ─────────────────────────────────────────────────────────────── */
typedef struct {
    char   emotion[MAX_STR];
    char   scale[MAX_STR];
    char   key[MAX_STR];
    char   chord_name[MAX_STR];
    double weight;
} GenEntry;

/* ── Global data store (loaded once via chord_gen_init) ──────────────────── */
static struct {
    int      loaded;
    GenEntry entries[MAX_ENTRIES];
    int      n;
} g_chord_gen;

/* ── Chord name → MIDI notes (identical logic to chord_play) ─────────────── */
static const struct { const char *name; int pc; } NOTE_MAP[] = {
    {"C#", 1}, {"Cb",11}, {"D#", 3}, {"Db", 1},
    {"Eb", 3}, {"Fb", 4}, {"F#", 6}, {"Gb", 6},
    {"G#", 8}, {"Ab", 8}, {"A#",10}, {"Bb",10},
    {"C",  0}, {"D",  2}, {"E",  4}, {"F",  5},
    {"G",  7}, {"A",  9}, {"B", 11},
    {NULL, -1}
};

static const struct { const char *qual; int n; int iv[5]; } QUAL_MAP[] = {
    {"m7b5", 4, {0, 3, 6,10, 0}},
    {"maj7", 4, {0, 4, 7,11, 0}},
    {"dim7", 4, {0, 3, 6, 9, 0}},
    {"m7",   4, {0, 3, 7,10, 0}},
    {"m6",   4, {0, 3, 7, 9, 0}},
    {"7",    4, {0, 4, 7,10, 0}},
    {"sus",  3, {0, 5, 7, 0, 0}},
    {"dim",  3, {0, 3, 6, 0, 0}},
    {"m",    3, {0, 3, 7, 0, 0}},
    {"",     3, {0, 4, 7, 0, 0}},
    {NULL,   0, {0, 0, 0, 0, 0}}
};

static int parse_chord(const char *tok, int octave, int out[8])
{
    int root_pc = -1, root_len = 0;
    for (int i = 0; NOTE_MAP[i].name; i++) {
        int l = (int)strlen(NOTE_MAP[i].name);
        if (strncmp(tok, NOTE_MAP[i].name, l) == 0) {
            root_pc = NOTE_MAP[i].pc; root_len = l; break;
        }
    }
    if (root_pc < 0) return -1;
    int         root_midi = 12 * (octave + 1) + root_pc;
    const char *qual      = tok + root_len;
    for (int q = 0; QUAL_MAP[q].qual; q++) {
        if (strcmp(qual, QUAL_MAP[q].qual) == 0) {
            for (int k = 0; k < QUAL_MAP[q].n; k++)
                out[k] = root_midi + QUAL_MAP[q].iv[k];
            return QUAL_MAP[q].n;
        }
    }
    return -1;
}

/* Schedule every chord token in a dash-separated chord_name string. */
static void schedule_progression(CSOUND *csound, const char *chord_name,
                                  double start, double dur,
                                  double instr, double amp, int octave)
{
    char  buf[512];
    strncpy(buf, chord_name, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, "-");
    while (tok) {
        int notes[8];
        int n = parse_chord(tok, octave, notes);
        for (int i = 0; i < n; i++) {
            EVTBLK evt;
            memset(&evt, 0, sizeof(EVTBLK));
            evt.opcod  = 'i';
            evt.pcnt   = 5;
            evt.p[1]   = (MYFLT)instr;
            evt.p[2]   = (MYFLT)start;
            evt.p2orig = (MYFLT)start;
            evt.p[3]   = (MYFLT)dur;
            evt.p3orig = (MYFLT)dur;
            evt.p[4]   = (MYFLT)notes[i];
            evt.p[5]   = (MYFLT)amp;
            evt.strarg = NULL;
            evt.scnt   = 0;
            csound->insert_score_event(csound, &evt, FL(0.0));
        }
        start += dur;
        tok = strtok(NULL, "-");
    }
}

/* ── Sampling utilities ──────────────────────────────────────────────────── */

/* Apply temperature scaling in-place: w_i = w_i^(1/T), then normalise. */
static void apply_temperature(double *w, int n, double T)
{
    if (T < 0.01) T = 0.01;
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        w[i] = pow(w[i] > 0.0 ? w[i] : 1e-10, 1.0 / T);
        sum += w[i];
    }
    if (sum > 0.0)
        for (int i = 0; i < n; i++) w[i] /= sum;
}

/* Weighted random sample — returns index into w[]. */
static int weighted_sample(const double *w, int n)
{
    double r      = (double)rand() / ((double)RAND_MAX + 1.0);
    double cumsum = 0.0;
    for (int i = 0; i < n; i++) {
        cumsum += w[i];
        if (r < cumsum) return i;
    }
    return n - 1;
}

/* ── TSV loader ──────────────────────────────────────────────────────────── */
static int load_tsv(CSOUND *csound, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        csound->Message(csound,
            "chord_gen_init: cannot open '%s'\n", path);
        return NOTOK;
    }

    char line[1024];
    int  n = 0, header = 1;

    while (fgets(line, sizeof(line), f) && n < MAX_ENTRIES) {
        if (header) { header = 0; continue; }

        /* strip trailing newline/carriage return */
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* split on tabs: emotion \t scale \t key \t chord_name \t weight */
        char *fields[5], *p = line;
        int   nf = 0;
        fields[nf++] = p;
        while (*p && nf < 5) {
            if (*p == '\t') { *p = '\0'; fields[nf++] = p + 1; }
            p++;
        }
        if (nf < 5) continue;

        GenEntry *e = &g_chord_gen.entries[n++];
        strncpy(e->emotion,    fields[0], MAX_STR - 1);
        strncpy(e->scale,      fields[1], MAX_STR - 1);
        strncpy(e->key,        fields[2], MAX_STR - 1);
        strncpy(e->chord_name, fields[3], MAX_STR - 1);
        e->weight = atof(fields[4]);
    }

    fclose(f);
    g_chord_gen.n      = n;
    g_chord_gen.loaded = 1;
    csound->Message(csound,
        "chord_gen: loaded %d chord entries from %s\n", n, path);
    return OK;
}

/* ── chord_gen_init opcode ───────────────────────────────────────────────── */
typedef struct { OPDS h; STRINGDAT *SPath; } CHORD_GEN_INIT;

static int chord_gen_init_fn(CSOUND *csound, CHORD_GEN_INIT *p)
{
    if (g_chord_gen.loaded) return OK;   /* idempotent */
    srand((unsigned int)time(NULL));
    return load_tsv(csound, p->SPath->data);
}

/* ── chord_gen opcode ────────────────────────────────────────────────────── */
typedef struct {
    OPDS       h;
    STRINGDAT *SEmotion;
    MYFLT     *iInstr;
    MYFLT     *iStart;
    MYFLT     *iDur;
    MYFLT     *iAmp;
    MYFLT     *iTemp;     /* j-type: default -1 → 1.0 (natural distribution) */
    MYFLT     *iOctave;   /* j-type: default -1 → 4   (C4 = MIDI 60)         */
} CHORD_GEN;

static int chord_gen_fn(CSOUND *csound, CHORD_GEN *p)
{
    if (!g_chord_gen.loaded) {
        csound->Message(csound,
            "chord_gen: no data loaded — call chord_gen_init first\n");
        return NOTOK;
    }

    const char *emotion = p->SEmotion->data;
    double      T       = (*p->iTemp   < FL(0.0)) ? 1.0 : (double)*p->iTemp;
    int         octave  = (*p->iOctave < FL(0.0)) ? 4   : (int)*p->iOctave;
    double      instr   = (double)*p->iInstr;
    double      start   = (double)*p->iStart;
    double      dur     = (double)*p->iDur;
    double      amp     = (double)*p->iAmp;

    /* ── Step 1: collect unique scales for this emotion ─────────────────── */
    char   scales[MAX_SCALES][MAX_STR];
    double scale_w[MAX_SCALES];
    int    n_scales = 0;

    for (int i = 0; i < g_chord_gen.n; i++) {
        GenEntry *e = &g_chord_gen.entries[i];
        if (strcasecmp(e->emotion, emotion) != 0) continue;
        int found = -1;
        for (int s = 0; s < n_scales; s++)
            if (strcasecmp(scales[s], e->scale) == 0) { found = s; break; }
        if (found < 0 && n_scales < MAX_SCALES) {
            strncpy(scales[n_scales], e->scale, MAX_STR - 1);
            scale_w[n_scales] = 0.0;
            found = n_scales++;
        }
        if (found >= 0) scale_w[found] += e->weight;
    }

    if (n_scales == 0) {
        csound->Message(csound,
            "chord_gen: unknown emotion '%s'\n", emotion);
        return NOTOK;
    }

    /* ── Step 2: temperature-sample a scale ─────────────────────────────── */
    apply_temperature(scale_w, n_scales, T);
    const char *scale = scales[weighted_sample(scale_w, n_scales)];

    /* ── Step 3: collect pairs for (emotion, scale) ──────────────────────── */
    const char *pair_chord[MAX_PAIRS];
    double      pair_w[MAX_PAIRS];
    int         n_pairs = 0;

    for (int i = 0; i < g_chord_gen.n; i++) {
        GenEntry *e = &g_chord_gen.entries[i];
        if (strcasecmp(e->emotion, emotion) != 0) continue;
        if (strcasecmp(e->scale,   scale)   != 0) continue;
        if (n_pairs < MAX_PAIRS) {
            pair_chord[n_pairs] = e->chord_name;
            pair_w[n_pairs]     = e->weight;
            n_pairs++;
        }
    }

    /* ── Step 4: temperature-sample a chord progression ─────────────────── */
    apply_temperature(pair_w, n_pairs, T);
    const char *chosen = pair_chord[weighted_sample(pair_w, n_pairs)];

    /* ── Step 5: schedule note events ────────────────────────────────────── */
    schedule_progression(csound, chosen, start, dur, instr, amp, octave);
    return OK;
}

/* ── Opcode table ────────────────────────────────────────────────────────── */
static OENTRY localops[] = {
    {
        "chord_gen_init", sizeof(CHORD_GEN_INIT),
        0, 1, "", "S",
        (SUBR)chord_gen_init_fn, NULL, NULL, NULL
    },
    {
        "chord_gen", sizeof(CHORD_GEN),
        0, 1, "", "Siiiijj",
        (SUBR)chord_gen_fn, NULL, NULL, NULL
    },
};

/* ── Module entry points ─────────────────────────────────────────────────── */
PUBLIC int csoundModuleCreate(CSOUND *csound)  { IGN(csound); return 0; }
PUBLIC int csoundModuleDestroy(CSOUND *csound) { IGN(csound); return 0; }

PUBLIC int csoundModuleInit(CSOUND *csound)
{
    return csound->AppendOpcodes(csound, localops,
        (int)(sizeof(localops) / sizeof(localops[0])));
}
