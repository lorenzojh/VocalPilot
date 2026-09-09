# Permission-cleared vocal corpus

No human recordings are included. Add your own dry, isolated voice locally in
the category folders. WAV/FLAC/MP3 files here are ignored; metadata remains
trackable. Never add a commercial acapella without permission. Do not force-add
private audio just to make a regression test pass.

Use pseudonymous fixture IDs, not singer names. For every recording, copy an
entry from `manifest.example.csv` into a local manifest under `test-output/`.
Record ownership/consent, allowed development use, redistribution permission,
rate, selected channel, range, phonation and known processing. Permission to
test locally does not imply permission to publish a singer's recording.

Record 5–15 seconds for each category: male_low, male_mid, female_mid,
female_high, breathy, vibrato, slides, consonants. Category labels describe
coverage goals; they do not infer a singer's gender from F0. Include ah/ee/oo,
both small sharp/flat errors, slow and fast slides, octave jumps, S/SH and P/T/K.
Keep independent pitch annotations alongside local audio when available.

Render with `vocalpilot_render input.wav NEW-output-directory`. This runs the
same M2 tracking source once and reuses its trajectory for every engine. Keep
the exported controls.csv for replay. See docs/MILESTONE3-LISTENING.md for
anonymous comparisons. Human listening scores have not yet been collected.
