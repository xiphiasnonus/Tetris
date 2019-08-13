//
//  TETRIS
//  a clone by Thomas Foster
//
//  foster.pianist@gmail.com
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>

#include "tetramino.h"

#define DRAW_SCALE      3
#define WINDOW_W        224
#define WINDOW_H        176

#define FONT_W          8
#define FONT_H          8
#define BOARD_W         10
#define BOARD_H         21 // visible area is 20 tall
#define TILE_SIZE       8
#define MS_PER_FRAME    16

#define INITIAL_LVL     0
#define LINES_PER_LVL   10
#define INITIAL_CYCLE   60
#define CYCLE_DECR      5   // cycle time 5 less each level
#define SLIDE_TIME      30

enum
{
    GS_TITLE,
    GS_PLAY,
    GS_GAMEOVER
} gamestate;

enum
{
    PS_DROP,
    PS_SLIDE,
    PS_LINEFADE,
} playstate;

typedef enum
{
    COLLISION_NONE,
    COLLISION_SIDE,
    COLLISION_BOTTOM,
    COLLISION_BLOCK
} collision_t;

SDL_Window *    window;
SDL_Renderer *  renderer;
SDL_Texture *   font;
int             rows, cols; // console size
int             csrx, csry; // cursor location

bool            spawn;
tetramino_t     tet; // player-controlled tetramino
tettype_t       nexttet;
int             board[BOARD_H][BOARD_W]; // -1 unoccupied, >= 0 tettype_t
bool            completed[BOARD_H]; // list of completed lines

int             score;
int             level;
int             numlines;
int             stats[TET_COUNT];

int             cyclelength;
int             cycletimer; // i.e. game speed, in frames
int             fadetimer;
int             slidetimer;

bool            slide;


//====================
//  OPTIONS
//====================

enum
{
    OPT_PAUSED,
    OPT_SHOWGUIDE,
    OPT_SOUND,
    NUMOPTIONS
};

bool options[NUMOPTIONS];

void ToggleOption (int i)
{
    options[i] = !options[i];
}


//====================
//  PANELS
//====================

enum {
    PANEL_NEXT,
    PANEL_LEVEL,
    PANEL_SCORE,
    PANEL_LINES,
    PANEL_STATS,
    PANEL_COUNT
};

typedef struct
{
    SDL_Rect    rect;
    char        name[8]; // display name that gets 'prints'd
    int *       data;
} panel_t;

panel_t panels[PANEL_COUNT];


//====================
//  RANDOM NUMBER GENERATOR
//====================

unsigned char rndtable[256] = {
    0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66 ,
    74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36 ,
    95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188 ,
    52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224 ,
    149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242 ,
    145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0 ,
    175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235 ,
    25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113 ,
    94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75 ,
    136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196 ,
    135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113 ,
    80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241 ,
    24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224 ,
    145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95 ,
    28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226 ,
    71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36 ,
    17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106 ,
    197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136 ,
    120, 163, 236, 249
};

int rndindex;

int Random (void)
{
    rndindex = (rndindex + 1) & 0xff;
    return rndtable[rndindex];
}



//====================
//  SOUND
//====================

typedef enum {
    SND_MOVE,
    SND_LINE,
    SND_TETRIS,
    SND_ROTATE,
    SND_LEVELUP,
    SND_DROP,
    
    NUMSOUNDS
} sount_t;

Mix_Chunk * sounds[NUMSOUNDS];

void PlaySound (sount_t s)
{
    if (options[OPT_SOUND])
        Mix_PlayChannel(1, sounds[s], 0);
}

void InitSounds (void)
{
    sounds[ SND_MOVE ]      = Mix_LoadWAV( "assets/deet.wav" );
    sounds[ SND_LINE ]      = Mix_LoadWAV( "assets/line.wav" );
    sounds[ SND_TETRIS ]    = Mix_LoadWAV( "assets/tetris.wav" );
    sounds[ SND_ROTATE ]    = Mix_LoadWAV( "assets/rotate.wav" );
    sounds[ SND_LEVELUP ]   = Mix_LoadWAV( "assets/level.wav" );
    sounds[ SND_DROP ]      = Mix_LoadWAV( "assets/drop.wav" );
}


//====================
//  COLOR
//====================

enum
{
    CGA_BLACK,
    CGA_BLUE,
    CGA_GREEN,
    CGA_CYAN,
    CGA_RED,
    CGA_MAGENTA,
    CGA_BROWN,
    CGA_WHITE,
    CGA_GRAY,
    CGA_BRIGHTBLUE,
    CGA_BRIGHTGREEN,
    CGA_BRIGHTCYAN,
    CGA_BRIGHTRED,
    CGA_BRIGHTMAGENTA,
    CGA_YELLOW,
    CGA_BRIGHTWHITE,
    CGA_NUMCOLORS
};

SDL_Color
colors[CGA_NUMCOLORS] = {
    {   0,   0,   0 },     // 0  BLACK
    {   0,   0, 170 },     // 1  BLUE
    {   0, 170,   0 },     // 2  GREEN
    {   0, 170, 170 },     // 3  CYAN
    { 170,   0,   0 },     // 4  RED
    { 170,   0, 170 },     // 5  MAGENTA
    { 170,  85,   0 },     // 6  BROWN
    { 170, 170, 170 },     // 7  WHITE
    {  85,  85,  85 },     // 8  GRAY
    {  85,  85, 255 },     // 9  BRIGHTBLUE
    {  85, 255,  85 },     // 10 BRIGHTGREEN
    {  85, 255, 255 },     // 11 BRIGHTCYAN
    { 255,  85,  85 },     // 12 BRIGHTRED
    { 255,  85, 255 },     // 13 BRIGHTMAGENTA
    { 255, 255,  85 },     // 14 YELLOW
    { 255, 255, 255 },     // 15 BRIGHTWHITE
};

int fgcolors[TET_COUNT] =
{
    CGA_BRIGHTWHITE,    // TET_O,
    CGA_BROWN,          // TET_I,
    CGA_GREEN,          // TET_L,
    CGA_BRIGHTRED,      // TET_J,
    CGA_MAGENTA,        // TET_S,
    CGA_GRAY,           // TET_Z,
    CGA_RED             // TET_T,
};

int bdcolors[TET_COUNT] =
{
    CGA_CYAN,
    CGA_YELLOW,
    CGA_BRIGHTGREEN,
    CGA_BRIGHTGREEN,
    CGA_BRIGHTBLUE,
    CGA_BRIGHTRED,
    CGA_BRIGHTMAGENTA
};

void SetColor (SDL_Color * c)
{
    SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, 255);
}




void Quit (const char * error)
{
    int i;
    
    SDL_DestroyTexture(font);
    for (i=0 ; i<NUMSOUNDS ; i++)
        Mix_FreeChunk(sounds[i]);
    Mix_CloseAudio();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    if (error && *error) {
        printf("Error! %s: %s\n", error, SDL_GetError());
        exit(1);
    }
    exit(0);
}


void
InitPanel
( panel_t * p,
  int x, int y, int w, int h, // rect, in tiles
  const char * name,
  int * data )
{
    p->rect.x = x * TILE_SIZE;
    p->rect.y = y * TILE_SIZE;
    p->rect.w = w * TILE_SIZE;
    p->rect.h = h * TILE_SIZE;
    if (name)
        strncpy(p->name, name, 8);
    else
        p->name[0] = '\0';
    p->data = data;
}



//
//  Initialize
//  Init SDL2, window, renderer, sound, console, and panels
//
void Initialize (void)
{
    SDL_Surface * s;
    int w = WINDOW_W * DRAW_SCALE;
    int h = WINDOW_H * DRAW_SCALE;
    int x1 ,y1, x2, y2;
    
    rndindex = time(NULL) % 256;
    
    // init window
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) != 0)
        Quit("main: Error! SDL_Init failed");
    
    window = SDL_CreateWindow("Tetris", 0, 0, w, h, 0);
    if (!window)
        Quit("main: Error! SDL_CreateWindow failed");
    
    // init renderer
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer)
        Quit("main: Error! SDL_CreateRenderer failed");
    SDL_RenderSetScale(renderer, DRAW_SCALE, DRAW_SCALE);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // init sound
    Mix_OpenAudio(MIX_DEFAULT_FREQUENCY,MIX_DEFAULT_FORMAT,MIX_DEFAULT_CHANNELS,512);
    Mix_Volume(-1, 8);
    InitSounds();
    
    
    // init console/font
    s = IMG_Load("assets/cgafont.png");
    if (!s)
        Quit("main: Error! Could not load cgafont");
    
    font = SDL_CreateTextureFromSurface(renderer, s);
    if (!font)
        Quit("main: Error! Could not create font texture");
    SDL_FreeSurface(s);
    
    rows = WINDOW_H / FONT_H;
    cols = WINDOW_W / FONT_W;
    
    InitPanel(&panels[PANEL_NEXT], 1, 1, 7, 6, "NEXT", NULL);
    InitPanel(&panels[PANEL_LEVEL], 1, 9, 7, 5, "LEVEL", &level);
    InitPanel(&panels[PANEL_SCORE], 1, 16, 7, 5, "SCORE", &score);
    InitPanel(&panels[PANEL_LINES], 20, 1, 7, 5, "LINES", &numlines);
    InitPanel(&panels[PANEL_STATS], 20, 10, 7, 11, "STATS", NULL);
    
    InitDropGuides();
    
    for (x1=0 ; x1<3 ; x1++) {
        for (y1=0 ; y1<3 ; y1++) {
            for (x2=0 ; x2<3 ; x2++) {
                for (y2=0 ; y2<3 ; y2++)
                {
                    
                }
            }
        }
    }
    
    options[OPT_SOUND] = true;
    options[OPT_SHOWGUIDE] = true;
    options[OPT_PAUSED] = false;
}




#pragma mark - Console Functions

void gotoxy (int x, int y)
{
    if (x < 0 || x >= cols || y < 0 || y > rows)
        Quit("gotoxy: Error! value out of bounds");

    csrx = x;
    csry = y;
}

void printc (int c)
{
    SDL_Rect src = { (c%32)*FONT_W, c/32*FONT_H, FONT_W, FONT_H };
    SDL_Rect dst = { csrx*FONT_W, csry*FONT_H, FONT_W, FONT_H };
    SDL_RenderCopy(renderer, font, &src, &dst);
}

void prints (const char *string)
{
    char *c = (char *)string;
    while (*c != '\0')
    {
        if (*c == '\n' && csry != rows - 1) {
            csry++;
            csrx = 0;
        } else {
            printc(*c);
            csrx++;
        }
        c++;
    }
}

void printd (int d)
{
    char buffer[80];
    sprintf(buffer, "%d", d);
    prints(buffer);
}




#pragma mark - GAME

void InitGame (void)
{
    memset(board, -1, sizeof(board));
    score = 0;
    level = INITIAL_LVL;
    numlines = 0;
    cyclelength = INITIAL_CYCLE;
    cycletimer = cyclelength;
    memset(completed, 0, sizeof(completed));
    spawn = true;
    nexttet = Random() % TET_COUNT;
    playstate = PS_DROP;
}



//
// Collision
// Check for collision between 'tet' (player's piece)
// and the side, bottom, or blocks already on the board
//
collision_t Collision (int checkx, int checky)
{
    int x, y;
    char * block; // check each block within 'tet'
    
    for (y=0 ; y<DATA_SIZE ; y++)
        for (x=0 ; x<DATA_SIZE ; x++)
        {
            block = &shapes[tet.type][tet.rotation][y][x];
            
            if (*block && (x + checkx < 0 || x + checkx >= BOARD_W))
                return COLLISION_SIDE;
            if (*block && y + checky >= BOARD_H)
                return COLLISION_BOTTOM;
            if (*block && board[y + checky][x + checkx] >= 0)
                return COLLISION_BLOCK;
        }
    
    return COLLISION_NONE;
}




void SpawnTetramino (void)
{
    memset(&tet, 0, sizeof(tetramino_t));
    
    tet.type = nexttet;
    nexttet = Random() % TET_COUNT;
    tet.x = 3;
    tet.y = tet.type == TET_O ? 1 : 0;
    tet.rotation = 0;
    
    if (Collision(tet.x, tet.y))
        gamestate = GS_GAMEOVER;
}




//
// MoveTetramino
// - Try to move the player-controlled tet
// does not move and returns false
// if the move results in a collision
//
bool MoveTetramino (int dx, int dy)
{
    if (!Collision(tet.x + dx, tet.y + dy)) {
        tet.x += dx;
        tet.y += dy;
        return true;
    }
    return false;
}



void RotateTetramino (void)
{
    tet.rotation = (tet.rotation + 1) % R_COUNT;
    
    if (Collision(tet.x, tet.y)) { // if collision, rotate it back
        tet.rotation = (tet.rotation - 1) % R_COUNT;
        return;
    }
    PlaySound(SND_ROTATE);
}


bool TilePresent (int x, int y)
{
    return shapes[tet.type][tet.rotation][y][x];
}


void UpdateGame (void)
{
    int     x, y, y1;
    int     linecnt; // num lines cleared
    
    if (spawn) {
        SpawnTetramino();
        spawn = false;
    }
    
    linecnt = 0;
    if (fadetimer) { // don't process anything while line(s) fading
        --fadetimer;
        return;
    } else { // remove completed lines
        for (y=0 ; y<BOARD_H ; y++)
        {
            if (!completed[y])
                continue;
            
            linecnt++;
            score += 25;
            numlines++;
            for (y1=y; y1>0; y1--) // move all higher rows down one
                for (x=0; x<BOARD_W; x++)
                    board[y1][x] = board[y1-1][x];
            completed[y] = false;
        }
    }
    if (linecnt == 4)
        PlaySound(SND_TETRIS);
    else if (linecnt)
        PlaySound(SND_LINE);
    
    // check for next level
    if (numlines / LINES_PER_LVL > level ) {
        level++;
        cyclelength -= CYCLE_DECR;
        if (cyclelength < CYCLE_DECR)
            cyclelength = CYCLE_DECR;
        PlaySound(SND_LEVELUP);
    }
    
    if (cycletimer) {
        --cycletimer;
        return;
    }
    cycletimer = cyclelength;
    
    // give a small amount of time to slide the piece
    if (Collision(tet.x, tet.y + 1) && cycletimer < SLIDE_TIME && !slide) {
        slide = true;
        cycletimer = SLIDE_TIME;
        return;
    }
    slide = false;
    
    // move down
    if (!MoveTetramino(0, 1))
    {
        for (y=0 ; y<DATA_SIZE ; y++) {
            for (x=0 ; x<DATA_SIZE ; x++)
            {
                if (TilePresent(x, y))
                    board[y+tet.y][x+tet.x] = tet.type;
            }
        }
        stats[tet.type] = (stats[tet.type] + 1) % 999;
        cycletimer = 0;
        spawn = true;
    }
    

    // check for complete rows
    bool gotline;
    for (y=0 ; y<BOARD_H ; y++)
    {
        gotline = true;
        for (x=0 ; x<BOARD_W ; x++)
        {
            if (board[y][x] == -1) {
                gotline = false;
                break;
            }
        }
        if (gotline) {
            completed[y] = true;
            fadetimer += 15; // clearing more lines takes longer
        }
    }
}


#pragma mark -

void DrawTile (int x, int y, tettype_t type)
{
    SDL_Rect    bdrect, fgrect;
    SDL_Color   *bd, *fg;
    
    x *= TILE_SIZE; // convert to screen coords
    y *= TILE_SIZE;
    
    bdrect = (SDL_Rect){ x, y, TILE_SIZE, TILE_SIZE };
    fgrect = (SDL_Rect){ x + 1, y + 1, TILE_SIZE - 2, TILE_SIZE - 2 };
    bd = &colors[bdcolors[type]];
    fg = &colors[fgcolors[type]];
    SetColor(bd);
    SDL_RenderFillRect(renderer, &bdrect);
    SetColor(fg);
    SDL_RenderFillRect(renderer, &fgrect);
}



void DrawBackground (void)
{
    SDL_Rect r = { 1, 1, TILE_SIZE - 2, TILE_SIZE - 2};
    
    SDL_SetRenderDrawColor(renderer, 16, 16, 16, 255);
    for (r.y=1; r.y<WINDOW_H ; r.y+=TILE_SIZE)
        for (r.x=1; r.x<WINDOW_W ; r.x+=TILE_SIZE)
        {
            SDL_RenderFillRect(renderer, &r);
        }
}


void DrawPanels (void)
{
    panel_t * p;
    int i;
    int x, y;
    
    for (i=0 ; i<PANEL_COUNT ; i++)
    {
        p = &panels[i];
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &p->rect);
        SDL_RenderSetViewport(renderer, &p->rect);
        gotoxy(1, 1); prints(p->name);
        if (p->data) {
            gotoxy(1, 3); printd(*p->data);
        }
        if (i == PANEL_NEXT) {
            for (y=0 ; y<DATA_SIZE ; y++) {
                for (x=0 ; x<DATA_SIZE ; x++) {
                    if (displayshapes[nexttet][y][x])
                        DrawTile(x+1, y+3, nexttet);
                }
            }
        }
        if (i == PANEL_STATS) {
            for (i=0 ; i<TET_COUNT ; i++) {
                DrawTile(1, i+3, i);
                gotoxy(3, i+3);
                printd(stats[i]);
            }
        }
        SDL_RenderSetViewport(renderer, NULL);
    }
}


void DrawDropGuide (void)
{
    int         x;
    int         alpha;
    SDL_Rect    beam;
    SDL_Rect    blend;

    beam.w = TILE_SIZE;
    blend.w = TILE_SIZE;
    blend.h = 1;
    for (x=0 ; x<DATA_SIZE ; x++)
    {
        if (guidedata[tet.type][tet.rotation][x] != -1) {
            beam.x = (x + tet.x) * TILE_SIZE;
            beam.y = (guidedata[tet.type][tet.rotation][x] + tet.y) * TILE_SIZE;
            beam.h = BOARD_H * TILE_SIZE - beam.y;
            SDL_SetRenderDrawColor(renderer, 24, 24, 24, 255);
            SDL_RenderFillRect(renderer, &beam);
            blend.x = beam.x;
            blend.y = beam.y;
            alpha = 0;
            while (blend.y < BOARD_H * TILE_SIZE) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
                SDL_RenderFillRect(renderer, &blend);
                blend.y++;
                alpha += 1;
                if (alpha > 255)
                    alpha = 255;
            }
        }
    }
}


void DrawBoard (void)
{
    int x, y;
    
    // visible area
    SDL_Rect boardrect = {
        9*TILE_SIZE, 1*TILE_SIZE, BOARD_W*TILE_SIZE, (BOARD_H-1)*TILE_SIZE
    };

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &boardrect);

    boardrect.y = 0;
    boardrect.h = BOARD_H*TILE_SIZE;
    SDL_RenderSetViewport(renderer, &boardrect);
    
    if (options[OPT_SHOWGUIDE]) {
        DrawDropGuide();
    }
    
    // player tetramino
    for (y=0 ; y<DATA_SIZE ; y++) {
        for (x=0 ; x<DATA_SIZE ; x++)
        {
            if (shapes[tet.type][tet.rotation][y][x])
                DrawTile(x+tet.x, y+tet.y, tet.type);
        }
    }
    
    // landed pieces
    for (y=0 ; y<BOARD_H ; y++)
        for (x=0 ; x<BOARD_W ; x++)
        {
            if (fadetimer && completed[y]) {
                DrawTile(x, y, Random() % TET_COUNT);
            } else if (board[y][x] != -1) {
                DrawTile(x, y, board[y][x]);
            }
        }
    
    SDL_RenderSetViewport(renderer, NULL);
}



#pragma mark -

void IncognitoMode (void)
{
    SDL_Event event;
    char c;
    
    SDL_RenderSetScale(renderer, 2, 2);
    
    while (1)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                Quit(NULL);
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    SDL_RenderSetScale(renderer, DRAW_SCALE, DRAW_SCALE);
                    return;
                } else if (event.key.keysym.sym == SDLK_q) {
                    Quit(NULL);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        gotoxy(0, 0);
        prints("The IBM Personal Computer DOS\n");
        prints("Version 2.10 (C)Copyright IBM Corp 1981,\n");
        prints(" 1982, 1983\n\n");
        prints("A>");
        c = (SDL_GetTicks() % 256) < 128 ? '_' : ' ';
        printc(c);
        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }
}



void SetWindowPosition (SDL_Keycode key)
{
    int x, y;
    SDL_Rect screen;

    SDL_GetDisplayBounds(0, &screen);

    x = key==SDLK_u || key==SDLK_j ? 0 : screen.w - WINDOW_W * DRAW_SCALE;
    y = key==SDLK_u || key==SDLK_i ? 0 : screen.h - WINDOW_H * DRAW_SCALE;
    SDL_SetWindowPosition(window, x, y);
}




void DoKeyDown (SDL_Keycode key)
{
    // general input
    switch (key)
    {
        case SDLK_ESCAPE:   IncognitoMode();                break;
        case SDLK_q:        Quit(NULL);                     break;
        case SDLK_p:        ToggleOption(OPT_PAUSED);       break;
        case SDLK_g:        ToggleOption(OPT_SHOWGUIDE);    break;
        case SDLK_s:        ToggleOption(OPT_SOUND);        break;
            
        case SDLK_c:
            SDL_SetWindowPosition(window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
            break;
            
        case SDLK_u: case SDLK_i: case SDLK_j: case SDLK_k:
            SetWindowPosition(key);
            break;

        default:
            break;
    }
    
    // game input

    if (options[OPT_PAUSED])
        return;
    
    switch (key)
    {
        case SDLK_SPACE:
        case SDLK_UP:
            Random();
            RotateTetramino();
            break;
            
        case SDLK_DOWN:
            Random();
            while (MoveTetramino(0, 1));
            score += 5;
            cycletimer = 0;
            PlaySound(SND_DROP);
            break;
            
        case SDLK_LEFT:
            Random();
            if (MoveTetramino(-1, 0))
                PlaySound(SND_MOVE);
            break;
            
        case SDLK_RIGHT:
            Random();
            if (MoveTetramino(1, 0))
                PlaySound(SND_MOVE);
            break;
            
            // debug:
        case SDLK_EQUALS:
            numlines += LINES_PER_LVL;
            break;
        case SDLK_MINUS:
            numlines -= LINES_PER_LVL;
            if (numlines < 0) numlines = 0;
            break;
            
        default:
            break;
    }
}


void PlayLoop (void)
{
    const SDL_Rect pausedrect = {
        WINDOW_W/2-4*TILE_SIZE,
        WINDOW_H/2-TILE_SIZE*2,
        8*TILE_SIZE,
        3*TILE_SIZE
    };
    
    SDL_Event   event;
    int         tics;
    int         elapsed;
    int         starttime;

    InitGame();
    tics = 0;
    
    do
    {
        starttime = SDL_GetTicks();
        
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                Quit(NULL);
            if (event.type == SDL_KEYDOWN)
                DoKeyDown(event.key.keysym.sym);
        }
        
        if (!options[OPT_PAUSED]) {
            UpdateGame();
        }
        
        SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
        SDL_RenderClear(renderer);
        DrawBackground();
        DrawPanels();
        DrawBoard();
        if (options[OPT_PAUSED])
        {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &pausedrect);
            if (SDL_GetTicks() % 600 < 300) {
                const char * msg = "PAUSED";
                gotoxy(((WINDOW_W/TILE_SIZE)-(int)strlen(msg))/2, WINDOW_H/2/TILE_SIZE-1);
                prints(msg);
            }
        }
        SDL_RenderPresent(renderer);
        
        
        elapsed = SDL_GetTicks() - starttime;
        if (elapsed < MS_PER_FRAME)
            SDL_Delay(MS_PER_FRAME - elapsed);
        tics++;
    } while (gamestate == GS_PLAY);
}




int main (int argc, const char * argv[])
{
    Initialize();
    
    gamestate = GS_PLAY;
    
    while (1)
    {
        switch (gamestate) {
            case GS_TITLE:
                break;
            case GS_PLAY:
                PlayLoop();
                break;
                
            default:
                break;
        }
    }
    
    return 0;
}
