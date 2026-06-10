# UIAnimationEditor

Standalone DirectX12 + ImGui tool for creating UI animation JSON files.

## 30 Second Flow

1. Launch `UIAnimationEditor.exe`.
2. Drop a PNG into the Preview, or press `Texture`.
3. Keep `Auto Key` on.
4. At frame 0, drag the image in Preview.
5. Move the timeline to frame 60.
6. Drag the image again.
7. Press `Play`.
8. Press `Export JSON`.

## Layout

```text
+------------------+--------------------------------------+------------------+
| Animations       | Preview                              | Selected Object  |
| - TitleOpen      |                                      | Auto Key         |
| New/Open/Save    | 1280x720 or 1920x1080 canvas         | Position         |
| Export settings  | Drag object to create keys           | Scale            |
|                  |                                      | Rotation         |
+------------------+--------------------------------------+------------------+
| Timeline: Position / Scale / Rotation / Color / Alpha key rows           |
+-------------------------------------------------------------------------+
```

## UX Policy

- The first action is visual: drop PNG into Preview.
- Main editing is drag based: move object in Preview and drag keys in Timeline.
- Numeric fields are secondary correction controls.
- `Auto Key` is enabled by default to match Unity/After Effects expectations.
- `Save` keeps project data, while `Export JSON` writes runtime data for the game.

## Class Design

- `UIEditorAnimationClip`: owns animation name, length, canvas size, texture path, base object state, and tracks.
- `UIEditorTrack`: owns keyframes for one exported property.
- `UIEditorKeyFrame`: frame/value/interpolation.
- `UIAnimationEditorApp`: ImGui layout, preview interaction, timeline, file commands.
- `UIEditorD3D12`: standalone Win32/DX12 rendering layer.
- `UIEditorTextureLoader`: PNG decode through WIC.

## JSON

Runtime export keeps the game-readable shape:

```json
{
  "Name": "TitleOpen",
  "Length": 60,
  "Texture": "D:/Textures/title.png",
  "CanvasWidth": 1280,
  "CanvasHeight": 720,
  "Tracks": [
    {
      "Property": "PositionX",
      "Keys": [
        { "Frame": 0, "Value": 100 },
        { "Frame": 60, "Value": 500 }
      ]
    }
  ]
}
```

## Future Expansion

- Multiple objects per animation can be added by introducing object IDs on tracks.
- Curve editor can reuse existing interpolation enum.
- Presets can be layered on top of `UIEditorAnimationClip` without changing runtime export.
