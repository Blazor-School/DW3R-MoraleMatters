Guard feedback assets for Morale Matters 1.9.0

shield-*.png: Original vector shield glyphs rendered by tools/generate_guard_shields.py.
The gold-to-empty row and break emphasis are inspired by the user's Origins reference.

GuardBreak.wav uses break_3.wav (0.90 seconds) for a full guard break.
GuardHit.wav uses flesh_4.wav (0.28 seconds) for a blocked hit.
Both clips are included as supplied. Rapid or simultaneous hits are rate-limited,
and a full-break cue can interrupt a hit cue.
Playback is an asynchronous 2D UI cue at Windows audio volume. If unavailable, the mod
falls back to the game's native spatial full-break impact; normal native block impacts
remain untouched. BreakSound=0 disables both custom cues.
