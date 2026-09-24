# Native YuE2 provider — capabilities and limits

**Provider id:** `yue2-native`  
**Engine:** [`audio.cpp`](https://github.com/0xShug0/audio.cpp) (family `yue2`), launched by `ai_runtime_host`.  
**Not** yue2.cpp. The two native ports expose different options; this note records what *our* engine supports.

## Supported request options

Taken from `audio.cpp`'s yue2 request parser (`src/models/yue2/request.cpp`):

| Option | Notes |
| --- | --- |
| `style` | required |
| `lyrics` | required (the CLI text input is used if omitted) |
| `cot` | `off` \| `melody` \| `full` — score route |
| `seed` | token seed, `[0, 2^63)` |
| `guidance_scale` | alias `cfg_scale`, `[0, 20]` |
| `num_inference_steps` | NAR midpoint ODE steps |
| `abc` / `abc_file` | external score; **requires `cot=melody` or `cot=full`** |
| `nar_noise_file` | raw float32 acoustic noise (64 columns) |
| `abc_*` / `semantic_*` | `temperature`, `top_p`, `top_k`, `repetition_penalty`, `penalty_window`, `min_tokens`, `max_tokens` for each stage |

There is **no `duration`** and **no `semantic_tokens`** option on this engine.

## What the Create panel sends

`style`, `lyrics`, `cot` (Score selector), `seed`, `steps` → `num_inference_steps`, `guidance_scale`, and an
`options` object carrying the ABC/semantic sampling knobs. The value actually used for `seed` is chosen
randomly when the Seed box is empty and is reported in the status and stored in the request.

## Editing the score and regenerating

1. A generation writes `score.abc`; the ABC body is parsed into the Song Plan (tempo, key, meter, sections, chords, melody).
2. Editing the plan and pressing **Regenerate** serialises the plan back to ABC (`writeAbcPlan`), which the host writes to
   `plan.abc` and passes as `--request-option abc_file=<path>` with `cot=full`.
3. The engine consumes that score as the plan, so the composition follows the edit.

`writeAbcPlan` round-trips through `parseAbcPlan` (tempo, key, meter, sections, chord symbols, note pitches and durations),
covered by unit tests.

## Prompt files

Prompts can be imported/exported as JSON or YAML using the engine's own field names (`style`, `lyrics`, `abc`, `cot`,
`seed`, `steps`, `guidance_scale`, `abc_*`/`semantic_*`), sparse (empty fields omitted). Unknown keys are ignored on
import, so files from other tools (for example a yue2.cpp studio that also writes `lm_seed`, `duration`,
`semantic_tokens`) load the fields they share.

## Not available on this engine

These are real features of yue2.cpp–based studios (such as YuE2-Studio) that `audio.cpp` does **not** expose, so they are
constraints here, not bugs:

- **Exact replay / performance variations.** No semantic token stream or generated acoustic noise is ever written, so a
  finished song cannot be re-rendered from saved codes. The only replay-adjacent control is supplying `nar_noise_file`
  yourself; the engine never emits it. `pipeline.cpp` only logs `nar_noise=generated|provided`.
- **Score-only ("plan") preview.** `cot` chooses whether/which ABC is generated but always renders audio; there is no
  audio-less plan task. Iterating on structure means editing the Song Plan and regenerating.
- **Target duration.** No `duration` option; the model decides the length.
- **Separate composition vs performance seed.** `audio.cpp` has one `seed` (plus `nar_noise_file`), not the
  `lm_seed`/sound-seed pair some tools use.

## Persistence

- Each job folder holds `request.json` (job id, provider, timestamp, and the raw parameters) alongside `output.wav`,
  `score.abc` and `result.json`.
- The workspace manifest (`manifest.json`) keeps the job list, a track→job map (`trackJobs`) and a title→job map
  (`jobTitles`).
- The generated clip's title carries its job id, so the clip↔job link survives a project reload (track and clip ids are
  reassigned on load).