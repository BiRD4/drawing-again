# {Name}
{Name} is a modal image drawing program.\
![(Drawing Program)-18\_39\_35](https://github.com/BiRD4/drawing-again/assets/20910668/16861026-b64e-46bb-8264-e55282e108a6)

## Guide

### Invocation
`{Name} [files...]`

### Window elements

| Window element             | Description                            |
| -------------------------- | -------------------------------------- |
| Scope and mode indicators  | The currently selected scope and mode  |
| Color swatches             | The currently selected colors          |
| Alpha blending indicator   | Whether to blend when drawing          |
| Color channel rulers       | Displayed when picking a color         |
| Canvases                   | The images being edited                |

Each edge of a canvas's border is a ruler that counts the row or column in binary,
which makes identifying powers of two easy.

### Concepts
When a scope is selected, its associated modes become available for selection.\
When a scope's mode is selected, its associated controls become available for use.

| Scope  | Key | Indicator | Description                                                                      |
| ------ | :-: | :-------: | -------------------------------------------------------------------------------- |
| Easel  |  V  |     ▚     | Comprises controls at the level of the imaginary "easel" that holds the canvases |
| Canvas |  C  |     ▬     | Comprises controls at the level of the canvases                                  |
| Pick   |  X  |     ▥     | Comprises controls for picking the color swatches                                |

| Easel mode | Key | Indicator | Description            |
| ---------- | :-: | :-------: | ---------------------- |
| Edit       |  R  |     ▬     | Add or delete canvases |
| Transform  |  E  |     ╋     | Transform canvases     |
| Select     |  W  |     ▭     | Select canvases        |

| Canvas mode | Key | Indicator | Description            |
| ----------- | :-: | :-------: | ---------------------- |
| Pixel       |  R  |     □     | Draw pixels            |
| Line        |  E  |     ─     | Line tool              |
| Rect        |  W  |     ■     | Rectangle tool         |
| Fill        |  Q  |     ▬     | Flood fill             |

| Pick mode |    Key    | Description    |
| --------- | :-------: | ------------   |
| F         | (Space-)R | (Swap) F color |
| D         | (Space-)E | (Swap) D color |
| S         | (Space-)W | (Swap) S color |
| A         | (Space-)Q | (Swap) A color |

### Controls

#### Global

| Global control            |      Key       |
| ------------------------- | :------------: |
| Open                      |     Ctrl-E     |
| Save (as)                 | Ctrl-(Shift-)S |
| Pan (+ zoom)              |   (Space-)G    |
| Alpha blend (off)         |   (Space-)T    |
| Toggle debug              |       F2       |
| Toggle hide mouse on zoom |       F3       |

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

To build without tinyfiledialogs: `make WITH_TINYFD=0`\
Otherwise: `make`

For tinyfiledialogs info, see [README-tinyfd](https://github.com/BiRD4/drawing-again/blob/main/README-tinyfd#L138 "README-tinyfd").

## Possible additions
- [ ] Image formats other than PNG
- [ ] Undo/redo
- [ ] Select/move/copy/paste tool (probably contigent on undo/redo)
- [ ] Brush editing
- [ ] Mouse button support
- [ ] Channel locking

## Monies
Can give monies if ya want :3\
[Patreon (Stripe)](https://patreon.com/user?u=106662965&utm_medium=clipboard_copy&utm_source=copyLink&utm_campaign=creatorshare_creator&utm_content=join_link "Patreon")\
<a href="https://patreon.com/user?u=106662965&utm_medium=clipboard_copy&utm_source=copyLink&utm_campaign=creatorshare_creator&utm_content=join_link"><img alt="Patreon" src="https://c10.patreonusercontent.com/4/patreon-media/p/campaign/11324090/5caed472704c4ac88a24a2a27d60c105/eyJ3IjoyMDB9/3.jpg?token-time=2145916800&token-hash=0T-lD0CODUi0piwk1j5DneZtxCbCPZOERKIn_yI8U7E%3D"></a>\
[Liberapay (PayPal)](https://liberapay.com/BiRD4/donate "Liberapay")\
<a href="https://liberapay.com/BiRD4/donate"><img alt="Liberapay" src="https://liberapay.com/assets/widgets/donate.svg"></a>\
[PayPal](https://paypal.me/BiRD4444 "PayPal")\
<a href="https://paypal.me/BiRD4444"><img alt="PayPal" src="https://www.paypalobjects.com/en_US/i/btn/btn_donate_LG.gif"></a>
