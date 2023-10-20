#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#define FPS 60
#define MAX_ZOOM 64
#define INIT_SCALE 16
#define INIT_WIN_WIDTH 512
#define INIT_WIN_HEIGHT 512
#define INIT_CANVAS_WIDTH 16
#define INIT_CANVAS_HEIGHT 16

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

#define LOCK_SURFACE_IF_MUST(s)       \
        if (SDL_MUSTLOCK(s))          \
                SDL_LockSurface(s);

#define UNLOCK_SURFACE_IF_MUST(s)     \
        if (SDL_MUSTLOCK(s))          \
                SDL_UnlockSurface(s);

struct canvas {
	int isSel;
	int x;
	int y;
	int w;
	int h;
	SDL_Texture *tex;
	SDL_Surface *surf;
	SDL_Renderer *ren;
};

struct pixel {
	int x;
	int y;
};

struct canvasArray {
	int size;
	struct canvas **array;
};

struct pixelArray {
	int memlen;
	int size;
	struct pixel *array;
};

struct {

	int quit;
	int warp;
	int debug;
	int space;

	struct {
		int x;
		int y;
		int s;

		int minX;
		int minY;
		int maxX;
		int maxY;
	} easel;

	// TODO add S_PICK and C_SELECT
	enum Scope {S_EASEL, S_CANVAS} scope;
	enum ModeEasel {E_EDIT, E_TRANSFORM, E_SELECT} modeEasel;
	enum ModeCanvas {C_PIXEL, C_LINE, C_FILL} modeCanvas;

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
			int setX;
			int setY;
			int setW;
			int setH;

			int *offX;
			int *offY;
			int *offW;
			int *offH;
		} canvasTransform;
		struct {
			int initX;
			int initY;
			enum Key {KEY_F, KEY_D, KEY_S, KEY_A} key;
			SDL_Color color;
			struct pixelArray *pixels;
		} drawPixel;
		struct {
			int initX;
			int initY;
			enum Key key;
			SDL_Color color;
			struct pixelArray *pixels;
			struct canvas *preview;
			struct canvasArray *previewArr;
			struct pixelArray *previewPixels;
		} drawLine;
	} drag;

	struct {
		SDL_Color f;
		SDL_Color d;
		SDL_Color s;
		SDL_Color a;
	} colors;

	struct canvasArray *canvasArr;
	struct canvasArray *canvasSel;

} state = {
	0, 0, 0, 0,
	{0, 0, INIT_SCALE, 0, 0, 0, 0},
	S_EASEL, E_EDIT, C_PIXEL,
	{
		D_NONE,
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, NULL, NULL, NULL, NULL},
		{0, 0, KEY_F, {0, 0, 0, 0}, NULL},
		{0, 0, KEY_F, {0, 0, 0, 0}, NULL, NULL, NULL, NULL}
	},
	{
		{255,   0,   0, 255},
		{  0, 255,   0, 255},
		{  0,   0, 255, 255},
		{255, 255, 255, 255}
	},
	NULL, NULL
};

SDL_Window *win;
SDL_Renderer *ren;

struct canvas *canvasNew(int x, int y, int w, int h);
struct canvasArray *canvasArrayNew();
struct pixelArray *pixelArrayNew();

int ceiling(float f)
{
	int i = (int) f;
	if (f - i == 0)
		return i;
	else
		return i + 1;
}

int round(double f)
{
	int i = (int) f;
	if (f - i < 0.5)
		return i;
	else
		return i + 1;
}

int allocDouble(void **array, int *memlen, int blockSize)
{
	int flag = 0;

	if (*memlen == 0) {
		void *newArray = malloc(blockSize);
		if (!newArray)
			goto cleanup;
		*memlen = blockSize;
		*array = newArray;
	} else {
		void *newArray = realloc(*array, 2 * *memlen);
		if (!newArray)
			goto cleanup;
		*memlen *= 2;
		*array = newArray;
	}

	flag = 1;
cleanup:
	return flag;
}

int init()
{
	int flag = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		goto cleanup;

	win = SDL_CreateWindow(
			"Drawing Program",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			INIT_WIN_WIDTH, INIT_WIN_HEIGHT,
			SDL_WINDOW_RESIZABLE
			);
	if (win == NULL)
		goto cleanup;

	ren = SDL_CreateRenderer(
			win, -1,
			SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE
			);
	if (ren == NULL)
		goto cleanup;

	state.drag.drawPixel.pixels = pixelArrayNew();

	state.drag.drawLine.pixels = pixelArrayNew();
	state.drag.drawLine.preview = canvasNew(0, 0, 1, 1);
	state.drag.drawLine.previewArr = canvasArrayNew();
	canvasArrayAppend(
			state.drag.drawLine.previewArr,
			state.drag.drawLine.preview
			);
	state.drag.drawLine.previewPixels = pixelArrayNew();

	state.canvasArr = canvasArrayNew();
	state.canvasSel = canvasArrayNew();

	flag = 1;
cleanup:
	return flag;
}

void quit()
{
	free(state.drag.canvasTransform.offX);
	free(state.drag.canvasTransform.offY);
	free(state.drag.canvasTransform.offW);
	free(state.drag.canvasTransform.offH);

	free(state.drag.drawPixel.pixels);

	free(state.drag.drawLine.pixels);
	canvasDel(state.drag.drawLine.preview);
	canvasArrayFree(state.drag.drawLine.previewArr);

	if (state.canvasArr->size != 0) {
		MAP_CANVASES(state.canvasArr, i, c) {
			canvasDel(c);
		}
	}
	canvasArrayFree(state.canvasArr);
	canvasArrayFree(state.canvasSel);

	SDL_DestroyRenderer(ren);
	SDL_Quit();
}

int texFix()
{
	int flag = 0;

	MAP_CANVASES(state.canvasArr, i, c) {
		LOCK_SURFACE_IF_MUST(c->surf);
		SDL_UpdateTexture(c->tex, NULL, c->surf->pixels, c->surf->pitch);
		UNLOCK_SURFACE_IF_MUST(c->surf);
	}

	flag = 1;
cleanup:
	return flag;
}

void easelBoundsFix()
{
	if (state.canvasArr->size != 0) {
		struct canvas *cInit = state.canvasArr->array[0];
		state.easel.minX = cInit->x;
		state.easel.minY = cInit->y;
		state.easel.maxX = cInit->x + cInit->w;
		state.easel.maxY = cInit->y + cInit->h;
		MAP_CANVASES(state.canvasArr, i, c) {
			if (c->x < state.easel.minX)
				state.easel.minX = c->x;
			if (c->y < state.easel.minY)
				state.easel.minY = c->y;
			if (c->x + c->w > state.easel.maxX)
				state.easel.maxX = c->x + c->w;
			if (c->y + c->h > state.easel.maxY)
				state.easel.maxY = c->y + c->h;
		}
	} else {
		state.easel.minX = 0;
		state.easel.minY = 0;
		state.easel.maxX = 0;
		state.easel.maxY = 0;
	}
}

void easelFix()
{
	int rw, rh;
	SDL_GetRendererOutputSize(ren, &rw, &rh);
	if (rw < TO_COORD_SCREEN_X(state.easel.minX))
		state.easel.x += rw - TO_COORD_SCREEN_X(state.easel.minX);
	if (rh < TO_COORD_SCREEN_Y(state.easel.minY))
		state.easel.y += rh - TO_COORD_SCREEN_Y(state.easel.minY);
	if ( 0 > TO_COORD_SCREEN_X(state.easel.maxX))
		state.easel.x +=  0 - TO_COORD_SCREEN_X(state.easel.maxX);
	if ( 0 > TO_COORD_SCREEN_Y(state.easel.maxY))
		state.easel.y +=  0 - TO_COORD_SCREEN_Y(state.easel.maxY);
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
	c->tex = SDL_CreateTexture(
			ren, SDL_PIXELFORMAT_ARGB32,
			SDL_TEXTUREACCESS_TARGET, w, h
			);
	if (!c->tex)
		goto cleanup;
	SDL_SetTextureBlendMode(c->tex, SDL_BLENDMODE_BLEND);
	c->surf = SDL_CreateRGBSurfaceWithFormat(
			0, w, h, 32,
			SDL_PIXELFORMAT_ARGB32
			);
	if (!c->surf)
		goto cleanup_surf;
	c->ren = SDL_CreateSoftwareRenderer(c->surf);
	if (!c->ren)
		goto cleanup_ren;
	return c;

cleanup_ren:
	SDL_FreeSurface(c->surf);
cleanup_surf:
	SDL_DestroyTexture(c->tex);
cleanup:
	free(c);
	return NULL;
}

int canvasDel(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	if (canvasArrayHas(state.canvasArr, c))
		canvasArrayRem(state.canvasArr, c);
	if (canvasArrayHas(state.canvasSel, c))
		canvasArrayRem(state.canvasSel, c);
	SDL_DestroyTexture(c->tex);
	SDL_DestroyRenderer(c->ren);
	SDL_FreeSurface(c->surf);
	free(c);

	flag = 1;
cleanup:
	return flag;
}

int canvasGetColor(struct canvas *c, int cx, int cy, SDL_Color *color)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	SDL_Surface *surf = c->surf;
	LOCK_SURFACE_IF_MUST(surf);
	Uint32 pix = ((Uint32 *)surf->pixels)[surf->w * cy + cx];
	SDL_PixelFormat *fmt = surf->format;
	Uint32 tmp;
	tmp = pix & fmt->Rmask;
	tmp = tmp >> fmt->Rshift;
	color->r = tmp << fmt->Rloss;
	tmp = pix & fmt->Gmask;
	tmp = tmp >> fmt->Gshift;
	color->g = tmp << fmt->Gloss;
	tmp = pix & fmt->Bmask;
	tmp = tmp >> fmt->Bshift;
	color->b = tmp << fmt->Bloss;
	tmp = pix & fmt->Amask;
	tmp = tmp >> fmt->Ashift;
	color->a = tmp << fmt->Aloss;
	UNLOCK_SURFACE_IF_MUST(surf);

	flag = 1;
cleanup:
	return flag;
}

int canvasClear(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	SDL_SetRenderTarget(ren, c->tex);
	SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
	SDL_RenderClear(ren);
	SDL_SetRenderTarget(ren, NULL);

	SDL_SetRenderDrawColor(c->ren, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
	SDL_RenderClear(c->ren);

	flag = 1;
cleanup:
	return flag;
}

int canvasMove(struct canvas *c, int x, int y, int w, int h)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	c->x = x;
	c->y = y;
	c->w = (w > 0) ? w : 1;
	c->h = (h > 0) ? h : 1;

	MAP_CANVASES(state.canvasArr, i, ci) {
		if (c == ci) {
			easelBoundsFix();
			break;
		}
	}

	flag = 1;
cleanup:
	return flag;
}

int canvasFix(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	int tw, th;
	SDL_QueryTexture(c->tex, NULL, NULL, &tw, &th);
	if (c->w != tw || c->h != th) {
		SDL_Texture *newTex = SDL_CreateTexture(
				ren, SDL_PIXELFORMAT_ARGB32,
				SDL_TEXTUREACCESS_TARGET, c->w, c->h
				);
		if (!newTex)
			goto cleanup;
		SDL_SetTextureBlendMode(newTex, SDL_BLENDMODE_BLEND);
		SDL_DestroyTexture(c->tex);
		c->tex = newTex;
	}
	if (c->w != c->surf->w || c->h != c->surf->h) {
		SDL_Surface *newSurf = SDL_CreateRGBSurfaceWithFormat(
				0, c->w, c->h, 32,
				SDL_PIXELFORMAT_ARGB32
				);
		if (!newSurf)
			goto cleanup;
		SDL_Renderer *newRen = SDL_CreateSoftwareRenderer(newSurf);
		if (!newRen)
			goto cleanup;
		SDL_BlitSurface(c->surf, NULL, newSurf, NULL);
		SDL_DestroyRenderer(c->ren);
		SDL_FreeSurface(c->surf);
		c->surf = newSurf;
		c->ren = newRen;
	}
	LOCK_SURFACE_IF_MUST(c->surf);
	SDL_UpdateTexture(c->tex, NULL, c->surf->pixels, c->surf->pitch);
	UNLOCK_SURFACE_IF_MUST(c->surf);

	flag = 1;
cleanup:
	return flag;
}

struct canvasArray *canvasArrayNew()
{
	struct canvasArray *ca = malloc(sizeof (struct canvasArray));
	if (!ca)
		return NULL;
	ca->size = 0;
	ca->array = NULL;
	return ca;
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
		goto cleanup;
	if (ca->size != 0) {
		MAP_CANVASES(ca, i, c) {
			newArray->array[i] = c;
		}
	}

	return newArray;

cleanup:
	free(newArray);
	return NULL;
}

int canvasArrayFree(struct canvasArray *ca)
{
	int flag = 0;
	if (!ca)
		goto cleanup;

	free(ca->array);
	free(ca);

	flag = 1;
cleanup:
	return flag;
}

int canvasArrayAppend(struct canvasArray *ca, struct canvas *c)
{
	int flag = 0;
	if (!ca || !c)
		goto cleanup;

	struct canvasArray newArray = {ca->size + 1, NULL};
	newArray.array = realloc(
			ca->array,
			newArray.size * sizeof (struct canvas *)
			);
	if (!newArray.array)
		goto cleanup;
	newArray.array[newArray.size - 1] = c;
	ca->size = newArray.size;
	ca->array = newArray.array;

	if (ca == state.canvasArr)
		easelBoundsFix();

	flag = 1;
cleanup:
	return flag;
}

int canvasArrayRem(struct canvasArray *ca, struct canvas *c)
{
	int flag = 0;
	if (!ca || !c || ca->size < 1)
		goto cleanup;

	MAP_CANVASES(ca, i, ci) {
		if (ci == c) {
			struct canvasArray newArray = {ca->size - 1, NULL};
			newArray.array = calloc(
					newArray.size,
					sizeof (struct canvas *)
					);
			if (!newArray.array)
				goto cleanup;
			for (int j = 0; j < i; ++j)
				newArray.array[j] = ca->array[j];
			for (int j = i; j < newArray.size; ++j)
				newArray.array[j] = ca->array[j + 1];
			free(ca->array);
			ca->size = newArray.size;
			ca->array = newArray.array;
		}
	}

	if (ca == state.canvasArr)
		easelBoundsFix();

	flag = 1;
cleanup:
	return flag;
}

int canvasArrayHas(struct canvasArray *ca, struct canvas *c)
{
	if (!ca || !c)
		return 0;
	for (int i = ca->size - 1; i >= 0; --i) {
		if (ca->array[i] == c)
			return 1;
	}
	return 0;
}

struct canvas *canvasArrayFind(struct canvasArray *ca, int x, int y)
{
	if (!ca)
		return NULL;
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

struct pixelArray *pixelArrayNew()
{
	struct pixelArray *pa = malloc(sizeof (struct pixelArray));
	if (!pa)
		return NULL;
	pa->memlen = 0;
	pa->size = 0;
	pa->array = NULL;
	return pa;
}

int pixelArrayFree(struct pixelArray *pa)
{
	int flag = 0;
	if (!pa)
		goto cleanup;

	free(pa->array);
	free(pa);

	flag = 1;
cleanup:
	return flag;
}

int pixelArrayAppend(struct pixelArray *pa, struct pixel pix)
{
	int flag = 0;
	if (!pa)
		goto cleanup;

	if (pa->size * sizeof (struct pixel) == pa->memlen)
		if (!allocDouble(
				&pa->array, &pa->memlen,
				sizeof (struct pixel)
				))
			goto cleanup;
	pa->array[pa->size++] = pix;

	flag = 1;
cleanup:
	return flag;
}

int pixelArrayLine(struct pixelArray *pa, int inX1, int inY1, int inX2, int inY2)
{
	int flag = 0;
	if (!pa)
		goto cleanup;

	if (inX1 == inX2 && inY1 == inY2) {
		struct pixel pix = {inX1, inY1};
		pixelArrayAppend(pa, pix);
		goto cleanupNoError;
	}

	int dx, dy;
	if (inX1 <= inX2)
		dx = inX2 - inX1;
	else
		dx = inX1 - inX2;
	if (inY1 <= inY2)
		dy = inY2 - inY1;
	else
		dy = inY1 - inY2;

	int i1, i2, di;
	int j1, j2, dj;
	int swap, flipX, flipY;
	if (dx >= dy) {
		swap = 0;
		if (inX1 <= inX2) {
			flipX = 0;
			i1 = inX1;
			i2 = inX2;
		} else {
			flipX = 1;
			i1 = inX2;
			i2 = inX1;
		}
		if (inY1 <= inY2) {
			flipY = 0;
			j1 = inY1;
			j2 = inY2;
		} else {
			flipY = 1;
			j1 = inY2;
			j2 = inY1;
		}
	} else {
		swap = 1;
		if (inX1 <= inX2) {
			flipX = 0;
			j1 = inX1;
			j2 = inX2;
		} else {
			flipX = 1;
			j1 = inX2;
			j2 = inX1;
		}
		if (inY1 <= inY2) {
			flipY = 0;
			i1 = inY1;
			i2 = inY2;
		} else {
			flipY = 1;
			i1 = inY2;
			i2 = inY1;
		}
	}
	di = i2 - i1;
	dj = j2 - j1;

	for (int i = 0; i <= di; ++i) {
		int j = round(i * dj / (double) di);
		struct pixel pix;
		if (!swap) {
			if (!flipX) {
				pix.x = i1 + i;
			} else {
				pix.x = i2 - i;
			}
			if (!flipY) {
				pix.y = j1 + j;
			} else {
				pix.y = j2 - j;
			}
		} else {
			if (!flipX) {
				pix.x = j1 + j;
			} else {
				pix.x = j2 - j;
			}
			if (!flipY) {
				pix.y = i1 + i;
			} else {
				pix.y = i2 - i;
			}
		}
		pixelArrayAppend(pa, pix);
	}

cleanupNoError:
	flag = 1;
cleanup:
	return flag;
}

int pixelArrayFill(struct pixelArray *pa, struct canvas *c, int x, int y)
{
	int flag = 0;
	if (!pa || !c)
		return 0;

	struct dirsNode {
		int indexPrev;
		int l, t, r, b;
	};

	struct dirsArray {
		int memlen;
		int size;
		struct dirsNode *array;
	};

	int cx = x - c->x;
	int cy = y - c->y;
	SDL_Color color;
	canvasGetColor(c, cx, cy, &color);

	struct dirsArray nodes = {0, 0, NULL};
	int indexCurr = 0;
	int indexPrev = -1;

	struct pixel initPix = {c->x + cx, c->y + cy};
	struct dirsNode initNode = {indexPrev, 0, 0, 0, 0};
	pixelArrayAppend(pa, initPix);
	if (nodes.size * sizeof (struct dirsNode) == nodes.memlen) {
		if (!allocDouble(
				&nodes.array, &nodes.memlen,
				sizeof (struct dirsNode)
				)) {
			return 0;
		}
	}
	nodes.array[nodes.size++] = initNode;
	while (!nodes.array[0].l
	    || !nodes.array[0].t
	    || !nodes.array[0].r
	    || !nodes.array[0].b) {
		struct dirsNode *nodeCurr = nodes.array + indexCurr;
		if (!nodeCurr->l) {
			if (cx > 0) {
				SDL_Color colorNext;
				canvasGetColor(c, cx - 1, cy, &colorNext);
				if (colorNext.r == color.r
				 && colorNext.g == color.g
				 && colorNext.b == color.b
				 && colorNext.a == color.a
				 && !pixelArrayHas(pa, c->x + cx - 1, c->y + cy)) {
					--cx;
				} else {
					nodeCurr->l = 1;
					continue;
				}
			} else {
				nodeCurr->l = 1;
				continue;
			}
		} else if (!nodeCurr->t) {
			if (cy > 0) {
				SDL_Color colorNext;
				canvasGetColor(c, cx, cy - 1, &colorNext);
				if (colorNext.r == color.r
				 && colorNext.g == color.g
				 && colorNext.b == color.b
				 && colorNext.a == color.a
				 && !pixelArrayHas(pa, c->x + cx, c->y + cy - 1)) {
					--cy;
				} else {
					nodeCurr->t = 1;
					continue;
				}
			} else {
				nodeCurr->t = 1;
				continue;
			}
		} else if (!nodeCurr->r) {
			if (cx < c->w - 1) {
				SDL_Color colorNext;
				canvasGetColor(c, cx + 1, cy, &colorNext);
				if (colorNext.r == color.r
				 && colorNext.g == color.g
				 && colorNext.b == color.b
				 && colorNext.a == color.a
				 && !pixelArrayHas(pa, c->x + cx + 1, c->y + cy)) {
					++cx;
				} else {
					nodeCurr->r = 1;
					continue;
				}
			} else {
				nodeCurr->r = 1;
				continue;
			}
		} else if (!nodeCurr->b) {
			if (cy < c->h - 1) {
				SDL_Color colorNext;
				canvasGetColor(c, cx, cy + 1, &colorNext);
				if (colorNext.r == color.r
				 && colorNext.g == color.g
				 && colorNext.b == color.b
				 && colorNext.a == color.a
				 && !pixelArrayHas(pa, c->x + cx, c->y + cy + 1)) {
					++cy;
				} else {
					nodeCurr->b = 1;
					continue;
				}
			} else {
				nodeCurr->b = 1;
				continue;
			}
		} else {
			struct pixel pixPrev = pa->array[indexPrev];
			struct dirsNode *nodePrev = nodes.array + indexPrev;
			if (c->x + cx + 1 == pixPrev.x) {
				nodePrev->l = 1;
			} else if (c->y + cy + 1 == pixPrev.y) {
				nodePrev->t = 1;
			} else if (c->x + cx - 1 == pixPrev.x) {
				nodePrev->r = 1;
			} else if (c->y + cy - 1 == pixPrev.y) {
				nodePrev->b = 1;
			} else {
				goto cleanup;
			}
			cx = pixPrev.x - c->x;
			cy = pixPrev.y - c->y;
			indexCurr = indexPrev;
			indexPrev = nodePrev->indexPrev;
			continue;
		}
		struct pixel pix = {c->x + cx, c->y + cy};
		struct dirsNode nodeNext = {indexCurr, 0, 0, 0, 0};
		indexPrev = indexCurr;
		indexCurr = nodes.size;
		pixelArrayAppend(pa, pix);
		if (nodes.size * sizeof (struct dirsNode) == nodes.memlen) {
			if (!allocDouble(
					&nodes.array, &nodes.memlen,
					sizeof (struct dirsNode)
					)) {
				goto cleanup;
			}
		}
		nodes.array[nodes.size++] = nodeNext;
	}

	flag = 1;
cleanup:
	free(nodes.array);
	return flag;
}

int pixelArrayReset(struct pixelArray *pa)
{
	int flag = 0;
	if (!pa)
		goto cleanup;

	pa->size = 0;

	flag = 1;
cleanup:
	return flag;
}

int pixelArrayHas(struct pixelArray *pa, int x, int y)
{
	if (!pa)
		return 0;
	for (int i = 0; i < pa->size; ++i) {
		if (pa->array[i].x == x
		 && pa->array[i].y == y)
			return 1;
	}
	return 0;
}

int pixelArrayDo(struct pixelArray *pa, struct canvasArray *ca, SDL_Color col)
{
	int flag = 0;
	if (!pa || !ca)
		goto cleanup;

	SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);

	for (int i = 0; i < pa->size; ++i) {
		struct pixel pix = pa->array[i];
		struct canvas *c = canvasArrayFind(ca, pix.x, pix.y);
		if (!c)
			continue;
		SDL_SetRenderDrawColor(c->ren, col.r, col.g, col.b, col.a);
		SDL_SetRenderTarget(ren, c->tex);
		SDL_RenderDrawPoint(ren, pix.x - c->x, pix.y - c->y);
		SDL_RenderDrawPoint(c->ren, pix.x - c->x, pix.y - c->y);
	}
	SDL_SetRenderTarget(ren, NULL);

	flag = 1;
cleanup:
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
			if (state.space) {
				state.warp = 1;
				SDL_WarpMouseInWindow(
						win,
						state.drag.panZoom.initX,
						state.drag.panZoom.initY
						);
			}
			break;
		case D_CANVASNEW:
			{
				struct canvasArray *oldArray =
					canvasArrayCopy(state.canvasSel);
				MAP_CANVASES(oldArray, i, c) {
					if (!c->isSel) {
						canvasFix(c);
						canvasArrayRem(state.canvasSel, c);
					}
				}
				canvasArrayFree(oldArray);
			}
			break;
		case D_CANVASTRANSFORM:
			{
				MAP_CANVASES(state.canvasSel, i, c) {
					canvasFix(c);
				}
				if (state.canvasSel->size == 1
				&& !state.canvasSel->array[0]->isSel)
					canvasArrayRem(
							state.canvasSel,
							state.canvasSel->array[0]
						      );
				free(state.drag.canvasTransform.offX);
				free(state.drag.canvasTransform.offY);
				free(state.drag.canvasTransform.offW);
				free(state.drag.canvasTransform.offH);
				state.drag.canvasTransform.offX = NULL;
				state.drag.canvasTransform.offY = NULL;
				state.drag.canvasTransform.offW = NULL;
				state.drag.canvasTransform.offH = NULL;
				break;
			}
		case D_DRAWPIXEL:
			break;
		case D_DRAWLINE:
			pixelArrayDo(
					state.drag.drawLine.pixels,
					(state.canvasSel->size == 0)
					? state.canvasArr : state.canvasSel,
					state.drag.drawLine.color
				    );
			break;
		default:
			break;
	}

	switch (action) {
		case D_NONE:
			break;
		case D_PANZOOM:
			{
				if (!SDL_GetMouseFocus())
					goto cleanupNoError;
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
				if (!c)
					goto cleanup;
				canvasArrayAppend(state.canvasArr, c);
				canvasArrayAppend(state.canvasSel, c);
				break;
			}
		case D_CANVASTRANSFORM:
			{
				if (state.canvasSel->size == 0) {
					int mx, my;
					SDL_GetMouseState(&mx, &my);
					struct canvas *c = canvasArrayFind(
							state.canvasArr,
							TO_COORD_EASEL_X(mx),
							TO_COORD_EASEL_Y(my)
							);
					if (!c)
						goto cleanupNoError;
					canvasArrayAppend(state.canvasSel, c);
				}
				int *newArray;
				newArray = realloc(
						state.drag.canvasTransform.offX,
						(state.canvasSel->size * sizeof (int *))
						);
				if (!newArray)
					goto cleanup;
				state.drag.canvasTransform.offX = newArray;
				newArray = realloc(
						state.drag.canvasTransform.offY,
						(state.canvasSel->size * sizeof (int *))
						);
				if (!newArray)
					goto cleanup;
				state.drag.canvasTransform.offY = newArray;
				newArray = realloc(
						state.drag.canvasTransform.offW,
						(state.canvasSel->size * sizeof (int *))
						);
				if (!newArray)
					goto cleanup;
				state.drag.canvasTransform.offW = newArray;
				newArray = realloc(
						state.drag.canvasTransform.offH,
						(state.canvasSel->size * sizeof (int *))
						);
				if (!newArray)
					goto cleanup;
				state.drag.canvasTransform.offH = newArray;
				break;
			}
		case D_DRAWPIXEL:
			{
				int mx, my;
				SDL_GetMouseState(&mx, &my);
				state.drag.drawPixel.initX = TO_COORD_EASEL_X(mx);
				state.drag.drawPixel.initY = TO_COORD_EASEL_Y(my);
				pixelArrayReset(state.drag.drawPixel.pixels);
				struct pixel pix = {
					state.drag.drawPixel.initX,
					state.drag.drawPixel.initY
				};
				pixelArrayAppend(
						state.drag.drawPixel.pixels,
						pix
						);
				pixelArrayDo(
						state.drag.drawPixel.pixels,
						(state.canvasSel->size == 0)
						? state.canvasArr : state.canvasSel,
						state.drag.drawPixel.color
					    );
				pixelArrayReset(state.drag.drawPixel.pixels);
				break;
			}
		case D_DRAWLINE:
			{
				int mx, my;
				SDL_GetMouseState(&mx, &my);
				state.drag.drawLine.initX = TO_COORD_EASEL_X(mx);
				state.drag.drawLine.initY = TO_COORD_EASEL_Y(my);
				pixelArrayReset(state.drag.drawLine.pixels);
				canvasClear(state.drag.drawLine.preview);
				canvasMove(
						state.drag.drawLine.preview,
						state.drag.drawLine.initX,
						state.drag.drawLine.initY,
						1, 1
						);
				canvasFix(state.drag.drawLine.preview);
				pixelArrayReset(state.drag.drawLine.previewPixels);
				break;
			}
		default:
			break;
	}

	state.drag.action = action;

cleanupNoError:
	flag = 1;
cleanup:
	return flag;
}

int resetDrag()
{
	int flag = 0;

	if (state.drag.action != D_PANZOOM) {
		if (!setDrag(D_NONE))
			goto cleanup;
	}

	flag = 1;
cleanup:
	return flag;
}

int setScope(enum Scope scope)
{
	int flag = 0;

	if (!resetDrag())
		goto cleanup;
	state.scope = scope;

	flag = 1;
cleanup:
	return flag;
}

int setModeEasel(enum ModeEasel mode)
{
	int flag = 0;

	if (!resetDrag())
		goto cleanup;
	state.modeEasel = mode;

	flag = 1;
cleanup:
	return flag;
}

int setModeCanvas(enum ModeCanvas mode)
{
	int flag = 0;

	if (!resetDrag())
		goto cleanup;
	state.modeCanvas = mode;

	flag = 1;
cleanup:
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
				state.warp = 1;
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

cleanupNoError:
	flag = 1;
cleanup:
	return flag;
}

int frameDo()
{
	int flag = 0;

	SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(ren);

	if (state.canvasArr->size != 0) {
		MAP_CANVASES(state.canvasArr, i, c) {
			int tw, th;
			SDL_QueryTexture(c->tex, NULL, NULL, &tw, &th);
			SDL_Rect src = {
				0, 0,
				(c->w >= tw) ? tw : c->w,
				(c->h >= th) ? th : c->h
			};
			SDL_Rect dst = {
				TO_COORD_SCREEN_X(c->x),
				TO_COORD_SCREEN_Y(c->y),
				state.easel.s * src.w,
				state.easel.s * src.h
			};
			SDL_Rect border = {
				dst.x - 1, dst.y - 1,
				state.easel.s * c->w + 2,
				state.easel.s * c->h + 2
			};

			SDL_RenderCopy(ren, c->tex, &src, &dst);

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
			SDL_RenderDrawRect(ren, &border);
		}
	}

	if (state.drag.action == D_DRAWLINE) {
		struct canvas *c = state.drag.drawLine.preview;
		int tw, th;
		SDL_QueryTexture(c->tex, NULL, NULL, &tw, &th);
		SDL_Rect src = {
			0, 0,
			(c->w >= tw) ? tw : c->w,
			(c->h >= th) ? th : c->h
		};
		SDL_Rect dst = {
			TO_COORD_SCREEN_X(c->x),
			TO_COORD_SCREEN_Y(c->y),
			state.easel.s * src.w,
			state.easel.s * src.h
		};

		SDL_RenderCopy(ren, c->tex, &src, &dst);
	}

	int cursorX;
	int cursorY;
	if (state.drag.action == D_PANZOOM && state.space) {
		cursorX = state.drag.panZoom.initX;
		cursorY = state.drag.panZoom.initY;
	} else {
		SDL_GetMouseState(&cursorX, &cursorY);
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

		if (state.canvasArr->size != 0) {
			MAP_CANVASES(state.canvasArr, i, c) {
				int tw, th;
				SDL_QueryTexture(c->tex, NULL, NULL, &tw, &th);
				SDL_Rect texture = {
					TO_COORD_SCREEN_X(c->x),
					TO_COORD_SCREEN_Y(c->y),
					state.easel.s * tw,
					state.easel.s * th
				};
				SDL_RenderDrawRect(ren, &texture);
			}
		}

		if (state.drag.action == D_DRAWLINE) {
			struct canvas *c = state.drag.drawLine.preview;
			int tw, th;
			SDL_QueryTexture(c->tex, NULL, NULL, &tw, &th);
			SDL_Rect texture = {
				TO_COORD_SCREEN_X(c->x),
				TO_COORD_SCREEN_Y(c->y),
				state.easel.s * tw,
				state.easel.s * th
			};
			SDL_RenderDrawRect(ren, &texture);
		}

		SDL_SetRenderDrawColor(ren, 0, 255, 0, SDL_ALPHA_OPAQUE);

		SDL_Rect bounds = {
			TO_COORD_SCREEN_X(state.easel.minX),
			TO_COORD_SCREEN_Y(state.easel.minY),
			TO_COORD_SCREEN_X(state.easel.maxX)
				- TO_COORD_SCREEN_X(state.easel.minX),
			TO_COORD_SCREEN_Y(state.easel.maxY)
				- TO_COORD_SCREEN_Y(state.easel.minY)
		};
		SDL_RenderDrawRect(ren, &bounds);
	}

	SDL_RenderPresent(ren);

	flag = 1;
cleanup:
	return flag;
}

int eventRender(SDL_Event *e)
{
	int flag = 0;

	if (!texFix())
		goto cleanup;

	flag = 1;
cleanup:
	return flag;
}

int eventKeyDown(SDL_Event *e)
{
	int flag = 0;

	if (e->key.repeat)
		goto cleanupNoError;

	switch (e->key.keysym.sym) {
		case SDLK_F12:
			state.debug = !state.debug;
			goto cleanupNoError;
		case SDLK_v:
			setScope(S_EASEL);
			goto cleanupNoError;
		case SDLK_c:
			setScope(S_CANVAS);
			goto cleanupNoError;
		case SDLK_g:
			setDrag(D_PANZOOM);
			goto cleanupNoError;
		case SDLK_SPACE:
			setSpace(1);
			goto cleanupNoError;
		default:
			break;
	}
	switch (state.scope) {
		case S_EASEL:
			switch (e->key.keysym.sym) {
				case SDLK_r:
					setModeEasel(E_EDIT);
					goto cleanupNoError;
				case SDLK_e:
					setModeEasel(E_TRANSFORM);
					goto cleanupNoError;
				case SDLK_w:
					setModeEasel(E_SELECT);
					goto cleanupNoError;
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
								struct canvas *c = canvasArrayFind(
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
						{
							int mx;
							SDL_GetMouseState(&mx, NULL);
							state.drag.canvasTransform.setX = 1;
							if (state.drag.action != D_CANVASTRANSFORM)
								setDrag(D_CANVASTRANSFORM);
							if (state.canvasSel->size != 0) {
								MAP_CANVASES(state.canvasSel, i, c) {
									state.drag.canvasTransform.offX[i] =
										c->x
										- TO_COORD_EASEL_X(mx);
								}
							}
							break;
						}
						case SDLK_d:
						{
							int my;
							SDL_GetMouseState(NULL, &my);
							state.drag.canvasTransform.setY = 1;
							if (state.drag.action != D_CANVASTRANSFORM)
								setDrag(D_CANVASTRANSFORM);
							if (state.canvasSel->size != 0) {
								MAP_CANVASES(state.canvasSel, i, c) {
									state.drag.canvasTransform.offY[i] =
										c->y
										- TO_COORD_EASEL_Y(my);
								}
							}
							break;
						}
						case SDLK_s:
						{
							int mx;
							SDL_GetMouseState(&mx, NULL);
							state.drag.canvasTransform.setW = 1;
							if (state.drag.action != D_CANVASTRANSFORM)
								setDrag(D_CANVASTRANSFORM);
							if (state.canvasSel->size != 0) {
								MAP_CANVASES(state.canvasSel, i, c) {
									state.drag.canvasTransform.offW[i] =
										c->x + c->w
										- TO_COORD_EASEL_X(mx);
								}
							}
							break;
						}
						case SDLK_a:
						{
							int my;
							SDL_GetMouseState(NULL, &my);
							state.drag.canvasTransform.setH = 1;
							if (state.drag.action != D_CANVASTRANSFORM)
								setDrag(D_CANVASTRANSFORM);
							if (state.canvasSel->size != 0) {
								MAP_CANVASES(state.canvasSel, i, c) {
									state.drag.canvasTransform.offH[i] =
										c->y + c->h
										- TO_COORD_EASEL_Y(my);
								}
							}
							break;
						}
						default:
							break;
					}
					break;
				case E_SELECT:
					switch (e->key.keysym.sym) {
						int sel;
						int mx, my;
						struct canvas *c;
						case SDLK_f:
							sel = 1;
							goto E_SELECT_fd;
						case SDLK_d:
							sel = 0;
							goto E_SELECT_fd;
E_SELECT_fd:
							SDL_GetMouseState(&mx, &my);
							c = canvasArrayFind(
								state.canvasArr,
								TO_COORD_EASEL_X(mx),
								TO_COORD_EASEL_Y(my)
								);
							if (sel) {
								if (c && !c->isSel) {
									canvasArrayAppend(state.canvasSel, c);
									c->isSel = 1;
								}
							} else {
								if (c && c->isSel) {
									canvasArrayRem(state.canvasSel, c);
									c->isSel = 0;
								}
							}
							break;
						case SDLK_s:
							if (state.canvasSel->size != 0) {
								struct canvasArray *oldArray =
									canvasArrayCopy(state.canvasSel);
								MAP_CANVASES(oldArray, i, c) {
									canvasArrayRem(state.canvasSel, c);
									c->isSel = 0;
								}
								canvasArrayFree(oldArray);
							}
							break;
						case SDLK_a:
							if (state.canvasArr->size != 0) {
								MAP_CANVASES(state.canvasArr, i, c) {
									if (!c->isSel) {
										canvasArrayAppend(state.canvasSel, c);
										c->isSel = 1;
									}
								}
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
					goto cleanupNoError;
				case SDLK_e:
					setModeCanvas(C_LINE);
					goto cleanupNoError;
				case SDLK_w:
					setModeCanvas(C_FILL);
					goto cleanupNoError;
				default:
					break;
			}
			switch (state.modeCanvas) {
				case C_PIXEL:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							state.drag.drawPixel.key = KEY_F;
							state.drag.drawPixel.color =
								state.colors.f;
							setDrag(D_DRAWPIXEL);
							break;
						case SDLK_d:
							state.drag.drawPixel.key = KEY_D;
							state.drag.drawPixel.color =
								state.colors.d;
							setDrag(D_DRAWPIXEL);
							break;
						case SDLK_s:
							state.drag.drawPixel.key = KEY_S;
							state.drag.drawPixel.color =
								state.colors.s;
							setDrag(D_DRAWPIXEL);
							break;
						case SDLK_a:
							state.drag.drawPixel.key = KEY_A;
							state.drag.drawPixel.color =
								state.colors.a;
							setDrag(D_DRAWPIXEL);
							break;
						default:
							break;
					}
					break;
				case C_LINE:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							resetDrag();
							state.drag.drawLine.key = KEY_F;
							state.drag.drawLine.color =
								state.colors.f;
							setDrag(D_DRAWLINE);
							break;
						case SDLK_d:
							resetDrag();
							state.drag.drawLine.key = KEY_D;
							state.drag.drawLine.color =
								state.colors.d;
							setDrag(D_DRAWLINE);
							break;
						case SDLK_s:
							resetDrag();
							state.drag.drawLine.key = KEY_S;
							state.drag.drawLine.color =
								state.colors.s;
							setDrag(D_DRAWLINE);
							break;
						case SDLK_a:
							resetDrag();
							state.drag.drawLine.key = KEY_A;
							state.drag.drawLine.color =
								state.colors.a;
							setDrag(D_DRAWLINE);
							break;
						default:
							break;
					}
					break;
				case C_FILL:
					switch (e->key.keysym.sym) {
						int mx, my;
						int easelX, easelY;
						struct canvasArray *ca;
						struct canvas *c;
						struct pixelArray *pa;
						SDL_Color color;
						case SDLK_f:
							color = state.colors.f;
							goto C_FILL_fdsa;
						case SDLK_d:
							color = state.colors.d;
							goto C_FILL_fdsa;
						case SDLK_s:
							color = state.colors.s;
							goto C_FILL_fdsa;
						case SDLK_a:
							color = state.colors.a;
							goto C_FILL_fdsa;
C_FILL_fdsa:
							SDL_GetMouseState(&mx, &my);
							easelX = TO_COORD_EASEL_X(mx);
							easelY = TO_COORD_EASEL_Y(my);
							ca = (state.canvasSel->size == 0)
								? state.canvasArr
								: state.canvasSel;
							c = canvasArrayFind(ca, easelX, easelY);
							if (!c)
								goto cleanupNoError;
							pa = pixelArrayNew();
							pixelArrayFill(pa, c, easelX, easelY);
							pixelArrayDo(pa, ca, color);
							pixelArrayFree(pa);
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

cleanupNoError:
	flag = 1;
cleanup:
	return flag;
}

int eventKeyUp(SDL_Event *e)
{
	int flag = 0;

	switch (e->key.keysym.sym) {
		case SDLK_g:
			if (state.drag.action == D_PANZOOM)
				setDrag(D_NONE);
			goto cleanup;
		case SDLK_SPACE:
			setSpace(0);
			goto cleanup;
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
							state.drag.canvasTransform.setX = 0;
							if (state.drag.canvasTransform.setY == 0
							 && state.drag.canvasTransform.setW == 0
							 && state.drag.canvasTransform.setH == 0)
								setDrag(D_NONE);
							break;
						case SDLK_d:
							state.drag.canvasTransform.setY = 0;
							if (state.drag.canvasTransform.setX == 0
							 && state.drag.canvasTransform.setW == 0
							 && state.drag.canvasTransform.setH == 0)
								setDrag(D_NONE);
							break;
						case SDLK_s:
							state.drag.canvasTransform.setW = 0;
							if (state.drag.canvasTransform.setX == 0
							 && state.drag.canvasTransform.setY == 0
							 && state.drag.canvasTransform.setH == 0)
								setDrag(D_NONE);
							break;
						case SDLK_a:
							state.drag.canvasTransform.setH = 0;
							if (state.drag.canvasTransform.setX == 0
							 && state.drag.canvasTransform.setY == 0
							 && state.drag.canvasTransform.setW == 0)
								setDrag(D_NONE);
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
							if (state.drag.drawPixel.key == KEY_F)
								setDrag(D_NONE);
							break;
						case SDLK_d:
							if (state.drag.drawPixel.key == KEY_D)
								setDrag(D_NONE);
							break;
						case SDLK_s:
							if (state.drag.drawPixel.key == KEY_S)
								setDrag(D_NONE);
							break;
						case SDLK_a:
							if (state.drag.drawPixel.key == KEY_A)
								setDrag(D_NONE);
							break;
						default:
							break;
					}
					break;
				case C_LINE:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							if (state.drag.drawLine.key == KEY_F)
								setDrag(D_NONE);
							break;
						case SDLK_d:
							if (state.drag.drawLine.key == KEY_D)
								setDrag(D_NONE);
							break;
						case SDLK_s:
							if (state.drag.drawLine.key == KEY_S)
								setDrag(D_NONE);
							break;
						case SDLK_a:
							if (state.drag.drawLine.key == KEY_A)
								setDrag(D_NONE);
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
cleanup:
	return flag;
}

int eventMouseMotion(SDL_Event *e)
{
	// TODO create event for cursor moving on easel scale
	// and separate out corresponding drag actions into new event handler function
	int flag = 0;

	switch (state.drag.action) {
		case D_NONE:
			break;
		case D_PANZOOM:
			{
				int focusX;
				int focusY;
				if (state.space) {
					if (!(state.easel.s == MAX_ZOOM && e->motion.yrel < 0)
					&& (!(state.easel.s == 1        && e->motion.yrel > 0)))
						state.drag.panZoom.accumStep -= e->motion.yrel;
					else
						state.drag.panZoom.accumStep = 0;
					int accum = state.drag.panZoom.accumStep;
					if (accum >= 0) {
						int quota = ceiling(48.0 / state.easel.s);
						while (accum >= quota && state.easel.s < MAX_ZOOM) {
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
					focusX = e->motion.x;
					focusY = e->motion.y;
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
				easelFix();
				break;
			}
		case D_CANVASNEW:
			{
				MAP_CANVASES(state.canvasSel, i, c) {
					if (!c->isSel) {
						canvasMove(c, c->x, c->y,
								TO_COORD_EASEL_X(e->motion.x) - c->x,
								TO_COORD_EASEL_Y(e->motion.y) - c->y
							  );
					}
				}
				break;
			}
		case D_CANVASTRANSFORM:
			{
				MAP_CANVASES(state.canvasSel, i, c) {
					int x = state.drag.canvasTransform.setX
						? TO_COORD_EASEL_X(e->motion.x)
						    + state.drag.canvasTransform.offX[i]
						: c->x;
					int y = state.drag.canvasTransform.setY
						? TO_COORD_EASEL_Y(e->motion.y)
						    + state.drag.canvasTransform.offY[i]
						: c->y;
					int w = state.drag.canvasTransform.setW
						? TO_COORD_EASEL_X(e->motion.x) - c->x
						    + state.drag.canvasTransform.offW[i]
						: c->w;
					int h = state.drag.canvasTransform.setH
						? TO_COORD_EASEL_Y(e->motion.y) - c->y
						    + state.drag.canvasTransform.offH[i]
						: c->h;
					canvasMove(c, x, y, w, h);
				}
				break;
			}
		case D_DRAWPIXEL:
			{
				pixelArrayReset(state.drag.drawPixel.pixels);
				pixelArrayLine(
						state.drag.drawPixel.pixels,
						state.drag.drawPixel.initX,
						state.drag.drawPixel.initY,
						TO_COORD_EASEL_X(e->motion.x),
						TO_COORD_EASEL_Y(e->motion.y)
					      );
				pixelArrayDo(
						state.drag.drawPixel.pixels,
						(state.canvasSel->size == 0)
						? state.canvasArr : state.canvasSel,
						state.drag.drawPixel.color
					    );
				state.drag.drawPixel.initX = TO_COORD_EASEL_X(e->motion.x);
				state.drag.drawPixel.initY = TO_COORD_EASEL_Y(e->motion.y);
				break;
			}
		case D_DRAWLINE:
			{
				int easelX = TO_COORD_EASEL_X(e->motion.x);
				int easelY = TO_COORD_EASEL_Y(e->motion.y);
				pixelArrayReset(state.drag.drawLine.pixels);
				pixelArrayLine(
						state.drag.drawLine.pixels,
						state.drag.drawLine.initX,
						state.drag.drawLine.initY,
						easelX, easelY
					      );
				canvasClear(state.drag.drawLine.preview);
				if (easelX >= state.drag.drawLine.initX) {
					if (easelY >= state.drag.drawLine.initY) {
						canvasMove(
								state.drag.drawLine.preview,
								state.drag.drawLine.initX,
								state.drag.drawLine.initY,
								easelX - state.drag.drawLine.initX + 1,
								easelY - state.drag.drawLine.initY + 1
							  );
					} else {
						canvasMove(
								state.drag.drawLine.preview,
								state.drag.drawLine.initX,
								easelY,
								easelX - state.drag.drawLine.initX + 1,
								state.drag.drawLine.initY - easelY + 1
							  );
					}
				} else {
					if (easelY >= state.drag.drawLine.initY) {
						canvasMove(
								state.drag.drawLine.preview,
								easelX,
								state.drag.drawLine.initY,
								state.drag.drawLine.initX - easelX + 1,
								easelY - state.drag.drawLine.initY + 1
							  );
					} else {
						canvasMove(
								state.drag.drawLine.preview,
								easelX,
								easelY,
								state.drag.drawLine.initX - easelX + 1,
								state.drag.drawLine.initY - easelY + 1
							  );
					}
				}
				canvasFix(state.drag.drawLine.preview);
				pixelArrayReset(state.drag.drawLine.previewPixels);
				pixelArrayLine(
						state.drag.drawLine.previewPixels,
						state.drag.drawLine.initX,
						state.drag.drawLine.initY,
						easelX, easelY
					      );
				pixelArrayDo(
						state.drag.drawLine.previewPixels,
						state.drag.drawLine.previewArr,
						state.drag.drawLine.color
					    );
				break;
			}
		default:
			break;
	}

	flag = 1;
cleanup:
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
					goto cleanup;
				break;
			case SDL_KEYUP:
				if (!eventKeyUp(e))
					goto cleanup;
				break;
			case SDL_MOUSEMOTION:
				if (!eventMouseMotion(e))
					goto cleanup;
				break;
			case SDL_MOUSEBUTTONDOWN:
				break;
			case SDL_MOUSEBUTTONUP:
				break;
			case SDL_MOUSEWHEEL:
				break;
			case SDL_DROPFILE:
				break;
			case SDL_RENDER_TARGETS_RESET:
			case SDL_RENDER_DEVICE_RESET:
				if (!eventRender(e))
					goto cleanup;
				break;
			default:
				break;
		}
	}

	flag = 1;
cleanup:
	return flag;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, NULL);

	int flag = 1;

	if (!init())
		goto cleanup;

	struct canvas *a = canvasNew(
			(INIT_WIN_WIDTH / INIT_SCALE - INIT_CANVAS_WIDTH) / 2,
			(INIT_WIN_HEIGHT / INIT_SCALE - INIT_CANVAS_HEIGHT) / 2,
			INIT_CANVAS_WIDTH, INIT_CANVAS_HEIGHT
			);
	canvasArrayAppend(state.canvasArr, a);

	Uint32 tickCurr;
	Uint32 tickNext = 0;
	int delta = 1000 / FPS;
	while (!state.quit) {
		tickCurr = SDL_GetTicks();
		if (SDL_TICKS_PASSED(tickCurr, tickNext)) {
			tickNext = tickCurr + delta;
			if (state.warp)
				state.warp = 0;
			else
				frameDo();
		}
		SDL_Event e;
		eventDo(&e);
	}

	flag = 0;
cleanup:
	if (flag)
		puts(SDL_GetError());
	quit();
	return flag;
}
