# {Name}
{Name} is a modal image drawing program.\
![(Drawing Program)-12\_28\_27](https://github.com/BiRD4/drawing-again/assets/20910668/c69a38c2-e4b9-4799-b945-84ac8d8c1dcc)

## Guide

### Invocation
`{Name} [files...]`

### Window elements

| Window element           | Description                    |
| ------------------------ | ------------------------------ |
| Color swatches           | The currently selected colors  |
| Color channel rulers     | Displayed when picking a color |
| Alpha blending indicator | Whether to blend when drawing  |
| Canvases                 | The images being edited        |

Each edge of a canvas's border is a ruler that counts the row or column in binary,
which makes identifying powers of two easy.

### Concepts
When a scope is selected, its associated modes become available for selection.\
When a scope's mode is selected, its associated controls become available for use.

| Scope  | Key | Description                                                                      |
| ------ | :-: | -------------------------------------------------------------------------------- |
| Easel  |  V  | Comprises controls at the level of the imaginary "easel" that holds the canvases |
| Canvas |  C  | Comprises controls at the level of the canvases                                  |
| Pick   |  X  | Comprises controls for picking the color swatches                                |

| Easel mode | Key | Description            |
| ---------- | :-: | ---------------------- |
| Edit       |  R  | Add or delete canvases |
| Transform  |  E  | Transform canvases     |
| Select     |  W  | Select canvases        |

| Canvas mode | Key | Description            |
| ----------- | :-: | ---------------------- |
| Pixel       |  R  | Draw pixels            |
| Line        |  E  | Line tool              |
| Rect        |  W  | Rectangle tool         |
| Fill        |  Q  | Flood fill             |

| Pick mode |    Key    | Description    |
| --------- | :-------: | ------------   |
| F         | (Space-)R | (Swap) F color |
| D         | (Space-)E | (Swap) D color |
| S         | (Space-)W | (Swap) S color |
| A         | (Space-)Q | (Swap) A color |

### Controls

#### Global

| Global control    |      Key       |
| ----------------- | :------------: |
| Open              |     Ctrl-E     |
| Save (as)         | Ctrl-(Shift-)S |
| Pan (+ zoom)      |   (Space-)G    |
| Alpha blend (off) |   (Space-)T    |
| Toggle debug      |      F12       |

Additionally, multifile drag and drop is supported.

#### Easel scope

| Edit mode control | Key |
| ----------------- | :-: |
| Add canvas        |  F  |
| Delete canvas     |  D  |

| Transform mode control | Key |
| ---------------------- | :-: |
| Translate X            |  F  |
| Translate Y            |  D  |
| Resize X               |  S  |
| Resize Y               |  A  |

| Select mode control   | Key |
| --------------------- | :-: |
| Select canvas         |  F  |
| Deselect canvas       |  D  |
| Select all canvases   |  S  |
| Deselect all canvases |  A  |

#### Canvas scope

| Pixel/line/rect/fill mode control |     Key     |
| --------------------------------- | :---------: |
| (Eyedrop) F color                 |  (Space-)F  |
| (Eyedrop) D color                 |  (Space-)D  |
| (Eyedrop) S color                 |  (Space-)S  |
| (Eyedrop) A color                 |  (Space-)A  |

#### Pick scope

| F/D/S/A mode control   | Key |
| ---------------------- | :-: |
| Red channel            |  F  |
| Green channel          |  D  |
| Blue channel           |  S  |
| Alpha channel          |  A  |

## Building
This program uses [SDL2](https://wiki.libsdl.org/SDL2/FrontPage "SDL2 Wiki") and [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/ "tinyfiledialogs Website").

To build without tinyfiledialogs: `make NO_TINYFD=1`\
Otherwise: `make`

If open/save doesn't work, make sure that you have what you need as described in README-tinyfd.

## Possible additions
- [ ] Image formats other than PNG
- [ ] Undo/redo
- [ ] Select/move/copy/paste tool (probably contigent on undo/redo)
- [ ] Brush editing
- [ ] Mouse button support
