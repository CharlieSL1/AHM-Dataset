# Workflow for cc: Generate Machine Learning CSV from Jazz Harmony Data

## Goal

Create a machine-learning-ready `.csv` file based on the user’s Jazz harmony data.

The final CSV must follow this schema exactly:

```csv
Key,Chord_Progression,ChordName,Voicing,Emotion,Scale
C,II-V-I,Dm7-G7-CMaj7,Root position,Happy,Ionian
```

Required columns:

| Key | Chord_Progression | ChordName | Voicing | Emotion | Scale |
|---|---|---|---|---|---|
| C | II-V-I | Dm7-G7-CMaj7 | Root position | Happy | Ionian |

Do not add extra columns unless the user explicitly asks.

---

## Input Sources

Use these files as the source of truth:

1. **Google Sheet file: `Jazz harmony data tracking`**
   - Read the chord progression information from this Google Sheet.
   - Identify the relevant fields related to:
     - Key
     - Chord progression in Roman numerals
     - Actual chord names
     - Voicing
     - Scale or mode
     - Any user notes or comments that explain formatting or interpretation

2. **User’s existing file structure and notes**
   - Follow the structure and comments already present in the user’s file.
   - Do not invent a new organization style if the user’s files already imply a preferred format.
   - Preserve the user’s naming style whenever possible.

3. **Emotion reference `.txt` file inside the folder**
   - Use this `.txt` file as the source of truth for key-specific emotions.
   - The emotion for each row must be assigned according to this text file.
   - If the Google Sheet and the `.txt` file disagree, the `.txt` file wins for the `Emotion` column.

---

## Output File

Create one CSV file for machine learning.

Recommended filename:

```text
jazz_harmony_ml_dataset.csv
```

The output must be a valid comma-separated CSV file with UTF-8 encoding.

The first row must be the header:

```csv
Key,Chord_Progression,ChordName,Voicing,Emotion,Scale
```

Each following row should represent one usable chord-progression data entry.

---

## Column Definitions

### 1. `Key`

The tonal center or key of the progression.

Examples:

```text
C
F
Bb
Eb
A minor
D minor
```

Rules:

- Use the key exactly as indicated in the Google Sheet, unless the user’s notes specify a preferred spelling.
- Keep flat keys as `Bb`, `Eb`, `Ab`, etc.
- Keep sharp keys as `F#`, `C#`, etc.
- Do not convert enharmonic spellings unless the user’s notes require it.

---

### 2. `Chord_Progression`

The Roman numeral version of the progression.

Examples:

```text
II-V-I
I-VI-II-V
I-IV-V
III-VI-II-V-I
```

Rules:

- Use Roman numerals from the Google Sheet.
- Keep the formatting consistent:
  - Use hyphens between chords.
  - Avoid extra spaces.
  - Use uppercase/lowercase exactly as the user’s source implies.
- If the sheet contains variants like `ii–V–I`, normalize the separators to regular hyphens: `ii-V-I`.

---

### 3. `ChordName`

The concrete chord names for the progression.

Examples:

```text
Dm7-G7-CMaj7
Am7-D7-GMaj7
F#m7b5-B7-Em7
```

Rules:

- Use chord names from the Google Sheet.
- Separate chords with hyphens.
- Preserve chord qualities such as:
  - `m7`
  - `Maj7`
  - `7`
  - `m7b5`
  - `dim7`
  - `sus4`
  - `alt`
- Do not simplify chord names unless the user’s notes require it.

---

### 4. `Voicing`

The voicing description for the chord or progression.

Examples:

```text
Root position
Drop 2
Shell voicing
Open voicing
Closed voicing
Guide tone
```

Rules:

- Use the voicing information from the Google Sheet or the user’s notes.
- If a row does not clearly specify voicing, use:

```text
Unknown
```

- Do not leave the field blank.

---

### 5. `Emotion`

The emotional label connected to the key.

Examples:

```text
Happy
Dark
Warm
Bright
Melancholic
Tense
Dreamy
Calm
```

Rules:

- Assign `Emotion` based on the key-specific emotion mapping in the `.txt` file.
- The `.txt` file is the authority for this column.
- Match the row’s `Key` to the corresponding emotion in the `.txt` file.
- If a key is missing from the `.txt` file, use:

```text
Unknown
```

- Do not guess the emotion based only on music theory.

---

### 6. `Scale`

The scale or mode associated with the progression, chord, or key.

Examples:

```text
Ionian
Dorian
Mixolydian
Lydian
Aeolian
Harmonic minor
Melodic minor
Blues scale
```

Rules:

- Use scale information from the Google Sheet or the user’s notes.
- If the scale is not specified but clearly implied by the row’s notes, use the implied scale.
- If the scale cannot be confirmed, use:

```text
Unknown
```

- Do not leave the field blank.

---

## Processing Steps

### Step 1: Locate and read the source files

Find and open:

```text
Jazz harmony data tracking
```

Then locate the folder containing the related `.txt` emotion reference file.

Read both before generating the CSV.

---

### Step 2: Understand the user’s file structure

Before writing the CSV, inspect the user’s files and notes.

Pay attention to:

- Existing column names
- Comments or remarks
- Naming style
- Progression formatting
- Chord spelling
- Key spelling
- Scale/mode labels
- Any repeated patterns in how the user organizes harmony data

Follow the existing structure instead of redesigning it.

---

### Step 3: Extract chord progression data from the Google Sheet

For each usable row in `Jazz harmony data tracking`, extract:

```text
Key
Chord_Progression
ChordName
Voicing
Scale
```

Do not include rows that are clearly:

- Empty
- Draft notes only
- Section headers
- Explanations without usable chord data
- Duplicate rows, unless the duplicate has a meaningful difference such as a different voicing, scale, or emotion

---

### Step 4: Read key-specific emotions from the `.txt` file

Parse the `.txt` file into a key-to-emotion mapping.

Example mapping:

```text
C = Happy
D minor = Melancholic
Eb = Warm
F# = Tense
```

Accept common formats such as:

```text
C: Happy
C - Happy
C = Happy
C, Happy
```

Then use this mapping to fill the `Emotion` column.

---

### Step 5: Build each CSV row

Each row must follow this exact order:

```csv
Key,Chord_Progression,ChordName,Voicing,Emotion,Scale
```

Example:

```csv
C,II-V-I,Dm7-G7-CMaj7,Root position,Happy,Ionian
```

If a field is unknown, write `Unknown`.

Do not use empty cells.

---

### Step 6: Normalize formatting

Before saving the CSV, clean the formatting:

- Replace long dashes such as `–` or `—` with regular hyphens `-`.
- Remove unnecessary spaces around hyphens.
- Trim extra spaces at the beginning or end of every field.
- Use consistent capitalization based on the user’s original file.
- Keep chord symbols musically accurate.
- Keep all six required columns.

Examples:

```text
Dm7 – G7 – CMaj7
```

Should become:

```text
Dm7-G7-CMaj7
```

---

### Step 7: Validate the CSV

Before finalizing, check:

- The CSV opens correctly.
- The header is exactly:

```csv
Key,Chord_Progression,ChordName,Voicing,Emotion,Scale
```

- Every row has exactly 6 columns.
- No required field is empty.
- `Emotion` values come from the `.txt` file whenever possible.
- `Chord_Progression` and `ChordName` use hyphen-separated formatting.
- There are no accidental line breaks inside cells.
- The file is saved as UTF-8.

---

## Handling Missing or Ambiguous Information

Use these fallback rules:

| Situation | Action |
|---|---|
| Missing voicing | Use `Unknown` |
| Missing scale | Use `Unknown` |
| Missing emotion for a key in the `.txt` file | Use `Unknown` |
| Conflicting emotion between Google Sheet and `.txt` | Use the `.txt` value |
| Unclear chord spelling | Preserve the Google Sheet spelling |
| Duplicate progression with identical fields | Keep only one |
| Duplicate progression with different voicing or scale | Keep both |
| Row is only a note or heading | Skip it |

---

## Required Output Quality

The CSV should be clean enough for machine learning preprocessing.

That means:

- Consistent column names
- Consistent separators
- No blank required fields
- No mixed formatting like both `II - V - I` and `II-V-I`
- No unnecessary explanatory text inside cells
- No extra metadata rows
- No Markdown table formatting inside the CSV
- No index column
- No row numbers unless the user asks for them

---

## Final Deliverables

Return:

1. The finished CSV file:

```text
jazz_harmony_ml_dataset.csv
```

2. A short note explaining:
   - How many rows were created
   - Which source files were used
   - Whether any fields were filled as `Unknown`
   - Whether there were any skipped rows or duplicates

---

## Example Final CSV

```csv
Key,Chord_Progression,ChordName,Voicing,Emotion,Scale
C,II-V-I,Dm7-G7-CMaj7,Root position,Happy,Ionian
F,I-VI-II-V,FMaj7-Dm7-Gm7-C7,Shell voicing,Warm,Ionian
D minor,II-V-I,Em7b5-A7-Dm7,Guide tone,Melancholic,Harmonic minor
Bb,III-VI-II-V-I,Dm7-G7-Cm7-F7-BbMaj7,Drop 2,Bright,Mixolydian
```

---

## Prompt to Give cc

```text
Please create a machine-learning-ready CSV file from my Jazz harmony data.

Use the Google Sheet named "Jazz harmony data tracking" to read the chord progression data. Follow the structure, formatting, and notes already present in my files. The CSV must use this exact schema:

Key,Chord_Progression,ChordName,Voicing,Emotion,Scale

For the Emotion column, use the key-specific emotion mapping from the .txt file in the folder. The .txt file is the source of truth for emotion. If the Google Sheet and the .txt file disagree, use the .txt value.

For each usable row, extract the key, Roman numeral chord progression, concrete chord names, voicing, emotion, and scale. Normalize separators to regular hyphens, remove extra spaces, preserve chord qualities, and do not leave any required field blank. If voicing, scale, or emotion cannot be confirmed, write "Unknown".

Skip empty rows, note-only rows, section headers, and exact duplicates. Keep duplicates only if they differ in voicing, scale, or another meaningful field.

Save the final file as:

jazz_harmony_ml_dataset.csv

Before finishing, validate that every row has exactly six columns, the header is exactly correct, all required cells are filled, and the CSV is UTF-8 encoded.

After creating the CSV, tell me how many rows were created, which files you used, whether any fields were filled as Unknown, and whether any rows were skipped or deduplicated.
```
