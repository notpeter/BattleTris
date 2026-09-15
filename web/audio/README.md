# Sound placeholders

[manifest.json](manifest.json) is an asset inventory, not a runtime feature.
The browser currently stays silent. All 47 cue `assets` arrays are empty, and
the file maps activation, expiration and launch behavior for all 34 weapons.
No audio is downloaded or synthesized from these placeholders.

The inventory covers every active sound path in `BTSoundManager.C` and the
direct playback calls in `BTStartup.C`. Challenge and About cues are retained
as source references even though their native screens are not browser features.
They do not imply a multiplayer implementation.

## Adding recordings

1. Choose a stable cue ID from the manifest. Keep its ID when replacing assets.
2. Put recordings in `web/audio/assets/`. Supply recordings we can distribute;
   legacy filenames identify intended cues, not supplied or licensed assets.
3. Add entries to the cue's `assets` array, using this shape:

   ```json
   {
     "src": "assets/game-start.mp3",
     "type": "audio/mpeg",
     "credit": "Creator and license or permission"
   }
   ```

   Paths are relative to the manifest's `asset_url_base` (`./audio/` from the
   game page). Start with one supported encoding per recording; validate added
   formats in the supported browsers before release. Multiple entries represent
   alternate recordings in a cue pool, not fallback encodings of one recording.
4. Add the optional browser adapter and copy the manifest/assets into the build
   when the first recordings are ready. The inventory and this guide are bundled
   for reference; the current game does not load either at runtime.
5. Check playback, mute, missing files and failed decoding in every browser.
   Keep empty arrays silent and do not block gameplay on audio failures.

Native files were Sun `.au` files using 8-bit mu-law audio. None are present in
this repository. The ten random pools use directory discovery, so their exact
filenames cannot be recovered from the code. `native.paths` records glob
patterns for those pools and exact, case-sensitive paths for individual cues.
These historical paths are documentation only; do not fetch them in a browser.

## Playback contract for a future adapter

- Enable audio only after an explicit user gesture, such as Play or an audio
  toggle. Resume/unlock the audio context in that gesture handler. Do not replay
  a backlog of welcome or game events after permission becomes available.
- Provide a visible mute control and remember its setting. Mute must stop current
  playback, as well as suppress future cues. Handle a suspended context without
  affecting game timers or controls.
- Trigger sounds from gameplay events, not canvas rendering or repeated state
  polling. Emit each event once. Audio selection must use separate randomness
  from game pieces, weapons and reconnaissance, preserving deterministic replays.
- Bound simultaneous voices; cancel waiting-bazaar audio when leaving the bazaar,
  and cancel pending playback on restart, game over or page suspension.
- Treat a random pool with zero entries as silent and one entry as playable.
  Do not copy native `play_random()` literally: it excludes index zero and can
  loop forever for a one-file pool. Prefer a shuffled pool without immediate
  repeats when there are multiple recordings.
- Native launch selection gives Gimp its own cue, otherwise uses cheap cues at
  price <= 100, expensive cues at price > 400, and silence in between. Native
  activation and expiration are separate events. Reusing a launch cue for both
  would introduce extra sounds.
- Null weapon cue references deliberately preserve native silence. In particular,
  the spy activation `Sonar.au` call is commented out. Ordinary movement, rotation,
  dropping and locking have no native sound calls; adding these is a separate
  creative decision, not restoration of missing files.
- Optional native `.r_rated` paths are documented for completeness. Do not enable
  alternate content implicitly; a future implementation must make that choice
  explicit. The inventory does not require any particular dialogue or recording.

## Inventory checks

From the repository root:

```sh
python3 web/audio/check.py
```

The native trigger descriptions refer to
`usr/src/game/BTSoundManager.C`, `usr/src/game/BTStartup.C`,
`usr/src/game/BTGame.C` (bazaar timer), and `usr/src/game/BTProtocol.H`
(weapon token ordering). Weapon names follow `usr/src/share/btweapons.db`.
