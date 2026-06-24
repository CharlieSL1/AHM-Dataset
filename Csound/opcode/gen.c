/*
 * gen.c — Csound plugin opcode: emoChord / emoChord_init
 *
 * Architecture:
 *   train_model.py trains a Random Forest (scikit-learn + Optuna) and exports
 *   it as gen_model.onnx via skl2onnx.  At Csound startup, emoChord_init loads
 *   the ONNX model via ONNX Runtime and a vocabulary lookup table from
 *   gen_data.tsv.  At score time, emoChord runs the ONNX model,
 *   temperature-samples a chord progression, prints the chord name to the
 *   console, and schedules note events.
 *
 * Opcodes:
 *   emoChord_init  SModelPath, SDataPath
 *   emoChord       SEmotion, iInstr, iStart, iDur, iAmp [, iTemp, iOctave]
 *
 * Score usage:
 *   ; <CsScore>
 *   i1  0  4  "joyful"
 *
 *   ; <CsInstruments>
 *   instr 1
 *     Sem strget p4
 *     emoChord Sem, 2, p2, p3, 0.7
 *   endin
 *
 * Parameters:
 *   SModelPath — absolute path to gen_model.onnx
 *   SDataPath  — absolute path to gen_data.tsv
 *   SEmotion   — emotion string, case-insensitive (e.g. "Joyful", "joyful")
 *   iInstr     — synthesis instrument number (receives p4=MIDI note, p5=amp)
 *   iStart     — score start time in seconds for the first chord
 *   iDur       — duration per chord in seconds
 *   iAmp       — amplitude 0–1
 *   iTemp      — sampling temperature (optional, default 1.0)
 *                  0 = deterministic (argmax), 1 = model distribution, >1 = random
 *   iOctave    — root octave (optional, default 4 → C4 = MIDI 60)
 *
 * Build:
 *   make -C Csound/opcode/
 */

#include <csdl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <math.h>
#include <time.h>

#include "onnxruntime_c_api.h"

#define MAX_ENTRIES   2048
#define MAX_STR        128
#define MAX_SCALES      16
#define MAX_CLASSES    256
#define MAX_PAIRS      256

/* ── Error checking macro ────────────────────────────────────────────────── */
#define ORT_CHECK(expr) do { \
    OrtStatus *_s = (OrtStatus *)(expr); \
    if (_s) { \
        csound->Message(csound, "emoChord: ORT error: %s\n", \
                        g_ort->GetErrorMessage(_s)); \
        g_ort->ReleaseStatus(_s); \
        return NOTOK; \
    } \
} while (0)

/* ── Data entry (one row of gen_data.tsv) ────────────────────────────────── */
typedef struct {
    char emotion[MAX_STR];
    char scale[MAX_STR];
    int  emotion_id;
    int  scale_id;
    int  prog_id;
    char key[MAX_STR];
    char chord_name[MAX_STR];
} DataEntry;

/* ── Global ONNX Runtime state ───────────────────────────────────────────── */
static const OrtApi  *g_ort     = NULL;
static OrtEnv        *g_env     = NULL;
static OrtSession    *g_session = NULL;
static OrtMemoryInfo *g_mem     = NULL;

/* ── Global lookup table ─────────────────────────────────────────────────── */
static struct {
    int       loaded;
    DataEntry entries[MAX_ENTRIES];
    int       n;
    int       n_classes;   /* number of output classes, read from model */
} g_gen;

/* ── Softmax + temperature sampling ──────────────────────────────────────── */
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

static int weighted_sample(const double *w, int n)
{
    double r = (double)rand() / ((double)RAND_MAX + 1.0);
    double c = 0.0;
    for (int i = 0; i < n; i++) {
        c += w[i];
        if (r < c) return i;
    }
    return n - 1;
}

/* ── Chord name → MIDI notes ─────────────────────────────────────────────── */
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
    if (root_pc < 0) return 0;
    int         base = 12 * (octave + 1) + root_pc;
    const char *qual = tok + root_len;
    for (int q = 0; QUAL_MAP[q].qual; q++) {
        if (strcmp(qual, QUAL_MAP[q].qual) == 0) {
            for (int k = 0; k < QUAL_MAP[q].n; k++)
                out[k] = base + QUAL_MAP[q].iv[k];
            return QUAL_MAP[q].n;
        }
    }
    return 0;
}

static void schedule_progression(CSOUND *csound, const char *chord_name,
                                  double start, double dur,
                                  double instr, double amp, int octave)
{
    char buf[512];
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

/* ── gen_init: load ONNX model + lookup table ────────────────────────────── */

static int load_data(CSOUND *csound, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        csound->Message(csound, "emoChord_init: cannot open data '%s'\n", path);
        return NOTOK;
    }

    char line[1024];
    int  n = 0, header = 1;
    while (fgets(line, sizeof(line), f) && n < MAX_ENTRIES) {
        if (header) { header = 0; continue; }
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* 7 tab-separated fields:
         * emotion  scale  emotion_id  scale_id  prog_id  key  chord_name */
        char *fields[7], *p = line;
        int   nf = 0;
        fields[nf++] = p;
        while (*p && nf < 7) {
            if (*p == '\t') { *p = '\0'; fields[nf++] = p + 1; }
            p++;
        }
        if (nf < 7) continue;

        DataEntry *e = &g_gen.entries[n++];
        strncpy(e->emotion,    fields[0], MAX_STR - 1);
        strncpy(e->scale,      fields[1], MAX_STR - 1);
        e->emotion_id = atoi(fields[2]);
        e->scale_id   = atoi(fields[3]);
        e->prog_id    = atoi(fields[4]);
        strncpy(e->key,        fields[5], MAX_STR - 1);
        strncpy(e->chord_name, fields[6], MAX_STR - 1);
    }
    fclose(f);
    g_gen.n = n;
    csound->Message(csound, "emoChord: loaded %d entries from %s\n", n, path);
    return OK;
}

typedef struct {
    OPDS       h;
    STRINGDAT *SModel;
    STRINGDAT *SData;
} EMOCHORD_INIT;

static int emochord_init_fn(CSOUND *csound, EMOCHORD_INIT *p)
{
    if (g_gen.loaded) return OK;   /* idempotent */
    srand((unsigned int)time(NULL));

    /* ── Initialise ONNX Runtime ─────────────────────────────────────────── */
    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    ORT_CHECK(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "gen", &g_env));

    OrtSessionOptions *opts = NULL;
    ORT_CHECK(g_ort->CreateSessionOptions(&opts));
    ORT_CHECK(g_ort->SetIntraOpNumThreads(opts, 1));

    ORT_CHECK(g_ort->CreateSession(g_env, p->SModel->data, opts, &g_session));
    g_ort->ReleaseSessionOptions(opts);

    ORT_CHECK(g_ort->CreateCpuMemoryInfo(
        OrtArenaAllocator, OrtMemTypeDefault, &g_mem));

    /* Read number of output classes from "probabilities" (output index 1) */
    OrtTypeInfo *type_info = NULL;
    g_ort->SessionGetOutputTypeInfo(g_session, 1, &type_info);
    const OrtTensorTypeAndShapeInfo *shape_info = NULL;
    g_ort->CastTypeInfoToTensorInfo(type_info, &shape_info);
    size_t n_dims = 0;
    g_ort->GetDimensionsCount(shape_info, &n_dims);
    int64_t dims[8] = {0};
    g_ort->GetDimensions(shape_info, dims, n_dims);
    g_gen.n_classes = (int)(n_dims >= 2 ? dims[1] : 80);
    g_ort->ReleaseTypeInfo(type_info);

    csound->Message(csound,
        "emoChord: ONNX model loaded  output_classes=%d\n", g_gen.n_classes);

    /* ── Load lookup table ───────────────────────────────────────────────── */
    if (load_data(csound, p->SData->data) != OK) return NOTOK;

    g_gen.loaded = 1;
    return OK;
}

/* ── gen: ONNX inference + chord scheduling ──────────────────────────────── */
typedef struct {
    OPDS       h;
    STRINGDAT *SEmotion;
    MYFLT     *iInstr;
    MYFLT     *iStart;
    MYFLT     *iDur;
    MYFLT     *iAmp;
    MYFLT     *iTemp;    /* j-type: default -1 → 1.0 (model distribution) */
    MYFLT     *iOctave;  /* j-type: default -1 → 4   (C4 = MIDI 60)        */
} EMOCHORD_OP;

static int emochord_fn(CSOUND *csound, EMOCHORD_OP *p)
{
    if (!g_gen.loaded) {
        csound->Message(csound,
            "emoChord: not initialised — call emoChord_init first\n");
        return NOTOK;
    }

    const char *emotion = p->SEmotion->data;
    double T      = (*p->iTemp   < FL(0.0)) ? 1.0 : (double)*p->iTemp;
    int    octave = (*p->iOctave < FL(0.0)) ? 4   : (int)*p->iOctave;

    /* ── Step 1: find emotion_id, collect unique scale_ids ──────────────── */
    int    emotion_id = -1;
    int    scale_ids[MAX_SCALES];
    double scale_counts[MAX_SCALES];
    int    n_scales = 0;

    for (int i = 0; i < g_gen.n; i++) {
        DataEntry *e = &g_gen.entries[i];
        if (strcasecmp(e->emotion, emotion) != 0) continue;
        if (emotion_id < 0) emotion_id = e->emotion_id;
        int found = -1;
        for (int s = 0; s < n_scales; s++)
            if (scale_ids[s] == e->scale_id) { found = s; break; }
        if (found < 0 && n_scales < MAX_SCALES) {
            scale_ids[n_scales]    = e->scale_id;
            scale_counts[n_scales] = 0.0;
            found = n_scales++;
        }
        if (found >= 0) scale_counts[found] += 1.0;
    }

    if (emotion_id < 0) {
        csound->Message(csound, "emoChord: unknown emotion '%s'\n", emotion);
        return NOTOK;
    }

    /* ── Step 2: sample a scale (weighted by data frequency) ────────────── */
    apply_temperature(scale_counts, n_scales, T);
    int scale_id = scale_ids[weighted_sample(scale_counts, n_scales)];

    /* ── Step 3: ONNX inference → logits[n_classes] ──────────────────────── */
    float   input_data[2] = { (float)emotion_id, (float)scale_id };
    int64_t input_shape[] = { 1, 2 };

    OrtValue *in_tensor  = NULL;
    OrtValue *out_tensor = NULL;

    OrtStatus *s;
    s = g_ort->CreateTensorWithDataAsOrtValue(
            g_mem, input_data, 2 * sizeof(float),
            input_shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &in_tensor);
    if (s) {
        csound->Message(csound, "emoChord: ORT input error: %s\n",
                        g_ort->GetErrorMessage(s));
        g_ort->ReleaseStatus(s);
        return NOTOK;
    }

    const char *in_names[]  = { "input"         };
    const char *out_names[] = { "probabilities" };

    s = g_ort->Run(g_session, NULL,
                   in_names,  (const OrtValue* const*)&in_tensor,  1,
                   out_names, 1, &out_tensor);
    if (s) {
        csound->Message(csound, "emoChord: ORT run error: %s\n",
                        g_ort->GetErrorMessage(s));
        g_ort->ReleaseStatus(s);
        g_ort->ReleaseValue(in_tensor);
        return NOTOK;
    }

    /* "probabilities" output is already normalised by the Random Forest */
    float *raw_probs = NULL;
    g_ort->GetTensorMutableData(out_tensor, (void**)&raw_probs);
    float probs[MAX_CLASSES];
    for (int k = 0; k < g_gen.n_classes; k++) probs[k] = raw_probs[k];

    g_ort->ReleaseValue(out_tensor);
    g_ort->ReleaseValue(in_tensor);

    /* ── Step 4: filter to prog_ids observed for (emotion_id, scale_id) ──── */
    int    prog_ids[MAX_CLASSES];
    double prog_w  [MAX_CLASSES];
    int    n_progs = 0;

    for (int i = 0; i < g_gen.n; i++) {
        DataEntry *e = &g_gen.entries[i];
        if (e->emotion_id != emotion_id || e->scale_id != scale_id) continue;
        int found = -1;
        for (int q = 0; q < n_progs; q++)
            if (prog_ids[q] == e->prog_id) { found = q; break; }
        if (found < 0 && n_progs < MAX_CLASSES) {
            prog_ids[n_progs] = e->prog_id;
            prog_w  [n_progs] = (e->prog_id < g_gen.n_classes)
                                ? (double)probs[e->prog_id] : 1e-10;
            found = n_progs++;
        }
    }

    if (n_progs == 0) {
        csound->Message(csound,
            "emoChord: no progressions found for emotion '%s'\n", emotion);
        return NOTOK;
    }

    /* ── Step 5: temperature-sample a prog_id from model probabilities ───── */
    apply_temperature(prog_w, n_progs, T);
    int chosen_prog = prog_ids[weighted_sample(prog_w, n_progs)];

    /* ── Step 6: collect chord_names for (emotion, scale, chosen_prog) ───── */
    const char *candidates[MAX_PAIRS];
    int         n_cands = 0;

    for (int i = 0; i < g_gen.n; i++) {
        DataEntry *e = &g_gen.entries[i];
        if (e->emotion_id != emotion_id) continue;
        if (e->scale_id   != scale_id)   continue;
        if (e->prog_id    != chosen_prog) continue;
        if (n_cands < MAX_PAIRS) candidates[n_cands++] = e->chord_name;
    }

    const char *chord_name = candidates[rand() % n_cands];

    /* ── Step 7: print chord name → user; schedule note events → Csound ──── */
    csound->Message(csound, "emoChord [%s]: %s\n", emotion, chord_name);
    schedule_progression(csound, chord_name,
                         (double)*p->iStart, (double)*p->iDur,
                         (double)*p->iInstr, (double)*p->iAmp, octave);
    return OK;
}

/* ── Opcode registration ─────────────────────────────────────────────────── */
static OENTRY localops[] = {
    {
        "emoChord_init", sizeof(EMOCHORD_INIT),
        0, 1, "", "SS",
        (SUBR)emochord_init_fn, NULL, NULL, NULL
    },
    {
        "emoChord", sizeof(EMOCHORD_OP),
        0, 1, "", "Siiiijj",
        (SUBR)emochord_fn, NULL, NULL, NULL
    },
};

PUBLIC int csoundModuleCreate(CSOUND *csound)  { IGN(csound); return 0; }
PUBLIC int csoundModuleDestroy(CSOUND *csound) { IGN(csound); return 0; }

PUBLIC int csoundModuleInit(CSOUND *csound)
{
    return csound->AppendOpcodes(csound, localops,
        (int)(sizeof(localops) / sizeof(localops[0])));
}
