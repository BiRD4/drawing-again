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
	{D_NONE, {0, 0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0}},
	NULL, NULL
};

SDL_Window *win;
SDL_Renderer *ren;

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
	for (int i = 0; i < state.canvasArr->size; ++i)
		canvasDel(state.canvasArr->array[i]);
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
			0, w, h, 32, SDL_PIXELFORMAT_ARGB32
			);
	return c;
}

int canvasAdd(struct canvasArray *ca, struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto canvasAdd_cleanup;

	struct canvasArray newArr = {ca->size + 1, NULL};
	newArr.array = realloc(
			ca->array, newArr.size * sizeof (struct canvas *)
			);
	if (!newArr.array)
		goto canvasAdd_cleanup;
	newArr.array[newArr.size - 1] = c;
	ca->size = newArr.size;
	ca->array = newArr.array;

	flag = 1;
canvasAdd_cleanup:
	return flag;
}

int canvasRem(struct canvasArray *ca, struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto canvasRem_cleanup;

	for (int i = 0; i < ca->size; ++i) {
		if (ca->array[i] == c) {
			struct canvasArray newArr = {ca->size - 1, NULL};
			newArr.array = calloc(
					newArr.size, sizeof (struct canvas *)
					);
			if (!newArr.array)
				goto canvasRem_cleanup;
			for (int j = 0; j < i; ++j)
				newArr.array[j] = ca->array[j];
			for (int j = i; j < newArr.size; ++j)
				newArr.array[j] = ca->array[j + 1];
			free(ca->array);
			ca->size = newArr.size;
			ca->array = newArr.array;
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

struct canvas *canvasGet(struct canvasArray ca, int x, int y)
{
	for (int i = 0; i < ca.size; ++i) {
		struct canvas *c = ca.array[i];
		if (TO_COORD_EASEL_X(x) >= c->x &&
		    TO_COORD_EASEL_Y(y) >= c->y &&
		    TO_COORD_EASEL_X(x) <  c->x + c->w &&
		    TO_COORD_EASEL_Y(y) <  c->y + c->h
		   )
			return c;
	}

}

int canvasFix(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto canvasFix_cleanup;

	if (c->w != c->surf->w || c->h != c->surf->h) {
		SDL_Surface *newSurf = SDL_CreateRGBSurfaceWithFormat(
				0, c->w, c->h, 32, SDL_PIXELFORMAT_ARGB32
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
			// TODO
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
				}
				break;
			}
		case D_CANVASNEW:
			{
				// TODO
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

int setScope(enum Scope scope)
{
	int flag = 0;

	if (state.drag.action != D_PANZOOM) {
		if (!setDrag(D_NONE))
			goto setScope_cleanup;
	}
	state.scope = scope;

	flag = 1;
setScope_cleanup:
	return flag;
}

int setModeEasel(enum modeEasel mode)
{
	int flag = 0;

	// TODO

	flag = 1;
setModeEasel_cleanup:
	return flag;
}

int setModeCanvas(enum modeCanvas mode)
{
	int flag = 0;

	// TODO

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

	for (int i = 0; i < state.canvasArr->size; ++i) {
		struct canvas *c = state.canvasArr->array[i];
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
		int rw, rh;
		SDL_GetRendererOutputSize(ren, &rw, &rh);
		SDL_SetRenderDrawColor(ren, 255, 0, 0, SDL_ALPHA_OPAQUE);
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
			break;
		case SDLK_v:
			setScope(S_EASEL);
			break;
		case SDLK_c:
			setScope(S_CANVAS);
			break;
		case SDLK_g:
			setDrag(D_PANZOOM);
			break;
		case SDLK_SPACE:
			setSpace(1);
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
			break;
		case SDLK_SPACE:
			setSpace(0);
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
			if (!state.space) {
				int mx, my;
				SDL_GetMouseState(&mx, &my);
				state.easel.x = mx + (
						state.easel.s
						* state.drag.panZoom.offX
						/ state.drag.panZoom.initScale
						);
				state.easel.y = my + (
						state.easel.s
						* state.drag.panZoom.offY
						/ state.drag.panZoom.initScale
						);
			} else {
				// you don't use offY in here
				// TODO
			}
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
