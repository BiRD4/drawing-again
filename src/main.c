#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <tinyfiledialogs.h>

#define FPS 60
#define MAX_ZOOM 64
#define MAX_PATHLEN 1024
#define INIT_SCALE 16
#define INIT_WIN_WIDTH 512
#define INIT_WIN_HEIGHT 512
#define INIT_CANVAS_WIDTH 16
#define INIT_CANVAS_HEIGHT 16

#define TO_COORD_EASEL_X(c) (((c) - state.easel.x) / state.easel.s \
                           - ((c) - state.easel.x < 0))
#define TO_COORD_EASEL_Y(c) (((c) - state.easel.y) / state.easel.s \
                           - ((c) - state.easel.y < 0))
#define TO_COORD_SCREEN_X(c) ((c) * state.easel.s + state.easel.x)
#define TO_COORD_SCREEN_Y(c) ((c) * state.easel.s + state.easel.y)

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
	char path[MAX_PATHLEN];
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

struct pixelMask {
	int x;
	int y;
	int w;
	int h;
	Uint8 *array;
};

struct {

	int quit;
	int warp;
	int debug;
	int space;
	int blend;

	struct {
		int x;
	} drop;

	struct {
		int x;
		int y;
		int s;

		int minX;
		int minY;
		int maxX;
		int maxY;
	} easel;

	// TODO add C_SELECT
	enum Scope {S_EASEL, S_CANVAS, S_PICK} scope;
	enum ModeEasel {E_EDIT, E_TRANSFORM, E_SELECT} modeEasel;
	enum ModeCanvas {C_PIXEL, C_LINE, C_RECT, C_FILL} modeCanvas;
	enum ModePick {P_F, P_D, P_S, P_A} modePick;

	struct {
		enum ActionDrag {
			D_NONE,
			D_PANZOOM,
			D_CANVASNEW,
			D_CANVASTRANSFORM,
			D_DRAWPIXEL,
			D_DRAWLINE,
			D_DRAWRECT,
			D_PICK
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
			int currX;
			int currY;
			enum Key key;
			SDL_Color color;
			struct pixelArray *pixels;
			struct canvas *preview;
			struct canvasArray *previewArr;
		} drawLine;
		struct {
			int initX;
			int initY;
			int currX;
			int currY;
			enum Key key;
			SDL_Color color;
		} drawRect;
		struct {
			int pickRed;
			int pickGreen;
			int pickBlue;
			int pickAlpha;
		} pick;
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
	0, 0, 0, 0, 1,
	{{0}},
	{0, 0, INIT_SCALE, 0, 0, 0, 0},
	S_EASEL, E_EDIT, C_PIXEL, P_F,
	{
		D_NONE,
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, NULL, NULL, NULL, NULL},
		{0, 0, KEY_F, {0, 0, 0, 0}, NULL},
		{0, 0, 0, 0, KEY_F, {0, 0, 0, 0}, NULL, NULL, NULL, NULL},
		{0, 0, 0, 0, KEY_F, {0, 0, 0, 0}},
		{0, 0, 0, 0}
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

SDL_Texture *texRuler;
enum Side {SIDE_BOTTOM, SIDE_LEFT, SIDE_TOP, SIDE_RIGHT};

struct canvas *canvasNew(int x, int y, int w, int h);
struct canvasArray *canvasArrayNew();
struct pixelArray *pixelArrayNew();

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

	if (!IMG_Init(IMG_INIT_PNG))
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
	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

	SDL_Surface *rulerSurf = SDL_CreateRGBSurfaceWithFormat(
			0, 256, 8, 32, SDL_PIXELFORMAT_ARGB32
			);
	if (!rulerSurf)
		goto cleanup;
	SDL_Renderer *rulerRen = SDL_CreateSoftwareRenderer(rulerSurf);
	if (!rulerRen)
		goto cleanup;

	for (int i = 0; i < rulerSurf->h; ++i) {
		int val = 255;
		for (int j = 0; j < rulerSurf->w; ++j) {
			if (j % (int) pow(2, i) == 0)
				val = !val * 255;
			SDL_SetRenderDrawColor(rulerRen, val, val, val, 127);
			SDL_RenderDrawPoint(rulerRen, j, i);
		}
	}

	texRuler = SDL_CreateTextureFromSurface(ren, rulerSurf);
	if (!texRuler)
		goto cleanup;

	SDL_DestroyRenderer(rulerRen);
	SDL_FreeSurface(rulerSurf);

	state.drag.drawPixel.pixels = pixelArrayNew();

	state.drag.drawLine.pixels = pixelArrayNew();
	state.drag.drawLine.preview = canvasNew(0, 0, 1, 1);
	state.drag.drawLine.previewArr = canvasArrayNew();
	canvasArrayAppend(
			state.drag.drawLine.previewArr,
			state.drag.drawLine.preview
			);

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
	SDL_DestroyTexture(texRuler);
	SDL_Quit();
}

char *dialogFileSave(char *title, char *initPath)
{
	char *patterns[] = {"*.png"};
	return tinyfd_saveFileDialog(title, initPath, 1, patterns, NULL);
}

char *dialogFileOpen(char *title, char *initPath, int multiSel)
{
	char *patterns[] = {"*.png"};
	return tinyfd_openFileDialog(title, initPath, 1, patterns, NULL, multiSel);
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
			SDL_TEXTUREACCESS_TARGET,
			w, h
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
	SDL_SetSurfaceBlendMode(c->surf, SDL_BLENDMODE_NONE);
	c->ren = SDL_CreateSoftwareRenderer(c->surf);
	if (!c->ren)
		goto cleanup_ren;
	SDL_SetRenderDrawBlendMode(c->ren, SDL_BLENDMODE_BLEND);
	c->path[0] = '\0';
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
		canvasArrayRemove(state.canvasArr, c);
	if (canvasArrayHas(state.canvasSel, c))
		canvasArrayRemove(state.canvasSel, c);
	SDL_DestroyTexture(c->tex);
	SDL_DestroyRenderer(c->ren);
	SDL_FreeSurface(c->surf);
	free(c);

	flag = 1;
cleanup:
	return flag;
}

int canvasSave(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	if (IMG_SavePNG(c->surf, c->path))
		goto cleanup;

	flag = 1;
cleanup:
	return flag;
}

int canvasLoad(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	SDL_Surface *newSurf = IMG_Load(c->path);
	if (!newSurf)
		goto cleanup;
	newSurf = SDL_ConvertSurfaceFormat(newSurf, SDL_PIXELFORMAT_ARGB32, 0);
	if (!newSurf)
		goto cleanup;
	SDL_Renderer *newRen = SDL_CreateSoftwareRenderer(newSurf);
	if (!newRen)
		goto cleanup;
	SDL_SetRenderDrawBlendMode(newRen, SDL_BLENDMODE_BLEND);
	SDL_DestroyRenderer(c->ren);
	SDL_FreeSurface(c->surf);
	c->surf = newSurf;
	c->ren = newRen;
	c->w = c->surf->w;
	c->h = c->surf->h;
	canvasFix(c);

	flag = 1;
cleanup:
	return flag;
}

int canvasSaveAs(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	char *path = dialogFileSave("Save as", c->path);
	if (!path)
		goto cleanup;
	int i;
	for (i = 0; path[i] != '\0' && i < MAX_PATHLEN - 1; ++i)
		c->path[i] = path[i];
	c->path[i] = '\0';
	canvasSave(c);

	flag = 1;
cleanup:
	return flag;
}

int canvasOpen(struct canvas *c)
{
	int flag = 0;
	if (!c)
		goto cleanup;

	char *path = dialogFileOpen("Open file", c->path, 0);
	if (!path)
		goto cleanup;
	int i;
	for (i = 0; path[i] != '\0' && i < MAX_PATHLEN - 1; ++i)
		c->path[i] = path[i];
	c->path[i] = '\0';
	canvasLoad(c);

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
				SDL_TEXTUREACCESS_TARGET,
				c->w, c->h
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
		SDL_SetSurfaceBlendMode(c->surf, SDL_BLENDMODE_NONE);
		SDL_Renderer *newRen = SDL_CreateSoftwareRenderer(newSurf);
		if (!newRen)
			goto cleanup;
		SDL_SetRenderDrawBlendMode(newRen, SDL_BLENDMODE_BLEND);
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

int canvasArrayRemove(struct canvasArray *ca, struct canvas *c)
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

int canvasArrayOpen(struct canvasArray *ca)
{
	int flag = 0;

	char *paths = dialogFileOpen("Open files", NULL, 1);
	if (!paths)
		goto cleanup;

	int done = 0;
	int i = 0;
	char *ch = paths;
	while (!done && i < ca->size) {
		int k = 0;
		while (!done && *ch != '|') {
			if (k < MAX_PATHLEN - 1)
				ca->array[i]->path[k++] = *ch;
			if (*ch++ == '\0')
				done = 1;
		}
		ch++;
		ca->array[i]->path[k] = '\0';
		canvasLoad(ca->array[i++]);
	}

cleanupNoError:
	flag = 1;
cleanup:
	return flag;
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

int pixelArrayLine(struct pixelArray *pa, int inX1, int inY1, int inX2, int inY2, int skipFirst)
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

	int start = !skipFirst ? 0 : 1;
	for (int i = start; i <= di; ++i) {
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

	Uint8 *bytes = malloc(c->w * c->h);
	if (!bytes)
		return 0;
	for (int i = 0; i < c->w * c->h; ++i)
		bytes[i] = 0;

	int cx = x - c->x;
	int cy = y - c->y;
	SDL_Color color;
	canvasGetColor(c, cx, cy, &color);

	struct pixel pix = {c->x + cx, c->y + cy};
	pixelArrayAppend(pa, pix);

	int done = 0;
	while (!done) {
		Uint8 *byte = bytes + c->w * cy + cx;
		SDL_Color colorNext;
		if (cx != 0) {
			canvasGetColor(c, cx - 1, cy, &colorNext);
			if (
				(*byte & 0x01) == 0
				&& *(byte - 1) == 0
				&& colorNext.r == color.r
				&& colorNext.g == color.g
				&& colorNext.b == color.b
				&& colorNext.a == color.a
			   ) {
				*(byte    ) |= 0x01;
				*(byte - 1) |= 0x10;
				--cx;
				goto pixAppend;
			}
		}
		if (cy != 0) {
			canvasGetColor(c, cx, cy - 1, &colorNext);
			if (
				(*byte & 0x02) == 0
				&& *(byte - c->w) == 0
				&& colorNext.r == color.r
				&& colorNext.g == color.g
				&& colorNext.b == color.b
				&& colorNext.a == color.a
			   ) {
				*(byte       ) |= 0x03;
				*(byte - c->w) |= 0x20;
				--cy;
				goto pixAppend;
			}
		}
		if (cx != c->w - 1) {
			canvasGetColor(c, cx + 1, cy, &colorNext);
			if (
				(*byte & 0x04) == 0
				&& *(byte + 1) == 0
				&& colorNext.r == color.r
				&& colorNext.g == color.g
				&& colorNext.b == color.b
				&& colorNext.a == color.a
			   ) {
				*(byte    ) |= 0x07;
				*(byte + 1) |= 0x40;
				++cx;
				goto pixAppend;
			}
		}
		if (cy != c->h - 1) {
			canvasGetColor(c, cx, cy + 1, &colorNext);
			if (
				(*byte & 0x08) == 0
				&& *(byte + c->w) == 0
				&& colorNext.r == color.r
				&& colorNext.g == color.g
				&& colorNext.b == color.b
				&& colorNext.a == color.a
			   ) {
				*(byte       ) |= 0x0f;
				*(byte + c->w) |= 0x80;
				++cy;
				goto pixAppend;
			}
		}
		*byte |= 0x0f;
		switch (*byte & 0xf0) {
			case 0x10:
				++cx;
				continue;
			case 0x20:
				++cy;
				continue;
			case 0x40:
				--cx;
				continue;
			case 0x80:
				--cy;
				continue;
			default:
				done = 1;
				continue;
		}
pixAppend:
		pix.x = c->x + cx;
		pix.y = c->y + cy;
		pixelArrayAppend(pa, pix);
	}

	flag = 1;
cleanup:
	free(bytes);
	return flag;
}

int pixelArrayRemove(struct pixelArray *pa, int index)
{
	int flag = 0;
	if (!pa || pa->size < 1)
		goto cleanup;

	--pa->size;
	for (int i = index; i < pa->size; ++i)
		pa->array[i] = pa->array[i + 1];

	flag = 1;
cleanup:
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

int pixelArrayDo(struct pixelArray *pa, struct canvasArray *ca, SDL_Color col, int blend)
{
	int flag = 0;
	if (!pa || !ca)
		goto cleanup;

	if (blend)
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
	else
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

	SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);

	for (int i = 0; i < pa->size; ++i) {
		struct pixel pix = pa->array[i];
		struct canvas *c = canvasArrayFind(ca, pix.x, pix.y);
		if (!c)
			continue;
		if (blend)
			SDL_SetRenderDrawBlendMode(c->ren, SDL_BLENDMODE_BLEND);
		else
			SDL_SetRenderDrawBlendMode(c->ren, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(c->ren, col.r, col.g, col.b, col.a);
		SDL_SetRenderTarget(ren, c->tex);
		SDL_RenderDrawPoint(ren, pix.x - c->x, pix.y - c->y);
		SDL_RenderDrawPoint(c->ren, pix.x - c->x, pix.y - c->y);
	}
	SDL_SetRenderTarget(ren, NULL);

	if (state.blend)
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
	else
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

	flag = 1;
cleanup:
	return flag;
}

struct pixelMask *pixelMaskNew(int x, int y, int w, int h, Uint8 initVal)
{
	if (w < 1 || h < 1)
		return NULL;
	struct pixelMask *pm = malloc(sizeof (struct pixelMask));
	pm->x = x;
	pm->y = y;
	pm->w = w;
	pm->h = h;
	pm->array = malloc(w * h * sizeof (Uint8));
	if (!pm->array) {
		free(pm);
		return NULL;
	}
	for (int i = 0; i < pm->w * pm->h; ++i) {
		pm->array[i] = initVal;
	}
	return pm;
}

int pixelMaskFree(struct pixelMask *pm)
{
	int flag = 0;
	if (!pm)
		goto cleanup;

	free(pm->array);
	free(pm);

	flag = 1;
cleanup:
	return flag;
}

int pixelMaskFill(struct pixelMask *pm, struct canvas *c, int x, int y)
{
	int flag = 0;
	if (!pm || !c)
		goto cleanup;

	int cx = x - c->x;
	int cy = y - c->y;
	SDL_Color color;
	canvasGetColor(c, cx, cy, &color);

	int done = 0;
	while (!done) {
		Uint8 *pix = pm->array + pm->w * cy + cx;
		SDL_Color colorNext;
		if (cx != 0) {
			canvasGetColor(c, cx - 1, cy, &colorNext);
			if (
				(*pix & 0x01) == 0
				&& *(pix - 1) == 0
				&& colorNext.r == color.r
				&& colorNext.g == color.g
				&& colorNext.b == color.b
				&& colorNext.a == color.a
			   ) {
				*(pix    ) |= 0x01;
				*(pix - 1) |= 0x10;
				--cx;
				continue;
			}
		}
		if (cy != 0) {
			canvasGetColor(c, cx, cy - 1, &colorNext);
			if (
				(*pix & 0x02) == 0
				&& *(pix - pm->w) == 0
				&& colorNext.r == color.r
				&& colorNext.g == color.g
				&& colorNext.b == color.b
				&& colorNext.a == color.a
			   ) {
				*(pix       ) |= 0x03;
				*(pix - pm->w) |= 0x20;
				--cy;
				continue;
			}
		}
		if (cx != c->w - 1) {
			canvasGetColor(c, cx + 1, cy, &colorNext);
			if (
				(*pix & 0x04) == 0
				&& *(pix + 1) == 0
				&& colorNext.r == color.r
				&& colorNext.g == color.g
				&& colorNext.b == color.b
				&& colorNext.a == color.a
			   ) {
				*(pix    ) |= 0x07;
				*(pix + 1) |= 0x40;
				++cx;
				continue;
			}
		}
		if (cy != c->h - 1) {
			canvasGetColor(c, cx, cy + 1, &colorNext);
			if (
				(*pix & 0x08) == 0
				&& *(pix + pm->w) == 0
				&& colorNext.r == color.r
				&& colorNext.g == color.g
				&& colorNext.b == color.b
				&& colorNext.a == color.a
			   ) {
				*(pix       ) |= 0x0f;
				*(pix + pm->w) |= 0x80;
				++cy;
				continue;
			}
		}
		*pix |= 0x0f;
		switch (*pix & 0xf0) {
			case 0x10:
				++cx;
				continue;
			case 0x20:
				++cy;
				continue;
			case 0x40:
				--cx;
				continue;
			case 0x80:
				--cy;
				continue;
			default:
				done = 1;
				continue;
		}
	}

	flag = 1;
cleanup:
	return flag;
}

int pixelMaskDo(struct pixelMask *pm, struct canvasArray *ca, SDL_Color col)
{
	int flag = 0;
	if (!pm || !ca)
		goto cleanup;

	SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);

	for (int i = 0; i < pm->h; ++i) {
		for (int j = 0; j < pm->w; ++j) {
			if (!pm->array[i * pm->w + j])
				continue;
			struct canvas *c = canvasArrayFind(ca, pm->x + j, pm->y + i);
			if (!c)
				continue;
			if (state.blend)
				SDL_SetRenderDrawBlendMode(c->ren, SDL_BLENDMODE_BLEND);
			else
				SDL_SetRenderDrawBlendMode(c->ren, SDL_BLENDMODE_NONE);
			SDL_SetRenderDrawColor(c->ren, col.r, col.g, col.b, col.a);
			SDL_SetRenderTarget(ren, c->tex);
			SDL_RenderDrawPoint(ren, pm->x - c->x + j, pm->y - c->y + i);
			SDL_RenderDrawPoint(c->ren, pm->x - c->x + j, pm->y - c->y + i);
		}
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
						canvasArrayRemove(state.canvasSel, c);
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
					canvasArrayRemove(
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
					state.drag.drawLine.color, 1
				    );
			break;
		case D_DRAWRECT:
			{
				int x, y, w, h;
				if (state.drag.drawRect.currX >= state.drag.drawRect.initX) {
						x = state.drag.drawRect.initX;
						w = state.drag.drawRect.currX - state.drag.drawRect.initX + 1;
				} else {
						x = state.drag.drawRect.currX;
						w = state.drag.drawRect.initX - state.drag.drawRect.currX + 1;
				}
				if (state.drag.drawRect.currY >= state.drag.drawRect.initY) {
						y = state.drag.drawRect.initY;
						h = state.drag.drawRect.currY - state.drag.drawRect.initY + 1;
				} else {
						y = state.drag.drawRect.currY;
						h = state.drag.drawRect.initY - state.drag.drawRect.currY + 1;
				}
				struct pixelMask *pm = pixelMaskNew(x, y, w, h, 1);
				struct canvasArray *ca =
					(state.canvasSel->size == 0)
					? state.canvasArr
					: state.canvasSel;
				pixelMaskDo(pm, ca, state.drag.drawRect.color);
				pixelMaskFree(pm);
				break;
			}
		case D_PICK:
			SDL_CaptureMouse(SDL_FALSE);
			state.drag.pick.pickRed = 0;
			state.drag.pick.pickGreen = 0;
			state.drag.pick.pickBlue = 0;
			state.drag.pick.pickAlpha = 0;
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
				newArray = calloc(state.canvasSel->size, sizeof (int *));
				if (!newArray)
					goto cleanup;
				state.drag.canvasTransform.offX = newArray;
				newArray = calloc(state.canvasSel->size, sizeof (int *));
				if (!newArray)
					goto cleanup;
				state.drag.canvasTransform.offY = newArray;
				newArray = calloc(state.canvasSel->size, sizeof (int *));
				if (!newArray)
					goto cleanup;
				state.drag.canvasTransform.offW = newArray;
				newArray = calloc(state.canvasSel->size, sizeof (int *));
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
						state.drag.drawPixel.color, 1
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
				state.drag.drawLine.currX = state.drag.drawLine.initX;
				state.drag.drawLine.currY = state.drag.drawLine.initY;
				pixelArrayReset(state.drag.drawLine.pixels);
				canvasClear(state.drag.drawLine.preview);
				canvasMove(
						state.drag.drawLine.preview,
						state.drag.drawLine.initX,
						state.drag.drawLine.initY,
						1, 1
					  );
				canvasFix(state.drag.drawLine.preview);
				break;
			}
		case D_DRAWRECT:
			{
				int mx, my;
				SDL_GetMouseState(&mx, &my);
				state.drag.drawRect.initX = TO_COORD_EASEL_X(mx);
				state.drag.drawRect.initY = TO_COORD_EASEL_Y(my);
				state.drag.drawRect.currX = state.drag.drawRect.initX;
				state.drag.drawRect.currY = state.drag.drawRect.initY;
				break;
			}
		case D_PICK:
			SDL_CaptureMouse(SDL_TRUE);
			break;
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

int setModePick(enum ModePick mode)
{
	int flag = 0;

	if (!resetDrag())
		goto cleanup;
	state.modePick = mode;

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
			case D_DRAWRECT:
				break;
			case D_PICK:
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
			case D_DRAWRECT:
				break;
			case D_PICK:
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

SDL_Rect drawRuler(SDL_Renderer *r, int length, int x, int y, int scaleLength, int scaleHeight, enum Side side, SDL_RendererFlip flip)
{
	int tw, th;
	SDL_QueryTexture(texRuler, NULL, NULL, &tw, &th);

	int pixels = ceil((float) length / scaleLength);
	int segs = ceil((float) pixels / tw);

	SDL_Rect rect;
	int rulerHeight = (scaleHeight * ceil(log2(pixels)));
	rulerHeight += (rulerHeight == 0) ? scaleHeight : 0;
	switch (side) {
		case SIDE_BOTTOM:
			rect.x = x;
			rect.y = y;
			rect.w = length;
			rect.h = rulerHeight;
			break;
		case SIDE_LEFT:
			rect.x = x - rulerHeight;
			rect.y = y;
			rect.w = rulerHeight;
			rect.h = length;
			break;
		case SIDE_TOP:
			rect.x = x;
			rect.y = y - rulerHeight;
			rect.w = length;
			rect.h = rulerHeight;
			break;
		case SIDE_RIGHT:
			rect.x = x;
			rect.y = y;
			rect.w = rulerHeight;
			rect.h = length;
			break;
		default:
			break;
	};

	SDL_RenderSetClipRect(r, &rect);

	if (segs != 1) {
		int nextX, nextY;
		switch (side) {
			case SIDE_BOTTOM:
				nextX = x;
				nextY = y + scaleHeight * th;
				break;
			case SIDE_LEFT:
				nextX = x - scaleHeight * th;
				nextY = y;
				break;
			case SIDE_TOP:
				nextX = x;
				nextY = y - scaleHeight * th;
				break;
			case SIDE_RIGHT:
				nextX = x + scaleHeight * th;
				nextY = y;
				break;
			default:
				break;
		}
		drawRuler(
			r, length,
			nextX, nextY,
			scaleLength * tw, scaleHeight,
			side, flip
			);
	}
	
	int angle;
	switch (side) {
		case SIDE_BOTTOM:
			angle = 0;
			flip ^= SDL_FLIP_NONE;
			break;
		case SIDE_LEFT:
			angle = 90;
			flip ^= SDL_FLIP_NONE;
			break;
		case SIDE_TOP:
			angle = 0;
			flip ^= SDL_FLIP_VERTICAL;
			break;
		case SIDE_RIGHT:
			angle = 90;
			flip ^= SDL_FLIP_VERTICAL;
			break;
		default:
			break;
	}
	SDL_Point center = {0, 0};

	int leftPixels = pixels;
	for (int i = 0; i < segs; ++i) {
		int currPixels = (leftPixels < tw) ? leftPixels : tw;
		leftPixels -= currPixels;
		SDL_Rect src = {0, 0, currPixels, ceil(log2(currPixels))};
		SDL_Rect dst;
		switch (side) {
			case SIDE_BOTTOM:
				dst.x = x + scaleLength * tw * i;
				dst.y = y;
				break;
			case SIDE_LEFT:
				dst.x = x;
				dst.y = y + scaleLength * tw * i;
				break;
			case SIDE_TOP:
				dst.x = x + scaleLength * tw * i;
				dst.y = y - scaleHeight * src.h;
				break;
			case SIDE_RIGHT:
				dst.x = x + scaleHeight * src.h;
				dst.y = y + scaleLength * tw * i;
				break;
			default:
				break;
		}
		dst.w = scaleLength * src.w;
		dst.h = scaleHeight * src.h;
		SDL_RenderCopyEx(r, texRuler, &src, &dst, angle, &center, flip);
	}

	SDL_RenderSetClipRect(r, NULL);

	return rect;
}

int frameDo()
{
	int flag = 0;

	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

	SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(ren);

	if (state.canvasArr->size != 0) {
		MAP_CANVASES(state.canvasArr, i, c) {
			int tw, th;
			SDL_QueryTexture(c->tex, NULL, NULL, &tw, &th);
			SDL_Rect rectSrc = {
				0, 0,
				(c->w >= tw) ? tw : c->w,
				(c->h >= th) ? th : c->h
			};
			SDL_Rect rectDst = {
				TO_COORD_SCREEN_X(c->x),
				TO_COORD_SCREEN_Y(c->y),
				state.easel.s * rectSrc.w,
				state.easel.s * rectSrc.h
			};
			SDL_RenderCopy(ren, c->tex, &rectSrc, &rectDst);

			SDL_Rect rectRulerBottom = drawRuler(
					ren,
					state.easel.s * c->w,
					rectDst.x,
					rectDst.y + state.easel.s * c->h,
					state.easel.s,
					(state.easel.s < 8)
					? 1 : state.easel.s / 8,
					SIDE_BOTTOM, SDL_FLIP_NONE
					);
			SDL_Rect rectRulerLeft = drawRuler(
					ren,
					state.easel.s * c->h,
					rectDst.x,
					rectDst.y,
					state.easel.s,
					(state.easel.s < 8)
					? 1 : state.easel.s / 8,
					SIDE_LEFT, SDL_FLIP_NONE
					);
			SDL_Rect rectRulerTop = drawRuler(
					ren,
					state.easel.s * c->w,
					rectDst.x,
					rectDst.y,
					state.easel.s,
					(state.easel.s < 8)
					? 1 : state.easel.s / 8,
					SIDE_TOP, SDL_FLIP_NONE
					);
			SDL_Rect rectRulerRight = drawRuler(
					ren,
					state.easel.s * c->h,
					rectDst.x + state.easel.s * c->w,
					rectDst.y,
					state.easel.s,
					(state.easel.s < 8)
					? 1 : state.easel.s / 8,
					SIDE_RIGHT, SDL_FLIP_NONE
					);
			SDL_SetRenderDrawColor(ren, 255, 255, 255, 95);
			SDL_RenderFillRect(ren, &rectRulerBottom);
			SDL_RenderFillRect(ren, &rectRulerLeft);
			SDL_RenderFillRect(ren, &rectRulerTop);
			SDL_RenderFillRect(ren, &rectRulerRight);
			if (!c->isSel) {
				SDL_SetRenderDrawColor(ren, 0, 0, 0, 95);
				SDL_RenderFillRect(ren, &rectRulerBottom);
				SDL_RenderFillRect(ren, &rectRulerLeft);
				SDL_RenderFillRect(ren, &rectRulerTop);
				SDL_RenderFillRect(ren, &rectRulerRight);
			}
		}
	}

	switch (state.drag.action) {
		case D_DRAWLINE:
			{
				struct canvas *preview = state.drag.drawLine.preview;
				int tw, th;
				SDL_QueryTexture(preview->tex, NULL, NULL, &tw, &th);
				SDL_Rect rectSrc = {
					0, 0,
					(preview->w >= tw) ? tw : preview->w,
					(preview->h >= th) ? th : preview->h
				};
				SDL_Rect rectDst = {
					TO_COORD_SCREEN_X(preview->x),
					TO_COORD_SCREEN_Y(preview->y),
					state.easel.s * rectSrc.w,
					state.easel.s * rectSrc.h
				};
				SDL_RenderCopy(ren, preview->tex, &rectSrc, &rectDst);
				break;
			}
		case D_DRAWRECT:
			{
				if (!state.blend)
					SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
				int x, y, w, h;
				if (state.drag.drawRect.currX
				 >= state.drag.drawRect.initX) {
					x = TO_COORD_SCREEN_X(
						state.drag.drawRect.initX
						);
					w = TO_COORD_SCREEN_X(
						state.drag.drawRect.currX + 1
						) - x;
				} else {
					x = TO_COORD_SCREEN_X(
						state.drag.drawRect.currX
						);
					w = TO_COORD_SCREEN_X(
						state.drag.drawRect.initX + 1
						) - x;
				}
				if (state.drag.drawRect.currY
				 >= state.drag.drawRect.initY) {
					y = TO_COORD_SCREEN_Y(
						state.drag.drawRect.initY
						);
					h = TO_COORD_SCREEN_Y(
						state.drag.drawRect.currY + 1
						) - y;
				} else {
					y = TO_COORD_SCREEN_Y(
						state.drag.drawRect.currY
						);
					h = TO_COORD_SCREEN_Y(
						state.drag.drawRect.initY + 1
						) - y;
				}
				SDL_Rect rectRect = {x, y, w, h};
				SDL_SetRenderDrawColor(
						ren,
						state.drag.drawRect.color.r,
						state.drag.drawRect.color.g,
						state.drag.drawRect.color.b,
						state.drag.drawRect.color.a
						);
				SDL_RenderFillRect(ren, &rectRect);
				SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
				break;
			}
		default:
			break;
	}

	int cursorX;
	int cursorY;
	if (state.drag.action == D_PANZOOM && state.space) {
		cursorX = state.drag.panZoom.initX;
		cursorY = state.drag.panZoom.initY;
	} else {
		SDL_GetMouseState(&cursorX, &cursorY);
	}
	SDL_Rect rectCursor = {
		TO_COORD_SCREEN_X(TO_COORD_EASEL_X(cursorX)),
		TO_COORD_SCREEN_Y(TO_COORD_EASEL_Y(cursorY)),
		state.easel.s,
		state.easel.s
	};
	SDL_SetRenderDrawColor(ren, 127, 127, 127, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(ren, &rectCursor);

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
				SDL_Rect rectTexture = {
					TO_COORD_SCREEN_X(c->x),
					TO_COORD_SCREEN_Y(c->y),
					state.easel.s * tw,
					state.easel.s * th
				};
				SDL_RenderDrawRect(ren, &rectTexture);
			}
		}

		if (state.drag.action == D_DRAWLINE) {
			struct canvas *c = state.drag.drawLine.preview;
			int tw, th;
			SDL_QueryTexture(c->tex, NULL, NULL, &tw, &th);
			SDL_Rect rectTexture = {
				TO_COORD_SCREEN_X(c->x),
				TO_COORD_SCREEN_Y(c->y),
				state.easel.s * tw,
				state.easel.s * th
			};
			SDL_RenderDrawRect(ren, &rectTexture);
		}

		SDL_SetRenderDrawColor(ren, 0, 255, 0, SDL_ALPHA_OPAQUE);

		SDL_Rect rectBounds = {
			TO_COORD_SCREEN_X(state.easel.minX),
			TO_COORD_SCREEN_Y(state.easel.minY),
			TO_COORD_SCREEN_X(state.easel.maxX)
				- TO_COORD_SCREEN_X(state.easel.minX),
			TO_COORD_SCREEN_Y(state.easel.maxY)
				- TO_COORD_SCREEN_Y(state.easel.minY)
		};
		SDL_RenderDrawRect(ren, &rectBounds);
	}

	SDL_Color colors[4] = {
		state.colors.a, state.colors.s, state.colors.d, state.colors.f
	};
	SDL_Rect rectColor = {0, 0, 32, 16};
	for (int i = 0; i < 4; ++i) {
		SDL_SetRenderDrawColor(
				ren,
				colors[i].r, colors[i].g,
				colors[i].b, colors[i].a
				);
		if (state.scope == S_PICK && 3 - i == state.modePick)
				rectColor.y += rectColor.h - 1;
		SDL_RenderFillRect(ren, &rectColor);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(ren, &rectColor);
		if (state.scope == S_PICK && 3 - i == state.modePick)
				rectColor.y -= rectColor.h - 1;
		rectColor.x += rectColor.w;
	}

	if (state.blend) {
		SDL_Rect rectBlend = {
			rectColor.x + rectColor.w / 4, rectColor.y + rectColor.h / 4,
			rectColor.w / 2, rectColor.h / 2
		};
		SDL_SetRenderDrawColor(ren, 127, 127, 127, SDL_ALPHA_OPAQUE);
		SDL_RenderFillRect(ren, &rectBlend);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(ren, &rectBlend);
	}

	if (state.scope == S_PICK) {
		SDL_Color color;
		switch (state.modePick) {
			case P_F:
				color = state.colors.f;
				break;
			case P_D:
				color = state.colors.d;
				break;
			case P_S:
				color = state.colors.s;
				break;
			case P_A:
				color = state.colors.a;
				break;
			default:
				break;
		}
		int channels[4] = {color.a, color.b, color.g, color.r};
		SDL_Color rulerColors[4] = {
			{255, 255, 255, 127},
			{  0,   0, 255, 127},
			{  0, 255,   0, 127},
			{255,   0,   0, 127}
		};
		for (int i = 0; i < 4; ++i) {
			SDL_Rect rectRuler = drawRuler(
					ren, 256,
					rectColor.w * i, rectColor.h * 2 + channels[i],
					1, 4, SIDE_RIGHT, SDL_FLIP_HORIZONTAL
					);
			SDL_SetRenderDrawColor(
					ren,
					rulerColors[i].r, rulerColors[i].g,
					rulerColors[i].b, rulerColors[i].a
					);
			SDL_RenderFillRect(ren, &rectRuler);
		}
		SDL_SetRenderDrawColor(ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawLine(ren, 0, rectColor.h * 2 + 255, 128, rectColor.h * 2 + 255);
	}

	SDL_RenderPresent(ren);

	if (!state.blend)
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

	flag = 1;
cleanup:
	return flag;
}

int eventDropFile(SDL_Event *e)
{
	int flag = 0;

	int y = TO_COORD_EASEL_Y(33);

	struct canvas *cFound;
checkX:
	cFound = canvasArrayFind(
			state.canvasSel,
			state.drop.x, y
			);
	if (cFound
	 && cFound->x == state.drop.x
	 && cFound->y == y) {
		state.drop.x += cFound->w + 1;
		goto checkX;
	}

	struct canvas *c = canvasNew(state.drop.x, y, 1, 1);
	canvasArrayAppend(state.canvasArr, c);
	canvasArrayAppend(state.canvasSel, c);
	c->isSel = 1;

	char *file = e->drop.file;
	int i;
	for (i = 0; file[i] != '\0'; ++i) {
		if (i == MAX_PATHLEN - 1) {
			canvasDel(c);
			goto cleanup;
		}
		c->path[i] = file[i];
	}
	c->path[i] = '\0';
	canvasLoad(c);
	SDL_free(file);

	state.drop.x += c->w + 1;

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

	if (e->key.keysym.mod & KMOD_CTRL) {
		switch (e->key.keysym.sym) {
			int mx, my;
			struct canvas *c;
			case SDLK_e:
				if (state.canvasSel->size != 0) {
					canvasArrayOpen(state.canvasSel);
				} else {
					SDL_GetMouseState(&mx, &my);
					int cursorX = TO_COORD_EASEL_X(mx);
					int cursorY = TO_COORD_EASEL_Y(my);
					c = canvasArrayFind(
							state.canvasArr,
							cursorX, cursorY
							);
					if (!c) {
						c = canvasNew(cursorX, cursorY, 1, 1);
						canvasArrayAppend(state.canvasArr, c);
					}
					canvasOpen(c);
				}
				break;
			case SDLK_s:
				if (state.canvasSel->size != 0) {
					MAP_CANVASES(state.canvasSel, i, c) {
						if (c->path[0] == '\0'
						 || e->key.keysym.mod & KMOD_SHIFT)
							canvasSaveAs(c);
						else
							canvasSave(c);
					}
				} else {
					SDL_GetMouseState(&mx, &my);
					c = canvasArrayFind(
							state.canvasArr,
							TO_COORD_EASEL_X(mx),
							TO_COORD_EASEL_Y(my)
							);
					if (c) {
						if (c->path[0] == '\0'
						 || e->key.keysym.mod & KMOD_SHIFT)
							canvasSaveAs(c);
						else
							canvasSave(c);
					}
				}
				break;
		}
		goto cleanupNoError;
	}

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
		case SDLK_x:
			setScope(S_PICK);
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
							if (!state.space) {
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
							} else {
								if (state.canvasSel->size != 0) {
									MAP_CANVASES(state.canvasSel, i, c) {
										canvasArrayRemove(state.canvasArr, c);
										canvasArrayAppend(state.canvasArr, c);
									}
								} else {
									int mx, my;
									SDL_GetMouseState(&mx, &my);
									struct canvas *c = canvasArrayFind(
											state.canvasArr,
											TO_COORD_EASEL_X(mx),
											TO_COORD_EASEL_Y(my)
											);
									canvasArrayRemove(state.canvasArr, c);
									canvasArrayAppend(state.canvasArr, c);
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
									canvasArrayRemove(state.canvasSel, c);
									c->isSel = 0;
								}
							}
							break;
						case SDLK_s:
							if (state.canvasSel->size != 0) {
								struct canvasArray *oldArray =
									canvasArrayCopy(state.canvasSel);
								MAP_CANVASES(oldArray, i, c) {
									canvasArrayRemove(state.canvasSel, c);
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
					setModeCanvas(C_RECT);
					goto cleanupNoError;
				case SDLK_q:
					setModeCanvas(C_FILL);
					goto cleanupNoError;
				default:
					break;
			}
			if (state.space) {
				switch (state.modeCanvas) {
					int mx, my;
					int cursorX, cursorY;
					struct canvas *c;
					SDL_Color *color;
					case C_PIXEL:
					case C_LINE:
					case C_RECT:
					case C_FILL:
						SDL_GetMouseState(&mx, &my);
						cursorX = TO_COORD_EASEL_X(mx);
						cursorY = TO_COORD_EASEL_Y(my);
						c = canvasArrayFind(
								state.canvasArr,
								cursorX, cursorY
								);
						switch (e->key.keysym.sym) {
							case SDLK_f:
								color = &state.colors.f;
								break;
							case SDLK_d:
								color = &state.colors.d;
								break;
							case SDLK_s:
								color = &state.colors.s;
								break;
							case SDLK_a:
								color = &state.colors.a;
								break;
							default:
								break;
						}
						canvasGetColor(
							c,
							cursorX - c->x,
							cursorY - c->y,
							color
							);
						goto cleanupNoError;
					default:
						break;
				}
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
				case C_RECT:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							resetDrag();
							state.drag.drawRect.key = KEY_F;
							state.drag.drawRect.color =
								state.colors.f;
							setDrag(D_DRAWRECT);
							break;
						case SDLK_d:
							resetDrag();
							state.drag.drawRect.key = KEY_D;
							state.drag.drawRect.color =
								state.colors.d;
							setDrag(D_DRAWRECT);
							break;
						case SDLK_s:
							resetDrag();
							state.drag.drawRect.key = KEY_S;
							state.drag.drawRect.color =
								state.colors.s;
							setDrag(D_DRAWRECT);
							break;
						case SDLK_a:
							resetDrag();
							state.drag.drawRect.key = KEY_A;
							state.drag.drawRect.color =
								state.colors.a;
							setDrag(D_DRAWRECT);
							break;
						default:
							break;
					}
					break;
				case C_FILL:
					switch (e->key.keysym.sym) {
						int mx, my;
						int cursorX, cursorY;
						struct canvasArray *ca;
						struct canvas *c;
						struct pixelMask *pm;
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
							cursorX = TO_COORD_EASEL_X(mx);
							cursorY = TO_COORD_EASEL_Y(my);
							ca = (state.canvasSel->size == 0)
								? state.canvasArr
								: state.canvasSel;
							c = canvasArrayFind(ca, cursorX, cursorY);
							if (!c)
								goto cleanupNoError;
							ca = canvasArrayNew();
							canvasArrayAppend(ca, c);
							pm = pixelMaskNew(c->x, c->y, c->w, c->h, 0);
							pixelMaskFill(pm, c, cursorX, cursorY);
							pixelMaskDo(pm, ca, color);
							pixelMaskFree(pm);
							canvasArrayFree(ca);
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			break;
		case S_PICK:
			switch (e->key.keysym.sym) {
				enum ModePick mode;
				SDL_Color tmp;
				SDL_Color *swap;
				case SDLK_t:
					if (!state.space) {
						state.blend = 1;
						SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
					} else {
						state.blend = 0;
						SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
					}
					break;
				case SDLK_r:
					mode = P_F;
					if (!state.space)
						goto S_PICK_rewq;
					tmp = state.colors.f;
					swap = &state.colors.f;
					goto S_PICK_rewq_space;
				case SDLK_e:
					mode = P_D;
					if (!state.space)
						goto S_PICK_rewq;
					tmp = state.colors.d;
					swap = &state.colors.d;
					goto S_PICK_rewq_space;
				case SDLK_w:
					mode = P_S;
					if (!state.space)
						goto S_PICK_rewq;
					tmp = state.colors.s;
					swap = &state.colors.s;
					goto S_PICK_rewq_space;
				case SDLK_q:
					mode = P_A;
					if (!state.space)
						goto S_PICK_rewq;
					tmp = state.colors.a;
					swap = &state.colors.a;
					goto S_PICK_rewq_space;
S_PICK_rewq_space:
					switch (state.modePick) {
						case P_F:
							*swap = state.colors.f;
							state.colors.f = tmp;
							break;
						case P_D:
							*swap = state.colors.d;
							state.colors.d = tmp;
							break;
						case P_S:
							*swap = state.colors.s;
							state.colors.s = tmp;
							break;
						case P_A:
							*swap = state.colors.a;
							state.colors.a = tmp;
							break;
						default:
							break;
					}
S_PICK_rewq:
					setModePick(mode);
					break;
				case SDLK_f:
					state.drag.pick.pickRed = 1;
					if (state.drag.action != D_PICK)
						setDrag(D_PICK);
					break;
				case SDLK_d:
					state.drag.pick.pickGreen = 1;
					if (state.drag.action != D_PICK)
						setDrag(D_PICK);
					break;
				case SDLK_s:
					state.drag.pick.pickBlue = 1;
					if (state.drag.action != D_PICK)
						setDrag(D_PICK);
					break;
				case SDLK_a:
					state.drag.pick.pickAlpha = 1;
					if (state.drag.action != D_PICK)
						setDrag(D_PICK);
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
				case C_RECT:
					switch (e->key.keysym.sym) {
						case SDLK_f:
							if (state.drag.drawRect.key == KEY_F)
								setDrag(D_NONE);
							break;
						case SDLK_d:
							if (state.drag.drawRect.key == KEY_D)
								setDrag(D_NONE);
							break;
						case SDLK_s:
							if (state.drag.drawRect.key == KEY_S)
								setDrag(D_NONE);
							break;
						case SDLK_a:
							if (state.drag.drawRect.key == KEY_A)
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
		case S_PICK:
			switch (e->key.keysym.sym) {
				case SDLK_f:
					state.drag.pick.pickRed = 0;
					if (state.drag.pick.pickGreen == 0
					 && state.drag.pick.pickBlue == 0
					 && state.drag.pick.pickAlpha == 0)
						setDrag(D_NONE);
					break;
				case SDLK_d:
					state.drag.pick.pickGreen = 0;
					if (state.drag.pick.pickRed == 0
					 && state.drag.pick.pickBlue == 0
					 && state.drag.pick.pickAlpha == 0)
						setDrag(D_NONE);
					break;
				case SDLK_s:
					state.drag.pick.pickBlue = 0;
					if (state.drag.pick.pickRed == 0
					 && state.drag.pick.pickGreen == 0
					 && state.drag.pick.pickAlpha == 0)
						setDrag(D_NONE);
					break;
				case SDLK_a:
					state.drag.pick.pickAlpha = 0;
					if (state.drag.pick.pickRed == 0
					 && state.drag.pick.pickGreen == 0
					 && state.drag.pick.pickBlue == 0)
						setDrag(D_NONE);
					break;
				default:
					break;
			}
			break;
	}

	flag = 1;
cleanup:
	return flag;
}

int eventMouseMotion(SDL_Event *e)
{
	int flag = 0;

	if (
		TO_COORD_EASEL_X(e->motion.x) !=
		TO_COORD_EASEL_X(e->motion.x - e->motion.xrel)
		|| TO_COORD_EASEL_Y(e->motion.y) !=
		   TO_COORD_EASEL_Y(e->motion.y - e->motion.yrel)
	   ) {
		cursorMotion(
				TO_COORD_EASEL_X(e->motion.x),
				TO_COORD_EASEL_Y(e->motion.y)
			    );
	}

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
						int quota = ceil(48.0 / state.easel.s);
						while (accum >= quota && state.easel.s < MAX_ZOOM) {
							accum -= quota;
							++state.easel.s;
							quota = ceil(48.0 / state.easel.s);
						}
					} else if (state.easel.s > 1) {
						int quota = ceil(48.0 /(state.easel.s - 1));
						while (-accum >= quota && state.easel.s > 1) {
							accum += quota;
							--state.easel.s;
							quota = ceil(48.0 /(state.easel.s - 1));
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
		case D_PICK:
			switch (state.modePick) {
				SDL_Color *color;
				case P_F:
					color = &state.colors.f;
					goto D_PICK_fdsa;
				case P_D:
					color = &state.colors.d;
					goto D_PICK_fdsa;
				case P_S:
					color = &state.colors.s;
					goto D_PICK_fdsa;
				case P_A:
					color = &state.colors.a;
					goto D_PICK_fdsa;
D_PICK_fdsa:
					if (state.drag.pick.pickRed) {
						int newVal = color->r + e->motion.yrel;
						if (newVal < 0)
							color->r = 0;
						else if (newVal > 255)
							color->r = 255;
						else
							color->r = newVal;
					}
					if (state.drag.pick.pickGreen) {
						int newVal = color->g + e->motion.yrel;
						if (newVal < 0)
							color->g = 0;
						else if (newVal > 255)
							color->g = 255;
						else
							color->g = newVal;
					}
					if (state.drag.pick.pickBlue) {
						int newVal = color->b + e->motion.yrel;
						if (newVal < 0)
							color->b = 0;
						else if (newVal > 255)
							color->b = 255;
						else
							color->b = newVal;
					}
					if (state.drag.pick.pickAlpha) {
						int newVal = color->a + e->motion.yrel;
						if (newVal < 0)
							color->a = 0;
						else if (newVal > 255)
							color->a = 255;
						else
							color->a = newVal;
					}
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

int cursorMotion(int cursorX, int cursorY)
{
	int flag = 0;

	switch (state.drag.action) {
		case D_CANVASNEW:
			{
				MAP_CANVASES(state.canvasSel, i, c) {
					if (!c->isSel) {
						canvasMove(
							c, c->x, c->y,
							cursorX - c->x,
							cursorY - c->y
							);
					}
				}
				break;
			}
		case D_CANVASTRANSFORM:
			{
				MAP_CANVASES(state.canvasSel, i, c) {
					int x = state.drag.canvasTransform.setX
						? cursorX
						+ state.drag.canvasTransform.offX[i]
						: c->x;
					int y = state.drag.canvasTransform.setY
						? cursorY
						+ state.drag.canvasTransform.offY[i]
						: c->y;
					int w = state.drag.canvasTransform.setW
						? cursorX - c->x
						+ state.drag.canvasTransform.offW[i]
						: c->w;
					int h = state.drag.canvasTransform.setH
						? cursorY - c->y
						+ state.drag.canvasTransform.offH[i]
						: c->h;
					canvasMove(c, x, y, w, h);
				}
				break;
			}
		case D_DRAWPIXEL:
			pixelArrayReset(state.drag.drawPixel.pixels);
			pixelArrayLine(
				state.drag.drawPixel.pixels,
				state.drag.drawPixel.initX,
				state.drag.drawPixel.initY,
				cursorX, cursorY, 1
				);
			pixelArrayDo(
				state.drag.drawPixel.pixels,
				(state.canvasSel->size == 0)
				? state.canvasArr : state.canvasSel,
				state.drag.drawPixel.color, 1
				);
			state.drag.drawPixel.initX = cursorX;
			state.drag.drawPixel.initY = cursorY;
			break;
		case D_DRAWLINE:
			state.drag.drawLine.currX = cursorX;
			state.drag.drawLine.currY = cursorY;
			canvasClear(state.drag.drawLine.preview);
			if (cursorX >= state.drag.drawLine.initX) {
				if (cursorY >= state.drag.drawLine.initY) {
					canvasMove(
						state.drag.drawLine.preview,
						state.drag.drawLine.initX,
						state.drag.drawLine.initY,
						cursorX - state.drag.drawLine.initX + 1,
						cursorY - state.drag.drawLine.initY + 1
						);
				} else {
					canvasMove(
						state.drag.drawLine.preview,
						state.drag.drawLine.initX,
						cursorY,
						cursorX - state.drag.drawLine.initX + 1,
						state.drag.drawLine.initY - cursorY + 1
						);
				}
			} else {
				if (cursorY >= state.drag.drawLine.initY) {
					canvasMove(
						state.drag.drawLine.preview,
						cursorX,
						state.drag.drawLine.initY,
						state.drag.drawLine.initX - cursorX + 1,
						cursorY - state.drag.drawLine.initY + 1
						);
				} else {
					canvasMove(
						state.drag.drawLine.preview,
						cursorX,
						cursorY,
						state.drag.drawLine.initX - cursorX + 1,
						state.drag.drawLine.initY - cursorY + 1
						);
				}
			}
			canvasFix(state.drag.drawLine.preview);
			pixelArrayReset(state.drag.drawLine.pixels);
			pixelArrayLine(
				state.drag.drawLine.pixels,
				state.drag.drawLine.initX,
				state.drag.drawLine.initY,
				cursorX, cursorY, 0
				);
			pixelArrayDo(
				state.drag.drawLine.pixels,
				state.drag.drawLine.previewArr,
				state.drag.drawLine.color, 0
				);
			break;
		case D_DRAWRECT:
			state.drag.drawRect.currX = cursorX;
			state.drag.drawRect.currY = cursorY;
			break;
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
				if (!eventDropFile(e))
					goto cleanup;
				break;
			case SDL_DROPBEGIN:
				state.drop.x = TO_COORD_EASEL_X(0);
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
