#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#define WIN_WIDTH 512
#define WIN_HEIGHT 512
#define FPS 60

#define TO_COORD_EASEL_X(c) ((c - state.easel.x) / state.easel.s \
                           - (c - state.easel.x < 0))
#define TO_COORD_EASEL_Y(c) ((c - state.easel.y) / state.easel.s \
                           - (c - state.easel.y < 0))
#define TO_COORD_SCREEN_X(c) (c * state.easel.s + state.easel.x)
#define TO_COORD_SCREEN_Y(c) (c * state.easel.s + state.easel.y)

#define MAP_CANVASES(ca, i, c)           \
        struct canvas *c = ca->array[0]; \
        for (                            \
                int i = 0;               \
                i < ca->size;            \
                ++i, c = ca->array[i]    \
                )

struct canvas {
	int isSel;
	int x;
	int y;
	int w;
	int h;
	SDL_Surface *surf;
};

struct canvasArray {
	int size;
	struct canvas **array;
};

struct {

	int quit;

	int debug;

	struct {
		int x;
		int y;
		int s;
	} easel;

	// TODO add S_PICK and C_SELECT
	enum Scope {S_EASEL, S_CANVAS} scope;
	enum modeEasel {E_EDIT, E_TRANSFORM, E_SELECT} modeEasel;
	enum modeCanvas {C_PIXEL, C_LINE, C_FILL} modeCanvas;

	int space;

	struct {
		enum ActionDrag {
			D_NONE,
			D_PANZOOM,
			D_CANVASNEW,
			D_CANVASTRANSFORM,
			D_DRAWPIXEL,
			D_DRAWLINE
		} action;
		struct {
			int offX;
			int offY;
			int initScale;

			int initX;
			int initY;
			int accumStep;
		} panZoom;
		struct {
			int moveX;
			int moveY;
			int resizeX;
			int resizeY;
		} canvasTransform;
		struct {
			int x1;
			int y1;
		} drawLine;
	} drag;

	struct canvasArray *canvasArr;
	struct canvasArray *canvasSel;

} state = {
	0,
	0,
	{0, 0, 16},
	S_EASEL, E_EDIT, C_PIXEL,
	0,
	{D_NONE, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0}},
	NULL, NULL
};

SDL_Window *win;
SDL_Renderer *ren;

int ceiling(float f)
{
	int i = (int) f;
	if (f - i == 0)
		return i;
	else
		return i + 1;
}

int init() {
	int flag = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		goto init_cleanup;

	win = SDL_CreateWindow(
			"Drawing Program",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			WIN_WIDTH, WIN_HEIGHT,
			SDL_WINDOW_RESIZABLE
			);
	if (win == NULL)
		goto init_cleanup;

	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	if (ren == NULL)
		goto init_cleanup;

	state.canvasArr = malloc(sizeof (struct canvasArray));
	state.canvasSel = malloc(sizeof (struct canvasArray));
	state.canvasArr->size = 0;
	state.canvasSel->size = 0;
	state.canvasArr->array = NULL;
	state.canvasSel->array = NULL;

	flag = 1;
init_cleanup:
	return flag;
}

void quit() {
	MAP_CANVASES(state.canvasArr, i, c) {
		canvasDel(c);
	}
	free(state.canvasArr);
	free(state.canvasSel);

	SDL_DestroyRenderer(ren);
	SDL_Quit();
}

struct canvas *canvasNew(int x, int y, int w, int h)
{
	struct canvas *c = malloc(sizeof (struct canvas));
	if (!c)
		return NULL;
	c->isSel = 0;
	c->x = x;
	c->y = y;
	c->w = w;
	c->h = h;
	c->surf = SDL_CreateRGBSurfaceWithFormat(
			0, w, h, 32,
			SDL_PIXELFORMAT_ARGB32
			);
	return c;
}

int canvasAdd(struct canvasArray *ca, struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto canvasAdd_cleanup;

	struct canvasArray newArray = {ca->size + 1, NULL};
	newArray.array = realloc(
			ca->array,
			newArray.size * sizeof (struct canvas *)
			);
	if (!newArray.array)
		goto canvasAdd_cleanup;
	newArray.array[newArray.size - 1] = c;
	ca->size = newArray.size;
	ca->array = newArray.array;

	flag = 1;
canvasAdd_cleanup:
	return flag;
}

int canvasRem(struct canvasArray *ca, struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto canvasRem_cleanup;

	MAP_CANVASES(ca, i, ci) {
		if (ci == c) {
			struct canvasArray newArray = {ca->size - 1, NULL};
			newArray.array = calloc(
					newArray.size,
					sizeof (struct canvas *)
					);
			if (!newArray.array)
				goto canvasRem_cleanup;
			for (int j = 0; j < i; ++j)
				newArray.array[j] = ca->array[j];
			for (int j = i; j < newArray.size; ++j)
				newArray.array[j] = ca->array[j + 1];
			free(ca->array);
			ca->size = newArray.size;
			ca->array = newArray.array;
		}
	}

	flag = 1;
canvasRem_cleanup:
	return flag;
}

int canvasDel(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto canvasDel_cleanup;

	canvasRem(state.canvasArr, c);
	canvasRem(state.canvasSel, c);
	SDL_FreeSurface(c->surf);
	free(c);

	flag = 1;
canvasDel_cleanup:
	return flag;
}

struct canvas *canvasGet(struct canvasArray *ca, int x, int y)
{
	for (int i = ca->size - 1; i >= 0; --i) {
		struct canvas *c = ca->array[i];
		if (x >= c->x
		 && y >= c->y
		 && x <  c->x + c->w
		 && y <  c->y + c->h)
			return c;
	}
	return NULL;
}

int canvasMove(struct canvas *c, int x, int y, int w, int h)
{
	int flag = 0;
	if (!c)
		goto canvasMove_cleanup;

	c->x = x;
	c->y = y;
	c->w = (w > 0) ? w : 1;
	c->h = (h > 0) ? h : 1;

	flag = 1;
canvasMove_cleanup:
	return flag;
}

int canvasFix(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto canvasFix_cleanup;

	if (c->w != c->surf->w || c->h != c->surf->h) {
		SDL_Surface *newSurf = SDL_CreateRGBSurfaceWithFormat(
				0, c->w, c->h, 32,
				SDL_PIXELFORMAT_ARGB32
				);
		if (!newSurf)
			goto canvasFix_cleanup;
		SDL_FreeSurface(c->surf);
		c->surf = newSurf;
	}

	flag = 1;
canvasFix_cleanup:
	return flag;
}

struct canvasArray *canvasArrayCopy(struct canvasArray *ca)
{
	if (!ca)
		return NULL;

	struct canvasArray *newArray = malloc(sizeof (struct canvasArray));
	if (!newArray)
		return NULL;
	newArray->size = ca->size;
	newArray->array = calloc(newArray->size, sizeof (struct canvas *));
	if (!newArray->array)
		return NULL;
	MAP_CANVASES(ca, i, c) {
		newArray->array[i] = c;
	}

	return newArray;
}

void canvasArrayFree(struct canvasArray *ca)
{
	free(ca->array);
	free(ca);
}

int setDrag(enum ActionDrag action)
{
	int flag = 0;

	switch (state.drag.action) {
		case D_NONE:
			break;
		case D_PANZOOM:
			SDL_CaptureMouse(SDL_FALSE);
			SDL_SetRelativeMouseMode(SDL_FALSE);
			if (state.space)
				SDL_WarpMouseInWindow(
						win,
						state.drag.panZoom.initX,
						state.drag.panZoom.initY
						);
			break;
		case D_CANVASNEW:
			{
				struct canvasArray *oldArray =
					canvasArrayCopy(state.canvasSel);
				MAP_CANVASES(oldArray, i, cFix) {
					canvasFix(cFix);
					canvasRem(state.canvasSel, cFix);
				}
				canvasArrayFree(oldArray);
			}
			break;
		case D_CANVASTRANSFORM:
			state.drag.canvasTransform.moveX = 0;
			state.drag.canvasTransform.moveY = 0;
			state.drag.canvasTransform.resizeX = 0;
			state.drag.canvasTransform.resizeY = 0;
			break;
		case D_DRAWPIXEL:
			break;
		case D_DRAWLINE:
			// TODO
			break;
		default:
			break;
	}

	switch (action) {
		case D_NONE:
			break;
		case D_PANZOOM:
			{
				int mx, my;
				SDL_GetMouseState(&mx, &my);
				state.drag.panZoom.offX = state.easel.x - mx;
				state.drag.panZoom.offY = state.easel.y - my;
				state.drag.panZoom.initScale = state.easel.s;

				SDL_CaptureMouse(SDL_TRUE);
				if (state.space) {
					SDL_SetRelativeMouseMode(SDL_TRUE);
					state.drag.panZoom.initX = mx;
					state.drag.panZoom.initY = my;
					state.drag.panZoom.accumStep = 0;
				}
				break;
			}
		case D_CANVASNEW:
			{
				int mx, my;
				struct canvas *c;
				SDL_GetMouseState(&mx, &my);
				c = canvasNew(
						TO_COORD_EASEL_X(mx),
						TO_COORD_EASEL_Y(my),
						1, 1
					     );
				if (c) {
					canvasAdd(state.canvasArr, c);
					canvasAdd(state.canvasSel, c);
				}
				break;
			}
		case D_CANVASTRANSFORM:
			{
				// TODO
				break;
			}
		case D_DRAWPIXEL:
			{
				// TODO
				break;
			}
		case D_DRAWLINE:
			{
				// TODO
				break;
			}
		default:
			break;
	}

	state.drag.action = action;

setDrag_cleanupNoError:
	flag = 1;
setDrag_cleanup:
	return flag;
}

int resetDrag()
{
	int flag = 0;

	if (state.drag.action != D_PANZOOM) {
		if (!setDrag(D_NONE))
			goto resetDrag_cleanup;
	}

	flag = 1;
resetDrag_cleanup:
	return flag;
}

int setScope(enum Scope scope)
{
	int flag = 0;

	if (!resetDrag())
		goto setScope_cleanup;
	state.scope = scope;

	flag = 1;
setScope_cleanup:
	return flag;
}

int setModeEasel(enum modeEasel mode)
{
	int flag = 0;

	if (!resetDrag())
		goto setModeEasel_cleanup;
	state.modeEasel = mode;

	flag = 1;
setModeEasel_cleanup:
	return flag;
}

int setModeCanvas(enum modeCanvas mode)
{
	int flag = 0;

	if (!resetDrag())
		goto setModeCanvas_cleanup;
	state.modeCanvas = mode;

	flag = 1;
setModeCanvas_cleanup:
	return flag;
}

int setSpace(int space)
{
	int flag = 0;

	if (space) {
		switch (state.drag.action) {
			case D_NONE:
				break;
			case D_PANZOOM:
				{
					int mx, my;
					SDL_GetMouseState(&mx, &my);
					state.drag.panZoom.initX = mx;
					state.drag.panZoom.initY = my;
					SDL_SetRelativeMouseMode(SDL_TRUE);
					break;
				}
			case D_CANVASNEW:
				break;
			case D_CANVASTRANSFORM:
				break;
			case D_DRAWPIXEL:
				break;
			case D_DRAWLINE:
				break;
			default:
				break;
		}
	} else {
		switch (state.drag.action) {
			case D_NONE:
				break;
			case D_PANZOOM:
				SDL_SetRelativeMouseMode(SDL_FALSE);
				SDL_WarpMouseInWindow(
						win,
						state.drag.panZoom.initX,
						state.drag.panZoom.initY
						);
				break;
			case D_CANVASNEW:
				break;
			case D_CANVASTRANSFORM:
				break;
			case D_DRAWPIXEL:
				break;
			case D_DRAWLINE:
				break;
			default:
				break;
		}
	}

	state.space = space;

setSpace_cleanupNoError:
	flag = 1;
setSpace_cleanup:
	return flag;
}

int frameDo() {
	int flag = 0;

	SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(ren);

	MAP_CANVASES(state.canvasArr, i, c) {
		if (c->isSel)
			SDL_SetRenderDrawColor(
					ren, 255, 255, 255,
					SDL_ALPHA_OPAQUE
					);
		else
			SDL_SetRenderDrawColor(
					ren, 127, 127, 127,
					SDL_ALPHA_OPAQUE
					);
		SDL_Rect border = {
			TO_COORD_SCREEN_X(c->x) - 1,
			TO_COORD_SCREEN_Y(c->y) - 1,
			state.easel.s * c->w + 2,
			state.easel.s * c->h + 2
		};
		SDL_RenderDrawRect(ren, &border);
	}

	int cursorX;
	int cursorY;
	if (state.drag.action == D_PANZOOM && state.space) {
		cursorX = state.drag.panZoom.initX;
		cursorY = state.drag.panZoom.initY;
	} else {
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		cursorX = mx;
		cursorY = my;
	}
	SDL_Rect cursor = {
		TO_COORD_SCREEN_X(TO_COORD_EASEL_X(cursorX)),
		TO_COORD_SCREEN_Y(TO_COORD_EASEL_Y(cursorY)),
		state.easel.s,
		state.easel.s
	};
	SDL_SetRenderDrawColor(ren, 127, 127, 127, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(ren, &cursor);

	if (state.debug) {
		SDL_SetRenderDrawColor(ren, 255, 0, 0, SDL_ALPHA_OPAQUE);

		int rw, rh;
		SDL_GetRendererOutputSize(ren, &rw, &rh);
		SDL_RenderDrawLine(ren, 0, state.easel.y, rw, state.easel.y);
		SDL_RenderDrawLine(ren, state.easel.x, 0, state.easel.x, rh);
	}

	SDL_RenderPresent(ren);

	flag = 1;
frameDo_cleanup:
	return flag;
}

int eventKeyDown(SDL_Event *e)
{
	int flag = 0;

	if (e->key.repeat)
		goto eventKeyDown_cleanupNoError;

	switch (e->key.keysym.sym) {
		case SDLK_F12:
			state.debug = !state.debug;
			goto eventKeyDown_cleanupNoError;
		case SDLK_v:
			setScope(S_EASEL);
			goto eventKeyDown_cleanupNoError;
		case SDLK_c:
			setScope(S_CANVAS);
			goto eventKeyDown_cleanupNoError;
		case SDLK_g:
			setDrag(D_PANZOOM);
			goto eventKeyDown_cleanupNoError;
		case SDLK_SPACE:
			setSpace(1);
			goto eventKeyDown_cleanupNoError;
		default:
			break;
	}
	switch (state.scope) {
		case S_EASEL:
			switch (e->key.keysym.sym) {
				case SDLK_r:
					setModeEasel(E_EDIT);
					goto eventKeyDown_cleanupNoError;
				case SDLK_e:
					setModeEasel(E_TRANSFORM);
					goto eventKeyDown_cleanupNoError;
				case SDLK_w:
					setModeEasel(E_SELECT);
					goto eventKeyDown_cleanupNoError;
				default:
					break;
			}
			switch (state.modeEasel) {
				case E_EDIT:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							setDrag(D_CANVASNEW);
							break;
						case SDLK_d:
							if (state.canvasSel->size == 0) {
								int mx, my;
								SDL_GetMouseState(&mx, &my);
								struct canvas *c = canvasGet(
									state.canvasArr,
									TO_COORD_EASEL_X(mx),
									TO_COORD_EASEL_Y(my)
									);
								canvasDel(c);
							} else {
								struct canvasArray *arrayDel =
									canvasArrayCopy(
										state.canvasSel
										);
								MAP_CANVASES(arrayDel, i, c) {
									canvasDel(c);
								}
								canvasArrayFree(arrayDel);
							}
							break;
						default:
							break;
					}
					break;
				case E_TRANSFORM:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							break;
						case SDLK_d:
							break;
						case SDLK_s:
							break;
						case SDLK_a:
							break;
						default:
							break;
					}
					break;
				case E_SELECT:
					switch (e->key.keysym.sym) {
						int mx, my;
						struct canvas *c;
						case SDLK_f:
							SDL_GetMouseState(&mx, &my);
							c = canvasGet(
								state.canvasArr,
								TO_COORD_EASEL_X(mx),
								TO_COORD_EASEL_Y(my)
								);
							if (c && !c->isSel) {
								canvasAdd(state.canvasSel, c);
								c->isSel = 1;
							}
							break;
						case SDLK_d:
							SDL_GetMouseState(&mx, &my);
							c = canvasGet(
								state.canvasArr,
								TO_COORD_EASEL_X(mx),
								TO_COORD_EASEL_Y(my)
								);
							if (c && c->isSel) {
								canvasRem(state.canvasSel, c);
								c->isSel = 0;
							}
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			break;
		case S_CANVAS:
			switch (e->key.keysym.sym) {
				case SDLK_r:
					setModeCanvas(C_PIXEL);
					goto eventKeyDown_cleanupNoError;
				case SDLK_e:
					setModeCanvas(C_LINE);
					goto eventKeyDown_cleanupNoError;
				case SDLK_w:
					setModeCanvas(C_FILL);
					goto eventKeyDown_cleanupNoError;
				default:
					break;
			}
			switch (state.modeCanvas) {
				case C_PIXEL:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							break;
						case SDLK_d:
							break;
						case SDLK_s:
							break;
						case SDLK_a:
							break;
						default:
							break;
					}
					break;
				case C_LINE:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							break;
						case SDLK_d:
							break;
						case SDLK_s:
							break;
						case SDLK_a:
							break;
						default:
							break;
					}
					break;
				case C_FILL:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							break;
						case SDLK_d:
							break;
						case SDLK_s:
							break;
						case SDLK_a:
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

eventKeyDown_cleanupNoError:
	flag = 1;
eventKeyDown_cleanup:
	return flag;
}

int eventKeyUp(SDL_Event *e)
{
	int flag = 0;

	switch (e->key.keysym.sym) {
		case SDLK_g:
			if (state.drag.action == D_PANZOOM)
				setDrag(D_NONE);
			goto eventKeyUp_cleanup;
		case SDLK_SPACE:
			setSpace(0);
			goto eventKeyUp_cleanup;
		default:
			break;
	}
	switch (state.scope) {
		case S_EASEL:
			switch (state.modeEasel) {
				case E_EDIT:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							setDrag(D_NONE);
							break;
						default:
							break;
					}
					break;
				case E_TRANSFORM:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							break;
						case SDLK_d:
							break;
						case SDLK_s:
							break;
						case SDLK_a:
							break;
						default:
							break;
					}
					break;
				case E_SELECT:
					break;
				default:
					break;
			}
			break;
		case S_CANVAS:
			switch (state.modeCanvas) {
				case C_PIXEL:
					switch (e->key.keysym.sym) {
						case SDLK_f:
						case SDLK_d:
						case SDLK_s:
						case SDLK_a:
							break;
						default:
							break;
					}
					break;
				case C_LINE:
					switch (e->key.keysym.sym) {
						case SDLK_f:
						case SDLK_d:
						case SDLK_s:
						case SDLK_a:
							break;
						default:
							break;
					}
					break;
				case C_FILL:
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	flag = 1;
eventKeyUp_cleanup:
	return flag;
}

int eventMouseMotion(SDL_Event *e)
{
	int flag = 0;

	switch (state.drag.action) {
		case D_NONE:
			break;
		case D_PANZOOM:
			{
				int focusX;
				int focusY;
				if (state.space) {
					if (!(state.easel.s == 64 && e->motion.yrel < 0)
					&& (!(state.easel.s ==  1 && e->motion.yrel > 0)))
						state.drag.panZoom.accumStep -= e->motion.yrel;
					else
						state.drag.panZoom.accumStep = 0;
					int accum = state.drag.panZoom.accumStep;
					if (accum >= 0) {
						int quota = ceiling(48.0 / state.easel.s);
						while (accum >= quota && state.easel.s < 64) {
							accum -= quota;
							++state.easel.s;
							quota = ceiling(48.0 / state.easel.s);
						}
					} else if (state.easel.s > 1) {
						int quota = ceiling(48.0 /(state.easel.s - 1));
						while (-accum >= quota && state.easel.s > 1) {
							accum += quota;
							--state.easel.s;
							quota = ceiling(48.0 /(state.easel.s - 1));
						}
					}
					state.drag.panZoom.accumStep = accum;
					state.drag.panZoom.initX += e->motion.xrel;
					focusX = state.drag.panZoom.initX;
					focusY = state.drag.panZoom.initY;
				} else {
					int mx, my;
					SDL_GetMouseState(&mx, &my);
					focusX = mx;
					focusY = my;
				}
				state.easel.x = focusX + (
						state.easel.s
						* state.drag.panZoom.offX
						/ state.drag.panZoom.initScale
						);
				state.easel.y = focusY + (
						state.easel.s
						* state.drag.panZoom.offY
						/ state.drag.panZoom.initScale
						);
				break;
			}
		case D_CANVASNEW:
			{
				int mx, my;
				SDL_GetMouseState(&mx, &my);
				MAP_CANVASES(state.canvasSel, i, c) {
					canvasMove(c, c->x, c->y,
						TO_COORD_EASEL_X(mx) - c->x,
						TO_COORD_EASEL_Y(my) - c->y
						);
				}
				break;
			}
		case D_CANVASTRANSFORM:
			break;
		case D_DRAWPIXEL:
			break;
		case D_DRAWLINE:
			break;
		default:
			break;
	}

	flag = 1;
eventMouseMotion_cleanup:
	return flag;
}

int eventDo(SDL_Event *e)
{
	int flag = 0;

	while (SDL_PollEvent(e)) {
		switch (e->type) {
			case SDL_QUIT:
				state.quit = 1;
				break;
			case SDL_KEYDOWN:
				if (!eventKeyDown(e))
					goto eventDo_cleanup;
				break;
			case SDL_KEYUP:
				if (!eventKeyUp(e))
					goto eventDo_cleanup;
				break;
			case SDL_MOUSEMOTION:
				if (!eventMouseMotion(e))
					goto eventDo_cleanup;
				break;
			case SDL_MOUSEBUTTONDOWN:
				break;
			case SDL_MOUSEBUTTONUP:
				break;
			case SDL_MOUSEWHEEL:
				break;
			case SDL_DROPFILE:
				break;
			default:
				break;
		}
	}

	flag = 1;
eventDo_cleanup:
	return flag;
}

//void canvasSelFix()
//{
//	if (!state.canvasSel.active) {
//		free(state.canvasSel.array);
//		state.canvasSel.size = 0;
//	}
//}
//
//void dragFix()
//{
//	switch (state.drag.action) {
//		case D_NEWCANVAS:
//		case D_TRANSFORMCANVAS:
//			canvasFix(state.canvasSel.array);
//			canvasSelFix();
//			break;
//		default:
//			break;
//	}
//	state.drag.action = D_NONE;
//}
//
//void scopeSet(enum Scope scope)
//{
//	dragFix();
//	state.scope = scope;
//}
//
//void eventKeyDown(SDL_Keycode key)
//{
//	switch (key) {
//		case SDLK_v:
//			scopeSet(EASEL);
//			goto eventKeyDown_done;
//		case SDLK_c:
//			scopeSet(CANVAS);
//			goto eventKeyDown_done;
//		default:
//			break;
//	}
//	switch (state.scope) {
//		case EASEL:
//			switch (key) {
//				case SDLK_r:
//					modeEaselSet(E_EDIT);
//					goto eventKeyDown_done;
//				case SDLK_e:
//					modeEaselSet(E_TRANSFORM);
//					goto eventKeyDown_done;
//				case SDLK_w:
//					modeEaselSet(E_SELECT);
//					goto eventKeyDown_done;
//				default:
//					break;
//			}
//			switch (state.modeEasel) {
//				case E_EDIT:
//					switch (key) {
//						case SDLK_f:
//							{
//								int mx, my;
//								SDL_GetMouseState(&mx, &my);
//								dragActionSet(D_NEWCANVAS);
//								canvasSel(canvasNew(mx, my, 1, 1), 0);
//								break;
//							}
//						case SDLK_d:
//							{
//								int mx, my;
//								SDL_GetMouseState(&mx, &my);
//								if (!state.canvasSel.active)
//									canvasDel(canvasGet(state.canvasArray, mx, my));
//								else
//									canvasDel(state.canvasSel);
//								break;
//							}
//						default:
//							break;
//					}
//					break;
//				case E_TRANSFORM:
//					switch (key) {
//						case SDLK_f:
//							state.drag.transformCanvas.moveX = 1;
//							goto eventKeyDown_eTransform;
//						case SDLK_d:
//							state.drag.transformCanvas.moveY = 1;
//							goto eventKeyDown_eTransform;
//						case SDLK_s:
//							state.drag.transformCanvas.resizeX = 1;
//							goto eventKeyDown_eTransform;
//						case SDLK_a:
//							state.drag.transformCanvas.resizeY = 1;
//							goto eventKeyDown_eTransform;
//						default:
//							break;
//					}
//					break;
//eventKeyDown_eTransform:
//					if (state.drag.action != D_TRANSFORMCANVAS) {
//						dragActionSet(D_TRANSFORMCANVAS);
//						if (state.canvasSel.size == 0) {
//							int mx, my;
//							SDL_GetMouseState(&mx, &my);
//							canvasSel(canvasGet(state.canvasArray, mx, my));
//						}
//					}
//					break;
//				case E_SELECT:
//					switch (key) {
//						case SDLK_f:
//							break;
//						case SDLK_d:
//							break;
//						default:
//							break;
//					}
//					break;
//				default:
//					break;
//			}
//			break;
//		case CANVAS:
//			switch (key) {
//				case SDLK_r:
//					state.modeCanvas = C_PIXEL;
//					break;
//				case SDLK_e:
//					state.modeCanvas = C_LINE;
//					break;
//				case SDLK_w:
//					state.modeCanvas = C_FILL;
//					break;
//				default:
//					break;
//			}
//			switch (state.modeCanvas) {
//				case C_PIXEL:
//					switch (key) {
//						default:
//							break;
//					}
//					break;
//				case C_LINE:
//					switch (key) {
//						default:
//							break;
//					}
//					break;
//				case C_FILL:
//					switch (key) {
//						default:
//							break;
//					}
//					break;
//				default:
//					break;
//			}
//			break;
//		default:
//			break;
//	}
//eventKeyDown_done:
//}
//
//int eventDo(SDL_Event *e) {
//	int flag = 0;
//
//	while (SDL_PollEvent(e)) {
//		switch (e->type) {
//			case SDL_QUIT:
//				state.quit = 1;
//				break;
//			case SDL_KEYDOWN:
//				if (!e->key.repeat)
//					eventKeyDown(e->key.keysym.sym);
//				break;
//			case SDL_KEYUP:
//				break;
//			case SDL_MOUSEMOTION:
//				break;
//			case SDL_MOUSEBUTTONDOWN:
//				break;
//			case SDL_MOUSEBUTTONUP:
//				break;
//			case SDL_MOUSEWHEEL:
//				break;
//			case SDL_DROPFILE:
//				break;
//			default:
//				break;
//		}
//	}
//
//	flag = 1;
//eventDo_cleanup:
//	return flag;
//}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, NULL);

	int flag = 1;

	if (!init())
		goto main_cleanup;

	Uint32 tickCurr;
	Uint32 tickNext = 0;
	int delta = 1000 / FPS;
	while (!state.quit) {
		tickCurr = SDL_GetTicks();
		if (SDL_TICKS_PASSED(tickCurr, tickNext)) {
			tickNext = tickCurr + delta;
			frameDo();
		}
		SDL_Event e;
		eventDo(&e);
	}

	flag = 0;
main_cleanup:
	if (flag)
		puts(SDL_GetError());
	quit();
	return flag;
}
