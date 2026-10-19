# Game GIF recording

- F7/F8 decrease/increase the recording duration: 5, 10, 15 or 30 seconds (default 5).
- F9 starts recording; F9 again stops early. Recording stops automatically at the selected duration.
- Duration is locked during recording/saving. Settings displays the current duration and keyboard controls, including when a gamepad is active.
- Debug/development single-frame stepping uses Shift+F7 to avoid conflicting with recording duration.
- Output: 640x360, approximately 15 captures per second, 256-color looping GIF, no audio.
- Files: `Captures/Gameplay_<Unix milliseconds>.gif` relative to the working directory.
- Game HUD is included. ImGui and capture notifications are excluded.
- Recording works in Debug and Release without ImGui. It follows real time, including paused game screens.
- Saving runs on a worker thread; another recording can start after saving completes.
- Slow frames or a full queue skip capture opportunities; timestamps preserve playback duration.
- At most eight downsampled RGBA frames wait in the queue (about 7 MiB); one pending image and encoding scratch are additional.
- Existing renderer GPU fence is reused; no extra per-capture GPU wait is inserted. CPU readback/downsampling still adds work.
- Exiting normally stops and drains the recording. Failed/empty recordings are reported and incomplete output is removed.

`GifWriter` is shared with ModelPreview via its `PreviewGif` compatibility alias.
`GifRecordingManager` owns capture scheduling, the reusable readback buffer, status text, and the encoding worker.
The renderer copies after game UI, before ImGui, then reads after `DirectXCommon::PostDraw` completes the GPU fence.

Validation: build Debug/Release and ModelPreview; run `Tests/PreviewGifTests.cpp` with
`Engine/Capture/GifWriter.cpp`, then independently decode `patterns.gif` and compare its RGB frames
with `expected.rgb`. Exercise auto-stop, manual stop, repeat recording, F12/Shift+F12,
scene changes and normal shutdown in the game.
