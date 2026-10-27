# Spirit box prototype

The first spirit box uses the existing offline phrase recognizer and authoritative store ghost. It plays an imported audio file; no LLM, streaming service or live speech synthesis runs during gameplay.

## PIE use

Open `/Game/Environment/Mall/Maps/Lvl_MallOval_Test`. The spirit box is beside the book on the low evidence stand in the east middle ground-floor test store, at `(4234, -190, 70.3)` cm. The checkout counter's existing blocker obstructed interaction traces, so the new radio uses the reachable stand instead.

1. Aim at the box and press **F** to pick it up. Place the book first if carrying it.
2. Press **R** to switch the radio on. Its scan noise and indicator show its power state.
3. Hold **V**, ask **Where are you?**, **Who are you?** or **How old are you?**, then release V.
4. The reply and quiet scan/static come from the **spirit box**. Carrying or moving it also moves the reply. The static pauses during an answer and resumes afterwards. A cooldown of at least four seconds prevents overlapping answers; longer replacement clips extend it.
5. Aim at a clear floor or tabletop and press **F** to place it. A powered box also works when placed. Aim at a nearby placed box and press **R** to change power.

Voice recognition now uses local Whisper English. Wait for its ready status, release V after the whole question, and allow a few seconds for CPU processing before repeating. See [Voice recognition](VoiceRecognition.md) for microphone controls, model details and tests.

The speaker and device must be in the ghost's store. Character confinement uses the physical collision capsule, ignoring camera and presentation volumes; equipment still requires its full bounds to fit. The speaker must be within 6 m of the box and the ghost within 15 m. The ghost answers in its invisible, harmless state. Turning the box off stops playback. Questions can independently request writing in an eligible open, placed book; saying **write in the book** remains a writing request, rather than a radio question.

The response is chosen once on the server and sent to clients as a sound asset, subtitle and starting radio position. Each listener hears the same reply from the radio in their local world. Playback stays attached to the locally replicated box, including while carried; the starting position remains historical metadata. Late joiners receive device state and last-answer metadata without replaying an old answer. Actual microphone input and audible two-player mixing remain hardware tests.

## Replacing the voice

The test device uses **BP_MallSpiritBox**, located at `/Game/Environment/Mall/Blueprints/BP_MallSpiritBox`. Its native parent provides pickup, power, server validation and spatial playback. The three questions have independent entries in the editable **Responses** array:

| Spoken question | PhraseId (keep unchanged) | Prototype Subtitle | Current Sound |
| --- | --- | --- | --- |
| Where are you? | `where_are_you` | I am here. | `SW_SpiritBox_Placeholder` |
| Who are you? | `who_are_you` | I am Evelyn. | `SW_SpiritBox_Placeholder` |
| How old are you? | `how_old_are_you` | I was thirty-two. | `SW_SpiritBox_Placeholder` |

Evelyn and thirty-two are editable prototype facts, not generated ghost identity or age. The sound intentionally remains the same temporary “I am here” recording for every entry, so identity/age text will not match the spoken placeholder until their recordings are replaced.

### Import a new recording

1. Prepare a clean **mono WAV** file, preferably 48 kHz / 16-bit PCM for straightforward import. Keep dry higher-quality masters separately if desired. Record a single answer, with a short lead-in/tail and no baked stereo panning, room reverb or looping.
2. In Unreal's **Content Browser**, open `Content/Audio/Ghost/SpiritBox`.
3. Click **Import**, choose the WAV, and import it as a **Sound Wave**. Suggested names are `SW_SpiritBox_Where`, `SW_SpiritBox_Who`, and `SW_SpiritBox_Age`.
4. Open the imported Sound Wave, check that it is mono, leave **Looping** off, and preview the recording. Save it.

### Link it in the Blueprint

1. Open **BP_MallSpiritBox** from `Content/Environment/Mall/Blueprints`.
2. Click **Class Defaults** and search the Details panel for **Responses** (category **Mall → Ghost → Spirit Box**).
3. Expand the entry whose **PhraseId** matches the question. Change **Sound** using its asset picker, or select the Sound Wave in the Content Browser and click the property's **Use Selected Asset** arrow.
4. Update **Subtitle** to match the actual spoken answer. Change only this entry to replace one question independently. No Event Graph or speech-recognition changes are needed.
5. **Compile** and **Save** the Blueprint. Test in PIE with the radio powered on in the ghost's store.

Blueprint Class Defaults apply to instances that inherit the array. A placed actor's custom **Responses** override takes priority. To use the Blueprint defaults again, select the placed radio, find **Responses**, and use its **Reset to Default** arrow. To customize only one device instead, edit the placed actor's array and save the level.

To add alternate answers, add more array entries with the same **PhraseId** and different Sound/Subtitle values. The server cycles through valid matching entries. Use non-looping clips with a finite duration; a missing or looping sound will not be selected. Sound Cues can also be assigned, provided they produce suitable non-looping positional audio.

Keep identity and age facts consistent when recording variants for the same ghost. These entries are currently device/Blueprint data; per-ghost biographies can be added later if needed.

For friends' recordings, retain clean mono WAV masters, ideally 48 kHz / 24-bit, with one line per file, a short lead-in and tail, and several takes. Import the WAVs into `/Game/Audio/Ghost/SpiritBox`. Keep voices dry so radio processing can be improved separately later. The current separate scan loop is quiet and does not alter the recording. Keep a record of each performer's credit and permission to use their recordings.

The primitive radio is a functional test prop. Its casing, grille and antenna can later be replaced with a finished model. The carried visual uses a simple waist pose; a dedicated hand animation and hotbar integration are later work.

## Replacing the static / scan sound

The static is a separate **Scan Sound** property, not a fourth entry in **Responses**. Its current asset is `/Game/Audio/Ghost/SpiritBox/SW_SpiritBox_Static`. This pass replaces the original harsh four-second placeholder with a softer original twelve-second loop: filtered hiss, irregular tuning swells and small crackles, without broadcast voices, music or a constant tone. The old `SW_SpiritBox_Scan` asset is retained for reference.

1. Prepare a **mono WAV**, preferably **48 kHz / 16-bit PCM**. For downloaded MP3 or stereo recordings, edit/convert a copy to this format first; changing a filename extension does not convert audio. Keep the original and its license information separately.
2. Choose a clean static section without intelligible broadcasts or music. Trim sudden starts/stops and crossfade the end into the beginning so repeated playback has no click or obvious silence. Around 8–20 seconds is a useful starting length. Keep enough headroom for crackles; avoid clipping and unusually loud peaks.
3. Import it into `Content/Audio/Ghost/SpiritBox` as a **Sound Wave**, for example `SW_SpiritBox_Static_Custom`. Open it, enable **Looping**, preview it through several repeats, and save. Voice answer WAVs must still have Looping **off**.
4. Open **BP_MallSpiritBox → Class Defaults**, search for **Scan Sound** (`ScanSound`), and assign the new Sound Wave with the asset picker or **Use Selected Asset** arrow. **Compile** and **Save**. A placed actor's custom Scan Sound override takes priority; reset it to inherit the Blueprint value.
5. To adjust the static level, select **ScanAudio** in the Blueprint's Components panel and edit **Volume Multiplier**, currently **0.04**. The **ResponseAudio** component controls spoken-answer volume separately, currently **0.65**. Adjust the component rather than boosting the WAV into clipping. Both sounds use distance attenuation.
6. In PIE, power on a placed box, walk around it, pick it up, and ask a question. Check that the bed loops without clicks, stops while the reply plays, resumes afterwards, and stops when powered off. Turning the device off also stops a reply.

For later edits to an already imported WAV, use its **Reimport** command in the Content Browser and verify **Looping** afterwards, or import under a new name and change **Scan Sound** as above. Keep source WAVs in `SourceAssets`, outside `Content`; import the resulting Sound Wave assets into `Content`. An automated **Import with Replace Existing** attempt on the original, already loaded native-default sound hit Unreal's `!IsRooted()` editor assertion in this pass. The replacement was therefore imported as a new asset; avoid that overwrite workflow.

### Free static recordings to audition

These are source options checked on October 17, 2026; they are not bundled into the project or acoustically auditioned in this pass. The installed replacement is original synthesized audio.

| Recording | Useful starting material | License / download notes |
| --- | --- | --- |
| [Radio, frequency sweep #10 — BigSoundBank](https://bigsoundbank.com/radio-frequency-sweep-10-s2626.html) | 9-second mono radio-tuner sweep; 48 kHz / 24-bit | CC0; commercial games and edits allowed; attribution optional; no account required. |
| [Radio Noise.wav — RutgerMuller / Freesound](https://freesound.org/people/RutgerMuller/sounds/50750/) | 57-second recording of tuning through static; stereo 44.1 kHz / 24-bit | CC0; commercial use and edits allowed; attribution optional; login required to download. Convert the selected section to mono. |
| [Radio Static — DRAGON-STUDIO / Pixabay](https://pixabay.com/sound-effects/film-special-effects-radio-static-323621/) | 8-second static effect, supplied as MP3 | Pixabay Content License; free use/adaptation and optional credit, with restrictions including standalone redistribution/resale. Convert a copy to WAV. |

License references: [BigSoundBank permissions](https://bigsoundbank.com/licenses.html), [CC0 terms](https://creativecommons.org/publicdomain/zero/1.0/), and [Pixabay Content License summary](https://pixabay.com/service/license-summary/). Save the asset URL, author, license and download date with any recording chosen. Audition for unwanted broadcast voices/music before selecting a loop.

## Direction and later environmental acoustics

Spoken replies use positional audio attached to the radio, with full level inside 1 m and a linear falloff extending to 25 m. Static also stays attached to the radio, with its own quieter level and shorter range. Use stereo headphones and turn the camera while listening to test left/right localization and distance. The current method is Unreal's standard spatial panning; no new HRTF plugin is required by this prototype.

Ghost-origin playback uses the independent native `MallGhostAudio` helper, including the existing signal chime and the new physical ghost sounds below. It provides the pawn-view/mouth source position and 1–25 m spatial attenuation, attaching playback to the ghost so movement carries the sound. These sounds do not require an equipped or powered radio; spirit-box replies still come from the box.

Environmental reverb, reflections and wall/door occlusion are deferred until the mall layout is stable. A later pass can define acoustic zones for stores, corridors and the large central space, tune reverb sends and portal transitions, and evaluate baked or real-time propagation if needed. Record dry dialogue now so those effects can be applied by the game later. No map-finalization acoustics are claimed by this pass.

Official references: [Epic's audio import guide](https://dev.epicgames.com/documentation/unreal-engine/importing-audio-files) and [sound attenuation, spatialization and reverb settings](https://dev.epicgames.com/documentation/unreal-engine/sound-attenuation-in-unreal-engine).

## Physical ghost sounds

The test ghost now has a **MallGhostSounds** component (`MallGhostSoundComponent`) for its own occasional breaths, moans and chase growls. The server chooses one short non-looping clip and sends a live event to listeners. Playback is attached to each listener's local copy of the ghost at eye/mouth height, so both movement and distance affect the source. Last-cue metadata replicates for inspection, but joining a session does not replay old sounds.

While invisible, the ghost randomly chooses a breath or moan only when a living player is wholly inside its store and within 25 m. Its first automatic check is six seconds after spawn, with silent gaps of **8–16 seconds after a clip finishes**. It stays invisible and harmless. Existing disturbance pauses and recovery cooldowns silence these cues too.

Chase growls require visible hunt mode, the actual authored **Chasing** state, and a living current target within the store and range. A merely visible, idle or returning ghost does not use chase sounds. Entering a valid chase requests a cue within roughly half a second; later clips have **3–6 seconds of silence** between them. Changing state, losing eligibility, pausing the chase or ending play stops its physical cue. Only one physical cue plays at a time per ghost. Other systems, including the radio, keep their own playback.

| Use | Imported asset in `/Game/Audio/Ghost/Physical` | Source |
| --- | --- | --- |
| Invisible breath, about 3.42 s | `SW_Ghost_InvisibleBreath` | [Ghost breath — qubodup](https://opengameart.org/content/ghost-breath), CC0 |
| Invisible moan, about 3.02 s | `SW_Ghost_InvisibleMoan` | [Ghost Monster Voice Moaning & Growling — qubodup](https://opengameart.org/content/ghost-monster-voice-moaning-growling), `GhostMoan01`, CC0 |
| Chase growl, about 1.73 s | `SW_Ghost_HuntGrowl` | Same CC0 pack, `GhostMoan05` |

The original files, license notes, source URLs, hashes and reproducible preparation script are in `SourceAssets/GhostSounds`. Preparation converts to mono 48 kHz PCM16, applies edge fades and level adjustments, and adds no new reverb. The original ghost vocals already contain artistic processing. They were selected from author descriptions and waveform/level checks; subjective quality and loudness need a listening test.

To replace clips, import new short mono WAV Sound Waves with **Looping off**. In **BP_StoreGhost**, select **MallGhostSounds** in the Components panel, then edit **Invisible Sounds** or **Hunt Sounds**. Add array entries for more random variants. The component's **Invisible Volume**, **Hunt Volume**, and minimum/maximum gap fields tune levels and timing separately. Compile and save; a placed component override takes priority over Blueprint defaults. Invalid, missing, looping or longer-than-15-second clips are skipped. The runtime subsystem also supplies this component to tagged store ghosts that have no authored one.

For a listening check, enter the ground-floor east test store in PIE and wait for an invisible cue and the automatic hunt cycle below. Listen with headphones while moving and turning: the source should remain at the ghost, including when it passes to either side. Environmental occlusion/reverb remains deferred until the map is finalized.

The saved Blueprint passes all five `Mall.Ghost` tests, including real-IP physical-sound delivery, state/store/cooldown guards, source movement, duplicate rejection and late joining. The live mall check observed automatic invisible cues and a growl playing during actual AI pursuit: its sound position matched the moving ghost's current eye position, rather than the earlier starting snapshot. Restoring invisible mode stopped the chase audio. Evidence is in `Automation/GhostSounds/acceptance.json` and `live_pie.json`; subjective audio acceptance remains a headphones test.

## Automatic activity and hunt cycle

The test ghost cycles automatically on the server. Press the usual green **Play** button and enter its ground-floor east store; no console commands are needed.

| Phase | Default timing | Behaviour |
| --- | --- | --- |
| Invisible activity | 30–45 seconds after an eligible player enters | The ghost stays harmless, with its existing sounds, loose-object disturbances, book and radio interactions. An empty store does not consume the first-entry grace period. |
| Warning | 3 seconds | The ghost becomes visible but cannot chase or catch the player yet. Leaving the store cancels this warning. |
| Hunting | Up to 20 seconds | Existing store-confined pursuit and positional chase sounds operate. A catch or everyone leaving the store ends the hunt early. |
| Cooldown | 40–60 seconds after the hunt ends | The ghost returns to invisible, harmless activity. It cannot begin another hunt until this separate timer expires, and another warning precedes hunting. |

Only a living, possessed player whose physical capsule fits wholly inside the ghost's store can arm a hunt. Neighboring stores, upstairs players and cameras protruding into the store do not qualify. Existing path validation, store boundaries and catch behaviour remain in the chase Blueprint. The brief catch/voice `CooldownUntil` pause is separate from the inter-hunt rest period, so harmless activity can resume without waiting for the entire hunt cooldown.

In **BP_StoreGhost**, select **MallGhostInteraction** in the Components panel. Under **Mall → Ghost → Hunt Cycle**, **Automatic Hunts**, the initial-activity minimum/maximum, warning duration, hunt duration and cooldown minimum/maximum settings control the cycle. Compile and save changes; placed component overrides take priority. **Automatic Disturbances** controls loose props independently from **Automatic Hunts**. Turning automatic hunts off stops an automatic warning/hunt and allows manual development tests.

For debugging, the component exposes the current **Cycle Phase**, **Phase Ends Server Time**, **Hunt Count** and a remaining-time getter. Phase state and visibility replicate to clients, including late joiners. The old host-only `mall.GhostMode hunt` and `mall.GhostMode invisible` commands remain optional development controls; normal play does not require them.

All seven cycle/ghost/voice-transport tests pass, including warning catch prevention, a full catch cooldown, book writing during rest, authority and late joining. The live mall check used the saved default timings with no mode commands or timing overrides: it observed a warning at about 46.6 s, hunting at 49.7 s, a catch ending that hunt at 57.2 s, and a second automatic warning/hunt at 104.9/108.2 s. During cooldown, harmless disturbances and invisible sounds continued. PIE is stopped with clean saved packages. Current evidence is in `Automation/GhostHuntCycle/acceptance.json` and `live_timeline.json`. The numeric recorder cannot sample enum values, so its phase labels are inferred from visibility and hunt counts; direct property reads separately confirm the hunt/cooldown states.

## Store lighting during hunts

The test store's **MallGhostLights** component follows the server's ghost state. The harmless warning dips to **10%** brightness; actual **Chasing** includes brief **0%** outages. Invisible activity, cooldown, a cancelled warning, an idle/returning hunt, and loss of an eligible target restore the lights' original intensity. The runtime effect changes intensity only. Stopping PIE or removing the ghost also restores the captured intensity.

In **BP_StoreGhost**, select **MallGhostLights**. **Enable Flicker** enables the effect, **Warning Minimum Brightness** and **Chase Minimum Brightness** set the dimmest fraction of normal brightness, and **Flicker Rate** controls the variation. Defaults are **0.10**, **0.00**, and **2.3 brightness steps per second**, or about **1.15 complete bright/dim cycles per second**. Smooth transitions alternate between variable bright peaks and short held dim intervals, ensuring the configured minimum is actually reached. Compile and save after tuning. The server replicates the state, timing, pattern seed and selected light references, including for players joining during a chase; it does not send every brightness sample over the network.

**Target Light Tag** must match an actor tag on each participating placed point, spot or rect light. The test store uses **GhostFlicker_East_F0_02** on its ceiling light and four interior fixture lights. Use a different tag for each future haunted store. An unset tag selects nothing; unrelated lights, player flashlights and the ghost's voice-signal light are excluded. Static lights are skipped because their baked lighting cannot flicker at runtime. Use movable store lights when authoring new rooms.

Only the selected light actors flicker. Shadows are enabled on the **14 point lights whose attenuation ranges reach the test store**, including neighboring and upstairs sources, so the existing shadow-casting walls and floors can block their light. The other sources retain their normal intensity. Ambient skylight, light entering doorways and final map lighting still affect the remaining room brightness; 0% means the five selected lights are off, not that the entire scene has no illumination. Exposure remains fixed at EV100 11 with the existing +3 compensation. The effect changes real lights, not emissive fixture materials.

To review it, press **Play**, enter the ground-floor east test store, and wait for the usual initial activity period. Watch for 10% dips during the three-second warning, complete lamp outages once the ghost starts chasing, and normal lighting after a catch or leaving the store starts cooldown.

The full native build succeeds, and all eight ghost/voice-transport regression tests pass against the rebuilt DLL with zero test warnings/errors, including exact held 10% warning and 0% chase samples across three seeds. The light test also covers server-selected references/settings, differently bright lamps, two peers and a late joiner, actual-chase eligibility, unaffected neighboring/upstairs/signal/flashlight intensities, exact restoration, disabling, unregistering and ghost destruction. Current results are recorded in `Automation/GhostLights/DarknessPass/native_tests.json`; the initial Live Coding checks and earlier fixture correction remain as historical evidence.

The stronger shadowed setup was verified in two-player listen-server PIE with normal cycle timings and no mode commands. Both peers reached the exact 10% warning floor and all five lamps reached exactly zero during chase intervals, then restored their original intensities when a catch started cooldown. The neighboring and upstairs comparison intensities remained constant. The placed ghost's light-group and brightness overrides are saved as well as the Blueprint defaults. Evidence for this pass is in `Automation/GhostLights/DarknessPass/acceptance.json`, `live_summary.json` and the two peer timelines. The earlier mild-flicker results remain in the parent folder as historical evidence.

After restarting Unreal with the full build, both the Blueprint template and placed ghost retain the correct group and 0.10/0.00 settings. All 14 shadow flags and original normal lamp intensities also persist. The mall is open with PIE stopped and no dirty map/content packages; the restart readback is in `Automation/GhostLights/DarknessPass/restart_verification.json`.

## Source audio

`SourceAssets/SpiritBox/SW_SpiritBox_Placeholder.wav` is an offline synthesized prototype voice created with the installed Microsoft Zira Desktop voice. `SW_SpiritBox_Scan.wav` is an original deterministic twelve-second filtered noise texture, with no sampled radio broadcasts or third-party dialogue. The reproducible preparation scripts are in `Automation/SpiritBox`; `SourceAssets/SpiritBox/provenance.json` records format and file hashes. Imported SoundWaves live at `/Game/Audio/Ghost/SpiritBox` and are referenced by the native actor defaults and editable Blueprint.

## Interaction references

[Kinetic Games' voice recognition update](https://kineticgames.co.uk/news/phasmophobia-voice-recognition-update) describes spoken Spirit Box questions, cursed-object questions, and using a ghost's name to provoke activity. [Demonologist's developer description](https://store.steampowered.com/app/1929610/?l=english) describes spoken words triggering environmental events. [Ghost Exile's developer description](https://store.steampowered.com/app/1807080/Ghost_Exile/?l=english) combines investigation equipment and voice interaction with a no-microphone command option. These suggest reusable interaction families: ask, provoke, offer an object, observe physical evidence, and record a response. This pass implements the radio question-and-response tool only.

## Validation

The radio-origin native build and ghost/book suite verify separate location/identity/age bindings, subtitles and selected sound delivery, radio source positions on both clients, independence from ghost movement, following a placed/carried radio, response variants, ownership/store/range/hunt guards and late join without replay. The suite retains the regression for oversized presentation volumes versus physical character capsules. The separate spatial-audio test checks retained ghost playback without a radio, including ghost movement/turns, attachment, attenuation and invalid-input rejection.

Null-audio automation establishes source transforms and settings, not perceived direction or loudness. Listen in PIE with headphones for the final acoustic check. Current results are recorded in `Automation/SpiritBox/radio_audio_acceptance.json`; the earlier ghost-origin experiment remains in `directional_acceptance.json` as historical evidence, and the original pickup/power/placement test record remains in `validation_summary.json`.
