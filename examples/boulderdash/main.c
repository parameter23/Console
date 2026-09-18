/**
 * @file main.c
 * @brief A port of a Windows Boulder Dash clone (github.com/kristerw/
 *        boulder-dash, ZX Spectrum 1984 rules/cave data) onto the generic
 *        GameAPI. The original's rendering already funneled through one
 *        setPixel(x,y,Color) call with a 9-color palette and 1bpp 8x8
 *        sprite/tile data (fg/bg color pair) - both map directly onto
 *        this engine's fb8_set_pixel()/16-color base palette, so the
 *        cave-physics simulation (falling boulders/diamonds, amoeba
 *        growth, fireflies/butterflies, explosions, magic wall) ported
 *        with no changes at all; only the Win32 window/message-pump/GDI
 *        layer and WASAPI sound were replaced, with this file's
 *        GameAPI init/update/draw callbacks taking over pacing (see
 *        boulder_update()/boulder_draw() below) instead of the
 *        original's manual dt-accumulator main loop.
 *
 * data_caves.h/data_sprites.h are copied unchanged from the original -
 * pure portable data (the real 1984 cave layouts A-P + 4 intermissions,
 * and 1bpp sprite/tile bitmaps).
 *
 * Controls: arrows move Rockford, BTN digs/pushes in the faced direction
 * (holding it while moving drops what would otherwise be picked up, as
 * in the original).
 */
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "game.h"
#include "framebuffer8.h"
#include "joystick.h"
#include "sfx.h"
#include "text.h"
#include "music.h"
#include "track.h"

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(*array))

#include "data_sprites.h"
#include "data_caves.h"

// Developer options
#define DEV_IMMEDIATE_STARTUP 0
#define DEV_NEAR_OUTBOX 0
#define DEV_SINGLE_DIAMOND_NEEDED 0
#define DEV_CHEAP_BONUS_LIFE 0
#define DEV_CAMERA_DEBUGGING 0
#define DEV_SLOW_TICK_DURATION 0
#define DEV_QUICK_OUT_OF_TIME 0
#define DEV_SINGLE_LIFE 0

// Gameplay constants
#define START_CAVE CAVE_A
#define TICKS_PER_TURN 5
#define ROCKFORD_TURNS_TILL_BIRTH 12
#define CELL_COVER_TURNS 40
#define TILE_COVER_TICKS (32*TICKS_PER_TURN)
#define BONUS_LIFE_COST (DEV_CHEAP_BONUS_LIFE ? 5 : 500)
#define SPACE_FLASHING_TURNS 10
#define MAX_LIVES 9
#define COVER_PAUSE 2
#define TICKS_PER_CAVE_SECOND (7*TICKS_PER_TURN)
#define OUT_OF_TIME_ON_TURNS 25
#define OUT_OF_TIME_OFF_TURNS 42
#define TURNS_TILL_GAME_RESTART 10
#define TURNS_TILL_EXITING_CAVE 12

#define TOO_MANY_AMOEBA 200
#define AMOEBA_FACTOR_SLOW 127
#define AMOEBA_FACTOR_FAST 15

// Controls (JS_* bitmasks from joystick.h, checked against the current
// JoystickState - see isKeyDown() below). No KEY_QUIT: there's no OS to
// quit to on embedded, game_run() just loops forever. KEY_FAIL (the
// original's debug "force fail the cave" cheat) is kept, mapped to
// SELECT - a legitimate way to bail out of a cave gone wrong on real
// hardware, not just a dev cheat here.
#define KEY_FIRE  JS_BTN
#define KEY_RIGHT JS_RIGHT
#define KEY_LEFT  JS_LEFT
#define KEY_DOWN  JS_DOWN
#define KEY_UP    JS_UP
#define KEY_FAIL  JS_SELECT

// Cave map consists of cells, each cell contains 4 (2x2) tiles
#define TILE_SIZE 8
#define CELL_SIZE (TILE_SIZE*2)

#define BORDER_SIZE CELL_SIZE
#define STATUS_BAR_HEIGHT CELL_SIZE

// Viewport is the whole screen area except the border
#define VIEWPORT_WIDTH 256
#define VIEWPORT_HEIGHT 192
#define VIEWPORT_LEFT BORDER_SIZE
#define VIEWPORT_TOP BORDER_SIZE
#define VIEWPORT_RIGHT (VIEWPORT_LEFT + VIEWPORT_WIDTH - 1)
#define VIEWPORT_BOTTOM (VIEWPORT_TOP + VIEWPORT_HEIGHT - 1)

// Playfield is the whole viewport except the status bar
#define PLAYFIELD_WIDTH VIEWPORT_WIDTH
#define PLAYFIELD_HEIGHT (VIEWPORT_HEIGHT - STATUS_BAR_HEIGHT)
#define PLAYFIELD_LEFT VIEWPORT_LEFT
#define PLAYFIELD_TOP (VIEWPORT_TOP + STATUS_BAR_HEIGHT)
#define PLAYFIELD_RIGHT (PLAYFIELD_LEFT + PLAYFIELD_WIDTH - 1)
#define PLAYFIELD_BOTTOM (PLAYFIELD_TOP + PLAYFIELD_HEIGHT - 1)

#define PLAYFIELD_HEIGHT_IN_TILES (PLAYFIELD_HEIGHT/TILE_SIZE)
#define PLAYFIELD_WIDTH_IN_TILES (PLAYFIELD_WIDTH/TILE_SIZE)

#define CAMERA_START_LEFT (PLAYFIELD_LEFT + 6*TILE_SIZE)
#define CAMERA_STOP_LEFT (PLAYFIELD_LEFT + 14*TILE_SIZE)
#define CAMERA_START_TOP (PLAYFIELD_TOP + 4*TILE_SIZE)
#define CAMERA_STOP_TOP (PLAYFIELD_TOP + 9*TILE_SIZE)
#define CAMERA_START_RIGHT (PLAYFIELD_RIGHT - 6*TILE_SIZE + 1)
#define CAMERA_STOP_RIGHT (PLAYFIELD_RIGHT - 13*TILE_SIZE + 1)
#define CAMERA_START_BOTTOM (PLAYFIELD_BOTTOM - 4*TILE_SIZE + 1)
#define CAMERA_STOP_BOTTOM (PLAYFIELD_BOTTOM - 9*TILE_SIZE + 1)

#define CAMERA_X_MIN 0
#define CAMERA_Y_MIN 0
#define CAMERA_X_MAX (CAVE_WIDTH*CELL_SIZE - PLAYFIELD_WIDTH)
#define CAMERA_Y_MAX (CAVE_HEIGHT*CELL_SIZE - PLAYFIELD_HEIGHT)

#define CAMERA_STEP TILE_SIZE

// The original buffered a 4-bit-per-pixel backbuffer here and blitted it
// via GDI; this port's setPixel() writes straight into framebuffer8[]
// instead (see below), so only the canvas size survives, used to center
// it on this console's larger 320x240 screen.
#define BACKBUFFER_WIDTH (VIEWPORT_WIDTH + BORDER_SIZE*2)
#define BACKBUFFER_HEIGHT (VIEWPORT_HEIGHT + BORDER_SIZE*2)
#define SCREEN_OFFSET_X ((FB8_WIDTH - BACKBUFFER_WIDTH) / 2)
#define SCREEN_OFFSET_Y ((FB8_HEIGHT - BACKBUFFER_HEIGHT) / 2)

typedef enum {
  OBJ_SPACE = 0x00,
  OBJ_DIRT = 0x01,
  OBJ_BRICK_WALL = 0x02,
  OBJ_MAGIC_WALL = 0x03,
  OBJ_PRE_OUTBOX = 0x04,
  OBJ_FLASHING_OUTBOX = 0x05,
  OBJ_STEEL_WALL = 0x07,
  OBJ_FIREFLY_LEFT = 0x08,
  OBJ_FIREFLY_UP = 0x09,
  OBJ_FIREFLY_RIGHT = 0x0A,
  OBJ_FIREFLY_DOWN = 0x0B,
  OBJ_FIREFLY_LEFT_SCANNED = 0x0C,
  OBJ_FIREFLY_UP_SCANNED = 0x0D,
  OBJ_FIREFLY_RIGHT_SCANNED = 0x0E,
  OBJ_FIREFLY_DOWN_SCANNED = 0x0F,
  OBJ_BOULDER_STATIONARY = 0x10,
  OBJ_BOULDER_STATIONARY_SCANNED = 0x11,
  OBJ_BOULDER_FALLING = 0x12,
  OBJ_BOULDER_FALLING_SCANNED = 0x13,
  OBJ_DIAMOND_STATIONARY = 0x14,
  OBJ_DIAMOND_STATIONARY_SCANNED = 0x15,
  OBJ_DIAMOND_FALLING = 0x16,
  OBJ_DIAMOND_FALLING_SCANNED = 0x17,
  OBJ_EXPLODE_TO_SPACE_0 = 0x1B,
  OBJ_EXPLODE_TO_SPACE_1 = 0x1C,
  OBJ_EXPLODE_TO_SPACE_2 = 0x1D,
  OBJ_EXPLODE_TO_SPACE_3 = 0x1E,
  OBJ_EXPLODE_TO_SPACE_4 = 0x1F,
  OBJ_EXPLODE_TO_DIAMOND_0 = 0x20,
  OBJ_EXPLODE_TO_DIAMOND_1 = 0x21,
  OBJ_EXPLODE_TO_DIAMOND_2 = 0x22,
  OBJ_EXPLODE_TO_DIAMOND_3 = 0x23,
  OBJ_EXPLODE_TO_DIAMOND_4 = 0x24,
  OBJ_PRE_ROCKFORD_1 = 0x25,
  OBJ_PRE_ROCKFORD_2 = 0x26,
  OBJ_PRE_ROCKFORD_3 = 0x27,
  OBJ_PRE_ROCKFORD_4 = 0x28,
  OBJ_BUTTERFLY_DOWN = 0x30,
  OBJ_BUTTERFLY_LEFT = 0x31,
  OBJ_BUTTERFLY_UP = 0x32,
  OBJ_BUTTERFLY_RIGHT = 0x33,
  OBJ_BUTTERFLY_DOWN_SCANNED = 0x34,
  OBJ_BUTTERFLY_LEFT_SCANNED = 0x35,
  OBJ_BUTTERFLY_UP_SCANNED = 0x36,
  OBJ_BUTTERFLY_RIGHT_SCANNED = 0x37,
  OBJ_ROCKFORD = 0x38,
  OBJ_ROCKFORD_SCANNED = 0x39,
  OBJ_AMOEBA = 0x3A,
  OBJ_AMOEBA_SCANNED = 0x3B,
} Object;

typedef enum {
  OBJST_SINGLE,
  OBJST_LINE,
  OBJST_FILLED_RECT,
  OBJST_RECT,
} ObjectStructure;

#define CAVE_HEIGHT 22
#define CAVE_WIDTH 40
#define NUM_DIFFICULTY_LEVELS 5
#define NUM_RANDOM_OBJECTS 4

typedef struct {
  uint8_t caveNumber;
  uint8_t magicWallMillingTime; // also amoebaSlowGrowthTime
  uint8_t initialDiamondValue;
  uint8_t extraDiamondValue;
  uint8_t randomiserSeed[NUM_DIFFICULTY_LEVELS];
  uint8_t diamondsNeeded[NUM_DIFFICULTY_LEVELS];
  uint8_t caveTime[NUM_DIFFICULTY_LEVELS];
  uint8_t backgroundColor1;
  uint8_t backgroundColor2;
  uint8_t foregroundColor;
  uint8_t unused[2];
  uint8_t randomObject[NUM_RANDOM_OBJECTS];
  uint8_t objectProbability[NUM_RANDOM_OBJECTS];
} CaveInfo;

typedef enum {
  CAVE_A, CAVE_B, CAVE_C, CAVE_D, INTERMISSION_1,
  CAVE_E, CAVE_F, CAVE_G, CAVE_H, INTERMISSION_2,
  CAVE_I, CAVE_J, CAVE_K, CAVE_L, INTERMISSION_3,
  CAVE_M, CAVE_N, CAVE_O, CAVE_P, INTERMISSION_4,
  CAVE_COUNT,
} CaveName;

typedef enum {BLACK, GRAY, WHITE, RED, YELLOW, GREEN, BLUE, PURPLE, CYAN, COLOR_COUNT} Color;

typedef struct {
  Color boulderFg;
  Color brickWallFg;
  Color brickWallBg;
  Color dirtFg;
  Color flyFg;
  Color flyBg;
} CaveColors;

typedef enum {UP, DOWN, LEFT, RIGHT, DIRECTION_COUNT} Direction;
typedef enum {TURN_LEFT, STRAIGHT_AHEAD, TURN_RIGHT} Turning;
typedef enum {MAGIC_WALL_OFF, MAGIC_WALL_ON, MAGIC_WALL_EXPIRED} MagicWallStatus;

//
// Global variables
//

static uint8_t map[CAVE_HEIGHT][CAVE_WIDTH];
static CaveInfo *caveInfo;
static int turnsSinceRockfordSeenAlive;
static bool isOutOfTime;
static int livesLeft;
static int difficultyLevel;
static int score;
static int scoreTillBonusLife;
static int spaceFlashingTurnsLeft;
static int currentCaveNumber;
static MagicWallStatus magicWallStatus;

///////////////

/** @brief Current tick's joystick state, set once at the top of
 *         boulder_update() - lets isKeyDown() below keep the original's
 *         call shape (isKeyDown(KEY_RIGHT) etc.) at every one of its many
 *         call sites deep inside the ported logic, instead of threading a
 *         JoystickState pointer through every function signature. */
static const JoystickState *curInput;

static bool isKeyDown(uint8_t key) {
    return (curInput->raw & key) != 0;
}

/** @brief Freestanding rand() replacement (no libc under -nostdlib) - a
 *         small LCG, same technique as e.g. examples/invaders' rng_next().
 *         Every rand()%N call site in the ported logic below (cave-intro
 *         reveal, amoeba growth, boulder-push jitter, ...) works
 *         unchanged against this. */
static uint32_t rngState = 1;
static int rand(void) {
    rngState = rngState * 1664525u + 1013904223u;
    return (int)((rngState >> 8) & 0x7FFFFFFF);
}

/** @brief The original's 9-color palette (see Color/COLOR_COUNT below),
 *         mapped onto this engine's existing 16-color base palette
 *         (fb8_init_palette() in framebuffer8.c) - no palette changes
 *         needed. Indexed by Color. */
static const uint8_t boulderPalette[9] = {
    0,   /* BLACK  */
    12,  /* GRAY   */
    1,   /* WHITE  */
    2,   /* RED    */
    7,   /* YELLOW */
    5,   /* GREEN  */
    6,   /* BLUE   */
    4,   /* PURPLE */
    3,   /* CYAN   */
};

/** @brief The original's sound.h SoundID enum, kept so every playSound()
 *         call site below is unchanged; playSound() itself now maps each
 *         event onto this engine's canned sfx.h effects instead of the
 *         original's WASAPI synth (best-effort - see main.c's header
 *         comment). SND_UPDATE_CELL_COVER/SND_UPDATE_TILE_COVER fire once
 *         per revealed cell/tile during the cave-intro/outro wipe
 *         animation - deliberately silent here, mapping them would spam
 *         far more than the original's short blips ever did. */
typedef enum {
    SND_ROCKFORD_MOVE_SPACE,
    SND_ROCKFORD_MOVE_DIRT,
    SND_DIAMOND,
    SND_BOULDER,
    SND_ADDING_TIME_TO_SCORE,
    SND_UPDATE_CELL_COVER,
    SND_UPDATE_TILE_COVER,
    SND_ROCKFORD_BIRTH,
    SND_AMOEBA,
    SND_MAGIC_WALL,
} SoundID;

static void playSound(SoundID id) {
    switch (id) {
    case SND_ROCKFORD_MOVE_SPACE:
    case SND_ROCKFORD_MOVE_DIRT:
        sfx_play(SFX_MOVE);
        break;
    case SND_DIAMOND:
    case SND_ROCKFORD_BIRTH:
        sfx_play(SFX_PICKUP);
        break;
    case SND_BOULDER:
    case SND_AMOEBA:
        sfx_play(SFX_NOISE_SHORT);
        break;
    case SND_ADDING_TIME_TO_SCORE:
        sfx_play(SFX_BEEP);
        break;
    case SND_MAGIC_WALL:
        sfx_play(SFX_SIREN);
        break;
    case SND_UPDATE_CELL_COVER:
    case SND_UPDATE_TILE_COVER:
        break;
    }
}

//
// Graphics
//

void setPixel(int x, int y, Color color) {
  fb8_set_pixel(SCREEN_OFFSET_X + x, SCREEN_OFFSET_Y + y, boulderPalette[color]);
}

void drawRect(int left, int top, int right, int bottom, Color color) {
  for (int x = left; x <= right; ++x) {
    setPixel(x, top, color);
    setPixel(x, bottom, color);
  }
  for (int y = top; y <= bottom; ++y) {
    setPixel(left, y, color);
    setPixel(right, y, color);
  }
}

void drawFilledRect(int left, int top, int right, int bottom, Color color) {
  for (int y = top; y <= bottom; ++y) {
    for (int x = left; x <= right; ++x) {
      setPixel(x, y, color);
    }
  }
}

void drawTile(uint8_t *tile, int dstX, int dstY, Color fgColor, Color bgColor, int vOffset) {
  for (uint8_t bmpY = 0; bmpY < TILE_SIZE; ++bmpY) {
    int y = dstY + bmpY;
    uint8_t byte = tile[(bmpY + vOffset) % TILE_SIZE];

    for (uint8_t bmpX = 0; bmpX < TILE_SIZE; ++bmpX) {
      int x = dstX + bmpX;
      uint8_t mask = 1 << ((TILE_SIZE-1) - bmpX);
      uint8_t bit = byte & mask;
      Color color = bit ? fgColor : bgColor;

      setPixel(x, y, color);
    }
  }
}

void drawSprite(uint8_t *sprite, int frame, int dstX, int dstY, Color fgColor, Color bgColor, int vOffset) {
  int frames = sprite[0];
  int size = sprite[1];
  int bytesPerFrame = size*size*TILE_SIZE;
  int bytesPerRow = size*TILE_SIZE;

  for (int row = 0; row < size; ++row) {
    for (int col = 0; col < size; ++col) {
      int x = dstX + col*TILE_SIZE;
      int y = dstY + row*TILE_SIZE;

      if (x >= VIEWPORT_LEFT && (x+TILE_SIZE-1) <= VIEWPORT_RIGHT &&
          y >= VIEWPORT_TOP && (y+TILE_SIZE-1) <= VIEWPORT_BOTTOM) {
        uint8_t *data = sprite + 2 + (frame%frames)*bytesPerFrame + row*bytesPerRow + col*TILE_SIZE;
        drawTile(data, x, y, fgColor, bgColor, vOffset);
      }
    }
  }
}

//
// Cave decoding
//

void nextRandom(int *randSeed1, int *randSeed2) {
  int tempRand1 = (*randSeed1 & 0x0001) * 0x0080;
  int tempRand2 = (*randSeed2 >> 1) & 0x007F;

  int result = (*randSeed2) + (*randSeed2 & 0x0001) * 0x0080;
  int carry = (result > 0x00FF);
  result = result & 0x00FF;

  result = result + carry + 0x13;
  carry = (result > 0x00FF);
  *randSeed2 = result & 0x00FF;

  result = *randSeed1 + carry + tempRand1;
  carry = (result > 0x00FF);
  result = result & 0x00FF;

  result = result + carry + tempRand2;
  *randSeed1 = result & 0x00FF;
}

void placeObjectLine(Object object, int row, int col, int length, int direction) {
  int ldx[8] = { 0,  1, 1, 1, 0, -1, -1, -1 };
  int ldy[8] = { -1, -1, 0, 1, 1,  1,  0, -1 };

  for (int i = 0; i < length; i++) {
    map[row + i*ldy[direction]][col + i*ldx[direction]] = object;
  }
}

void placeObjectFilledRect(Object object, int row, int col, int width, int height, Object fillObject) {
  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      if (y == 0 || y == height-1 || x == 0 || x == width-1) {
        map[row+y][col+x] = object;
      } else {
        map[row+y][col+x] = fillObject;
      }
    }
  }
}

void placeObjectRect(Object object, int row, int col, int width, int height) {
  for (int i = 0; i < width; i++) {
    map[row][col+i] = object;
    map[row+height-1][col+i] = object;
  }
  for (int i = 0; i < height; i++) {
    map[row+i][col] = object;
    map[row+i][col+width-1] = object;
  }
}

void decodeCave(int caveIndex) {
  uint8_t *caves[CAVE_COUNT] = {
    caveA, caveB, caveC, caveD, intermission1,
    caveE, caveF, caveG, caveH, intermission2,
    caveI, caveJ, caveK, caveL, intermission3,
    caveM, caveN, caveO, caveP, intermission4,
  };

  assert(caveIndex >= 0 && caveIndex < CAVE_COUNT);

  caveInfo = (CaveInfo *)caves[caveIndex];

  // Clear out the map
  for (int row = 0; row < CAVE_HEIGHT; row++) {
    for (int col = 0; col < CAVE_WIDTH; col++) {
      map[row][col] = OBJ_STEEL_WALL;
    }
  }

  // Decode random map objects
  {
    int randSeed1 = 0;
    int randSeed2 = caveInfo->randomiserSeed[0];

    for (int row = 1; row < CAVE_HEIGHT; row++) {
      for (int col = 0; col < CAVE_WIDTH; col++) {
        Object object = OBJ_DIRT;
        nextRandom(&randSeed1, &randSeed2);
        for (int i = 0; i < NUM_RANDOM_OBJECTS; i++) {
          if (randSeed1 < caveInfo->objectProbability[i]) {
            object = caveInfo->randomObject[i];
          }
        }
        map[row][col] = object;
      }
    }
  }

  // Steel bounds
  placeObjectRect(OBJ_STEEL_WALL, 0, 0, CAVE_WIDTH, CAVE_HEIGHT);

  // Decode explicit map data
  {
    uint8_t *explicitData = caves[caveIndex] + sizeof(CaveInfo);
    int uselessTopBorderHeight = 2;

    for (int i = 0; explicitData[i] != 0xFF; i++) {
      Object object = (explicitData[i] & 0x3F);

      switch (3 & (explicitData[i] >> 6)) {
        case OBJST_SINGLE: {
          int col = explicitData[++i];
          int row = explicitData[++i] - uselessTopBorderHeight;
          map[row][col] = object;
          break;
        }
        case OBJST_LINE: {
          int col = explicitData[++i];
          int row = explicitData[++i] - uselessTopBorderHeight;
          int length = explicitData[++i];
          int direction = explicitData[++i];
          placeObjectLine(object, row, col, length, direction);
          break;
        }
        case OBJST_FILLED_RECT: {
          int col = explicitData[++i];
          int row = explicitData[++i] - uselessTopBorderHeight;
          int width = explicitData[++i];
          int height = explicitData[++i];
          Object fill = explicitData[++i];
          placeObjectFilledRect(object, row, col, width, height, fill);
          break;
        }
        case OBJST_RECT: {
          int col = explicitData[++i];
          int row = explicitData[++i] - uselessTopBorderHeight;
          int width = explicitData[++i];
          int height = explicitData[++i];
          placeObjectRect(object, row, col, width, height);
          break;
        }
      }
    }
  }
}

//
// Gameplay
//

bool isObjectRound(Object object) {
  return object == OBJ_BOULDER_STATIONARY || object == OBJ_DIAMOND_STATIONARY || object == OBJ_BRICK_WALL;
}

bool isObjectExplosive(Object object) {
  return object == OBJ_ROCKFORD ||
    object == OBJ_FIREFLY_LEFT ||
    object == OBJ_FIREFLY_UP ||
    object == OBJ_FIREFLY_RIGHT ||
    object == OBJ_FIREFLY_DOWN ||
    object == OBJ_BUTTERFLY_DOWN ||
    object == OBJ_BUTTERFLY_LEFT ||
    object == OBJ_BUTTERFLY_UP ||
    object == OBJ_BUTTERFLY_RIGHT;
}

bool explodesToDiamonds(Object object) {
  assert(isObjectExplosive(object));
  return object == OBJ_BUTTERFLY_DOWN || object == OBJ_BUTTERFLY_LEFT || object == OBJ_BUTTERFLY_UP || object == OBJ_BUTTERFLY_RIGHT;
}

void explodeCell(int row, int col, bool toDiamonds, int stage) {
  if (map[row][col] != OBJ_STEEL_WALL) {
    if (toDiamonds) {
      map[row][col] = stage == 0 ? OBJ_EXPLODE_TO_DIAMOND_0 : OBJ_EXPLODE_TO_DIAMOND_1;
    } else {
      map[row][col] = stage == 0 ? OBJ_EXPLODE_TO_SPACE_0 : OBJ_EXPLODE_TO_SPACE_1;
    }
  }
}

void explode(int atRow, int atCol, int scanRow, int scanCol) {
  bool toDiamonds = explodesToDiamonds(map[atRow][atCol]);

  for (int row = atRow-1; row <= atRow+1; ++row) {
    for (int col = atCol-1; col <= atCol+1; ++col) {
      int stage = ((row < scanRow) || (row == scanRow && col <= scanCol)) ? 1 : 0;
      explodeCell(row, col, toDiamonds, stage);
    }
  }
}

void updateBoulderAndDiamond(int row, int col, bool isFalling, bool isBoulder) {
  Object fallingScannedObj = isBoulder ? OBJ_BOULDER_FALLING_SCANNED : OBJ_DIAMOND_FALLING_SCANNED;
  Object stationaryScannedObj = isBoulder ? OBJ_BOULDER_STATIONARY_SCANNED : OBJ_DIAMOND_STATIONARY_SCANNED;
  Object fallingScannedObjInvert = isBoulder ? OBJ_DIAMOND_FALLING_SCANNED : OBJ_BOULDER_FALLING_SCANNED;

  if (map[row+1][col] == OBJ_SPACE) {
    map[row+1][col] = fallingScannedObj;
    map[row][col] = OBJ_SPACE;
    if (!isFalling) {
      playSound(isBoulder ? SND_BOULDER : SND_DIAMOND);
    }
  } else if (isFalling && map[row+1][col] == OBJ_MAGIC_WALL) {
    if (magicWallStatus == MAGIC_WALL_OFF) {
      magicWallStatus = MAGIC_WALL_ON;
    }
    if (magicWallStatus == MAGIC_WALL_ON && map[row+2][col] == OBJ_SPACE) {
      map[row+2][col] = fallingScannedObjInvert;
    }
    map[row][col] = OBJ_SPACE;
  } else if (isObjectRound(map[row+1][col])) {
    // Try to roll off
    if (map[row][col-1] == OBJ_SPACE && map[row+1][col-1] == OBJ_SPACE) {
      // Roll left
      map[row][col-1] = fallingScannedObj;
      map[row][col] = OBJ_SPACE;
    } else if (map[row][col+1] == OBJ_SPACE && map[row+1][col+1] == OBJ_SPACE) {
      // Roll right
      map[row][col+1] = fallingScannedObj;
      map[row][col] = OBJ_SPACE;
    } else {
      map[row][col] = stationaryScannedObj;
      if (isFalling) {
        playSound(isBoulder ? SND_BOULDER : SND_DIAMOND);
      }
    }
  } else if (isFalling && isObjectExplosive(map[row+1][col])) {
    explode(row+1, col, row, col);
  } else {
    map[row][col] = stationaryScannedObj;
    if (isFalling) {
      playSound(isBoulder ? SND_BOULDER : SND_DIAMOND);
    }
  }
}

bool isFailed() {
  return turnsSinceRockfordSeenAlive >= 16 || isOutOfTime;
}

bool checkFlyExplode(Object object) {
  return object == OBJ_ROCKFORD || object == OBJ_ROCKFORD_SCANNED || object == OBJ_AMOEBA;
}

void getNewFlyPosition(int curRow, int curCol, Direction curDirection, Turning turning, int *newRow, int *newCol, Direction *newDirection) {
  *newRow = curRow;
  *newCol = curCol;

  switch(curDirection) {
    case UP:
      switch (turning) {
        case TURN_LEFT:      (*newCol)--; *newDirection = LEFT;  break;
        case STRAIGHT_AHEAD: (*newRow)--; *newDirection = UP;    break;
        case TURN_RIGHT:     (*newCol)++; *newDirection = RIGHT; break;
      }
      break;
    case DOWN:
      switch (turning) {
        case TURN_LEFT:      (*newCol)++; *newDirection = RIGHT; break;
        case STRAIGHT_AHEAD: (*newRow)++; *newDirection = DOWN;  break;
        case TURN_RIGHT:     (*newCol)--; *newDirection = LEFT;  break;
      }
      break;
    case LEFT:
      switch (turning) {
        case TURN_LEFT:      (*newRow)++; *newDirection = DOWN; break;
        case STRAIGHT_AHEAD: (*newCol)--; *newDirection = LEFT; break;
        case TURN_RIGHT:     (*newRow)--; *newDirection = UP;   break;
      }
      break;
    case RIGHT:
      switch (turning) {
        case TURN_LEFT:      (*newRow)--; *newDirection = UP;    break;
        case STRAIGHT_AHEAD: (*newCol)++; *newDirection = RIGHT; break;
        case TURN_RIGHT:     (*newRow)++; *newDirection = DOWN;  break;
      }
      break;
    default:
      break;  /* DIRECTION_COUNT is a sentinel, never an actual direction */
  }
}

/* direction/fly are always one of the 4 real enumerators at every call
 * site below - the default cases exist only to satisfy the compiler
 * (every other enumerator, e.g. DIRECTION_COUNT or unrelated Object
 * values, can't reach here). */

Object getFlyScanned(Direction direction, bool isFirefly) {
  switch (direction) {
    case UP    : return isFirefly ? OBJ_FIREFLY_UP_SCANNED    : OBJ_BUTTERFLY_UP_SCANNED;
    case DOWN  : return isFirefly ? OBJ_FIREFLY_DOWN_SCANNED  : OBJ_BUTTERFLY_DOWN_SCANNED;
    case LEFT  : return isFirefly ? OBJ_FIREFLY_LEFT_SCANNED  : OBJ_BUTTERFLY_LEFT_SCANNED;
    case RIGHT : return isFirefly ? OBJ_FIREFLY_RIGHT_SCANNED : OBJ_BUTTERFLY_RIGHT_SCANNED;
    default    : return OBJ_SPACE;
  }
}

Direction getFlyDirection(Object fly, bool isFirefly) {
  if (isFirefly) {
    switch (fly) {
      case OBJ_FIREFLY_UP:      return UP;
      case OBJ_FIREFLY_DOWN:    return DOWN;
      case OBJ_FIREFLY_LEFT:    return LEFT;
      case OBJ_FIREFLY_RIGHT:   return RIGHT;
      default: break;
    }
  } else {
    switch (fly) {
      case OBJ_BUTTERFLY_UP:    return UP;
      case OBJ_BUTTERFLY_DOWN:  return DOWN;
      case OBJ_BUTTERFLY_LEFT:  return LEFT;
      case OBJ_BUTTERFLY_RIGHT: return RIGHT;
      default: break;
    }
  }
  return UP;
}

void updateFly(int row, int col, bool isFirefly) {
  if (checkFlyExplode(map[row-1][col]) || checkFlyExplode(map[row+1][col]) ||
      checkFlyExplode(map[row][col-1]) || checkFlyExplode(map[row][col+1])) {
    explode(row, col, row, col);
  } else {
    int direction = getFlyDirection(map[row][col], isFirefly);
    int newRow, newCol;
    Direction newDirection;
    getNewFlyPosition(row, col, direction, (isFirefly ? TURN_LEFT : TURN_RIGHT), &newRow, &newCol, &newDirection);
    if (map[newRow][newCol] == OBJ_SPACE) {
      map[newRow][newCol] = getFlyScanned(newDirection, isFirefly);
      map[row][col] = OBJ_SPACE;
    } else {
      getNewFlyPosition(row, col, direction, STRAIGHT_AHEAD, &newRow, &newCol, &newDirection);
      if (map[newRow][newCol] == OBJ_SPACE) {
        map[newRow][newCol] = getFlyScanned(newDirection, isFirefly);
        map[row][col] = OBJ_SPACE;
      } else {
        getNewFlyPosition(row, col, direction, (isFirefly ? TURN_RIGHT : TURN_LEFT), &newRow, &newCol, &newDirection);
        map[row][col] = getFlyScanned(newDirection, isFirefly);
      }
    }
  }
}

void addScore(int amount) {
  score += amount;

  // Check for bonus life
  scoreTillBonusLife -= amount;
  if (scoreTillBonusLife <= 0) {
    scoreTillBonusLife += BONUS_LIFE_COST;
    spaceFlashingTurnsLeft = SPACE_FLASHING_TURNS;
    ++livesLeft;
    if (livesLeft > MAX_LIVES) {
      livesLeft = MAX_LIVES;
    }
  }
}

bool isIntermission() {
  return ((currentCaveNumber + 1) % 5) == 0;
}

void incrementCaveNumber() {
  ++currentCaveNumber;
  if (currentCaveNumber >= CAVE_COUNT) {
    currentCaveNumber = 0;
    if (difficultyLevel < NUM_DIFFICULTY_LEVELS-1) {
      ++difficultyLevel;
    }
  }
}

char getCurrentCaveLetter() {
  switch (currentCaveNumber) {
    case CAVE_A: return 'A';
    case CAVE_B: return 'B';
    case CAVE_C: return 'C';
    case CAVE_D: return 'D';
    case CAVE_E: return 'E';
    case CAVE_F: return 'F';
    case CAVE_G: return 'G';
    case CAVE_H: return 'H';
    case CAVE_I: return 'I';
    case CAVE_J: return 'J';
    case CAVE_K: return 'K';
    case CAVE_L: return 'L';
    case CAVE_M: return 'M';
    case CAVE_N: return 'N';
    case CAVE_O: return 'O';
    case CAVE_P: return 'P';
  }
  return ' ';
}

bool canAmoebaGrowHere(int row, int col) {
  return map[row][col] == OBJ_SPACE || map[row][col] == OBJ_DIRT;
}

void getRandomCellNear(int row, int col, int *newRow, int *newCol) {
  *newRow = row;
  *newCol = col;
  switch (rand() % DIRECTION_COUNT) {
    case UP:    (*newRow)--; break;
    case DOWN:  (*newRow)++; break;
    case LEFT:  (*newCol)--; break;
    case RIGHT: (*newCol)++; break;
  }
}


//
// Per-cave palette (was local caveColors[]/assignments inside WinMain -
// unchanged content, just moved into a function called once from
// boulder_init()).
//

static CaveColors caveColors[CAVE_COUNT];

static void init_cave_colors(void) {
  caveColors[CAVE_A].boulderFg = YELLOW;
  caveColors[CAVE_A].brickWallFg = GRAY;
  caveColors[CAVE_A].brickWallBg = RED;
  caveColors[CAVE_A].dirtFg = RED;

  caveColors[CAVE_B].boulderFg = BLUE;
  caveColors[CAVE_B].brickWallFg = BLUE;
  caveColors[CAVE_B].brickWallBg = YELLOW;
  caveColors[CAVE_B].dirtFg = PURPLE;
  caveColors[CAVE_B].flyFg = BLUE;
  caveColors[CAVE_B].flyBg = YELLOW;

  caveColors[CAVE_C].boulderFg = PURPLE;
  caveColors[CAVE_C].brickWallFg = GRAY;
  caveColors[CAVE_C].brickWallBg = PURPLE;
  caveColors[CAVE_C].dirtFg = GREEN;

  caveColors[CAVE_D].boulderFg = YELLOW;
  caveColors[CAVE_D].dirtFg = BLUE;
  caveColors[CAVE_D].flyFg = CYAN;
  caveColors[CAVE_D].flyBg = BLACK;

  caveColors[INTERMISSION_1].boulderFg = PURPLE;
  caveColors[INTERMISSION_1].dirtFg = BLUE;
  caveColors[INTERMISSION_1].flyFg = BLUE;
  caveColors[INTERMISSION_1].flyBg = BLACK;

  caveColors[CAVE_E].boulderFg = PURPLE;
  caveColors[CAVE_E].dirtFg = RED;
  caveColors[CAVE_E].flyFg = RED;
  caveColors[CAVE_E].flyBg = YELLOW;

  caveColors[CAVE_F].boulderFg = PURPLE;
  caveColors[CAVE_F].brickWallFg = PURPLE;
  caveColors[CAVE_F].brickWallBg = GRAY;
  caveColors[CAVE_F].dirtFg = BLUE;
  caveColors[CAVE_F].flyFg = PURPLE;
  caveColors[CAVE_F].flyBg = GRAY;

  caveColors[CAVE_G].boulderFg = PURPLE;
  caveColors[CAVE_G].brickWallFg = RED;
  caveColors[CAVE_G].brickWallBg = GRAY;
  caveColors[CAVE_G].dirtFg = RED;
  caveColors[CAVE_G].flyFg = PURPLE;
  caveColors[CAVE_G].flyBg = YELLOW;

  caveColors[CAVE_H].boulderFg = CYAN;
  caveColors[CAVE_H].brickWallFg = CYAN;
  caveColors[CAVE_H].brickWallBg = GRAY;
  caveColors[CAVE_H].dirtFg = RED;
  caveColors[CAVE_H].flyFg = BLUE;
  caveColors[CAVE_H].flyBg = CYAN;

  caveColors[INTERMISSION_2].boulderFg = YELLOW;
  caveColors[INTERMISSION_2].dirtFg = BLUE;
  caveColors[INTERMISSION_2].flyFg = RED;
  caveColors[INTERMISSION_2].flyBg = GRAY;

  caveColors[CAVE_I].boulderFg = BLUE;
  caveColors[CAVE_I].brickWallFg = BLUE;
  caveColors[CAVE_I].brickWallBg = CYAN;
  caveColors[CAVE_I].dirtFg = PURPLE;

  caveColors[CAVE_J].boulderFg = RED;
  caveColors[CAVE_J].brickWallFg = BLUE;
  caveColors[CAVE_J].brickWallBg = GRAY;
  caveColors[CAVE_J].dirtFg = BLUE;
  caveColors[CAVE_J].flyFg = RED;
  caveColors[CAVE_J].flyBg = GRAY;

  caveColors[CAVE_K].boulderFg = YELLOW;
  caveColors[CAVE_K].brickWallFg = GRAY;
  caveColors[CAVE_K].brickWallBg = YELLOW;
  caveColors[CAVE_K].dirtFg = GREEN;
  caveColors[CAVE_K].flyFg = GREEN;
  caveColors[CAVE_K].flyBg = GRAY;

  caveColors[CAVE_L].boulderFg = YELLOW;
  caveColors[CAVE_L].brickWallFg = YELLOW;
  caveColors[CAVE_L].brickWallBg = GRAY;
  caveColors[CAVE_L].dirtFg = GREEN;
  caveColors[CAVE_L].flyFg = GREEN;
  caveColors[CAVE_L].flyBg = GRAY;

  caveColors[INTERMISSION_3].boulderFg = GREEN;
  caveColors[INTERMISSION_3].flyFg = GREEN;
  caveColors[INTERMISSION_3].flyBg = RED;

  caveColors[CAVE_M].boulderFg = RED;
  caveColors[CAVE_M].brickWallFg = BLACK;
  caveColors[CAVE_M].brickWallBg = GRAY;
  caveColors[CAVE_M].dirtFg = BLUE;
  caveColors[CAVE_M].flyFg = YELLOW;
  caveColors[CAVE_M].flyBg = BLACK;

  caveColors[CAVE_N].boulderFg = YELLOW;
  caveColors[CAVE_N].dirtFg = PURPLE;
  caveColors[CAVE_N].flyFg = YELLOW;
  caveColors[CAVE_N].flyBg = BLACK;

  caveColors[CAVE_O].boulderFg = BLUE;
  caveColors[CAVE_O].brickWallFg = BLACK;
  caveColors[CAVE_O].brickWallBg = GRAY;
  caveColors[CAVE_O].dirtFg = PURPLE;
  caveColors[CAVE_O].flyFg = BLUE;
  caveColors[CAVE_O].flyBg = GRAY;

  caveColors[CAVE_P].boulderFg = CYAN;
  caveColors[CAVE_P].brickWallFg = PURPLE;
  caveColors[CAVE_P].brickWallBg = GRAY;
  caveColors[CAVE_P].dirtFg = RED;
  caveColors[CAVE_P].flyFg = PURPLE;
  caveColors[CAVE_P].flyBg = GRAY;

  caveColors[INTERMISSION_4].boulderFg = GREEN;
  caveColors[INTERMISSION_4].brickWallFg = GRAY;
  caveColors[INTERMISSION_4].brickWallBg = GREEN;
  caveColors[INTERMISSION_4].dirtFg = YELLOW;
}

//
// Game state (was local variables inside WinMain's loop - moved to file
// scope so they persist across this engine's separate init/update/draw
// callbacks instead of one continuous function body).
//

static bool cellCover[CAVE_HEIGHT][CAVE_WIDTH];
static bool tileCover[PLAYFIELD_HEIGHT_IN_TILES][PLAYFIELD_WIDTH_IN_TILES];
static char statusBarText[PLAYFIELD_WIDTH_IN_TILES + 1];
static CaveColors curColors;

static int turn;
static int tick;

static bool isGameStart = true;
static int turnsTillGameRestart;
static int turnsTillExitingCave;
static bool isAddingTimeToScore;

static const Color normalBorderColor = BLACK;
static const Color flashBorderColor = GRAY;
static Color borderColor;

static int cameraX;
static int cameraY;
static int cameraVelX;
static int cameraVelY;

static bool isCaveStart = true;
static int pauseTurnsLeft;

static bool isExitingCave;
static int caveTimeLeft;
static int ticksTillNextCaveSecond;
static bool isOutOfTimeTextShown;
static int outOfTimeTurn;
static int diamondsCollected;
static int currentDiamondValue;
static int rockfordTurnsTillBirth;
static int cellCoverTurnsLeft;
static int tileCoverTicksLeft;

static int rockfordCol;
static int rockfordRow;
static bool rockfordIsBlinking;
static bool rockfordIsTapping;
static bool rockfordIsMoving;
static bool rockfordIsFacingRight;

static int amoebaSlowGrowthTimeLeft;
static int magicWallMillingTimeLeft;

static int numberOfAmoebaFoundThisTurn;
static int totalAmoebaFoundLastTurn;
static bool amoebaSuffocatedLastTurn;
static bool atLeastOneAmoebaFoundThisTurnWhichCanGrow;

//
// Small freestanding string helpers (no sprintf under -nostdlib) - same
// pattern examples/invaders and examples/kobold3d already use.
//

static void append(char *dst, int *pos, const char *s) {
  while (*s) dst[(*pos)++] = *s++;
}

/** @brief Appends value zero-padded to at least `width` digits (width=1
 *         for a plain, unpadded number). */
static void append_uint_padded(char *dst, int *pos, int value, int width) {
  char tmp[12];
  int n = 0;
  unsigned v = (unsigned)value;
  if (v == 0) {
    tmp[n++] = '0';
  } else {
    while (v > 0) {
      tmp[n++] = (char)('0' + v % 10);
      v /= 10;
    }
  }
  for (int i = n; i < width; i++) dst[(*pos)++] = '0';
  for (int i = n - 1; i >= 0; i--) dst[(*pos)++] = tmp[i];
}

//
// GameAPI
//

/** @brief GameAPI init callback. */
static void boulder_init(void) {
  sfx_init();
  rngState ^= ((uint32_t)joystick_get_adc_x() << 16) ^ joystick_get_adc_y();
  init_cave_colors();
  music_init(boulder_track);
}

/** @brief GameAPI update callback: one "tick" of the original's fixed-
 *         timestep simulation (~34ms, matching its own tickDuration),
 *         driven by this engine's fixed-timestep game_run() instead of
 *         the original's manual dt-accumulator - see boulderdash_game/
 *         main() at the bottom of this file. */
static void boulder_update(const JoystickState *js, uint32_t tick_ms) {
  (void)tick_ms;
  curInput = js;

  // Initialization on game start
  if (isGameStart) {
    isGameStart = false;

    isCaveStart = true;
    pauseTurnsLeft = 0;
    currentCaveNumber = START_CAVE;
    difficultyLevel = 0;
    livesLeft = 3;
    score = 0;
    scoreTillBonusLife = BONUS_LIFE_COST;
    spaceFlashingTurnsLeft = 0;
  }

  // Initialization on cave start
  if (isCaveStart && pauseTurnsLeft == 0) {
    isCaveStart = false;

    decodeCave(currentCaveNumber);
    curColors = caveColors[currentCaveNumber];

    isExitingCave = false;
    turnsSinceRockfordSeenAlive = 0;
    diamondsCollected = 0;
    currentDiamondValue = caveInfo->initialDiamondValue;
    caveTimeLeft = caveInfo->caveTime[difficultyLevel];

    amoebaSlowGrowthTimeLeft = caveInfo->magicWallMillingTime;
    magicWallMillingTimeLeft = caveInfo->magicWallMillingTime;

    ticksTillNextCaveSecond = TICKS_PER_CAVE_SECOND;
    isOutOfTime = false;
    isOutOfTimeTextShown = false;
    outOfTimeTurn = 0;
    rockfordTurnsTillBirth = ROCKFORD_TURNS_TILL_BIRTH;
    cellCoverTurnsLeft = CELL_COVER_TURNS;
    magicWallStatus = MAGIC_WALL_OFF;

    numberOfAmoebaFoundThisTurn = 0;
    totalAmoebaFoundLastTurn = 0;
    amoebaSuffocatedLastTurn = false;
    atLeastOneAmoebaFoundThisTurnWhichCanGrow = true;

    rockfordIsBlinking = false;
    rockfordIsTapping = false;
    tileCoverTicksLeft = 0;
    rockfordIsMoving = false;
    rockfordIsFacingRight = true;

    for (int row = 0; row < CAVE_HEIGHT; ++row) {
      for (int col = 0; col < CAVE_WIDTH; ++col) {
        cellCover[row][col] = true;
      }
    }

    for (int row = 0; row < PLAYFIELD_HEIGHT_IN_TILES; ++row) {
      for (int col = 0; col < PLAYFIELD_WIDTH_IN_TILES; ++col) {
        tileCover[row][col] = false;
      }
    }

    // Find initial rockford position
    for (int row = 0; row < CAVE_HEIGHT; ++row) {
      for (int col = 0; col < CAVE_WIDTH; ++col) {
        if (map[row][col] == OBJ_PRE_ROCKFORD_1) {
          rockfordRow = row;
          rockfordCol = col;
        }
      }
    }
  }

  ++tick;

  //
  // Do tick
  //

  int rockfordRectLeft = PLAYFIELD_LEFT + rockfordCol*CELL_SIZE - cameraX;
  int rockfordRectTop = PLAYFIELD_TOP + rockfordRow*CELL_SIZE - cameraY;
  int rockfordRectRight = rockfordRectLeft + CELL_SIZE;
  int rockfordRectBottom = rockfordRectTop + CELL_SIZE;

  //
  // Move camera
  //
  // The original ran this once per *turn* (TICKS_PER_TURN=5 ticks apart),
  // same cadence as Rockford's own up-to-CELL_SIZE(16px)-per-turn move,
  // while the camera only closes by CAMERA_STEP(8px)/turn once
  // triggered - half Rockford's speed, so holding a direction for a few
  // turns let him drift out past the camera's reach entirely (visible as
  // "runs off screen"). Running the same per-turn step once per *tick*
  // instead (5x more often, still CAMERA_STEP per step) comfortably
  // outpaces him and reads as noticeably snappier scrolling too.
  //

  if (rockfordRectRight > CAMERA_START_RIGHT) {
    cameraVelX = CAMERA_STEP;
  } else if (rockfordRectLeft < CAMERA_START_LEFT) {
    cameraVelX = -CAMERA_STEP;
  }
  if (rockfordRectBottom > CAMERA_START_BOTTOM) {
    cameraVelY = CAMERA_STEP;
  } else if (rockfordRectTop < CAMERA_START_TOP) {
    cameraVelY = -CAMERA_STEP;
  }

  if (rockfordRectLeft >= CAMERA_STOP_LEFT && rockfordRectRight <= CAMERA_STOP_RIGHT) {
    cameraVelX = 0;
  }
  if (rockfordRectTop >= CAMERA_STOP_TOP && rockfordRectBottom <= CAMERA_STOP_BOTTOM) {
    cameraVelY = 0;
  }

  cameraX += cameraVelX;
  cameraY += cameraVelY;

  if (cameraX < CAMERA_X_MIN) {
    cameraX = CAMERA_X_MIN;
  } else if (cameraX > CAMERA_X_MAX) {
    cameraX = CAMERA_X_MAX;
  }

  if (cameraY < CAMERA_Y_MIN) {
    cameraY = CAMERA_Y_MIN;
  } else if (cameraY > CAMERA_Y_MAX) {
    cameraY = CAMERA_Y_MAX;
  }

  if (isAddingTimeToScore) {
    if (caveTimeLeft > 0) {
      --caveTimeLeft;
      addScore(1);
      playSound(SND_ADDING_TIME_TO_SCORE);
    } else {
      isAddingTimeToScore = false;
      isExitingCave = true;
      incrementCaveNumber();
      turnsTillExitingCave = TURNS_TILL_EXITING_CAVE;
    }
  } else {
    //
    // Update cave timer
    //

    if (turnsTillExitingCave == 0 && tileCoverTicksLeft == 0 && rockfordTurnsTillBirth == 0 && !isOutOfTime) {
      --ticksTillNextCaveSecond;
      if (ticksTillNextCaveSecond == 0) {
        ticksTillNextCaveSecond = TICKS_PER_CAVE_SECOND;
        if (caveTimeLeft > 0) {
          --caveTimeLeft;

          if (amoebaSlowGrowthTimeLeft > 0) {
            --amoebaSlowGrowthTimeLeft;
          }

          if (magicWallStatus == MAGIC_WALL_ON) {
            --magicWallMillingTimeLeft;
            if (magicWallMillingTimeLeft == 0) {
              magicWallStatus = MAGIC_WALL_EXPIRED;
            }
          }
        } else {
          isOutOfTime = true;
          isOutOfTimeTextShown = true;
        }
      }
    }

    //
    // Turn-based update logic
    //

    if (tick % TICKS_PER_TURN == 0) {
      if (pauseTurnsLeft > 0) {
        pauseTurnsLeft--;
      } else {
        turn++;

        //
        // Do turn
        //

        borderColor = normalBorderColor;

        if (turnsTillExitingCave == 0 && spaceFlashingTurnsLeft > 0) {
          --spaceFlashingTurnsLeft;
        }

        if (magicWallStatus == MAGIC_WALL_ON) {
          playSound(SND_MAGIC_WALL);
        }

        // (camera follow now runs once per tick, above - see the comment
        // there)

        //
        // Out of time
        //

        if (isOutOfTime) {
          ++outOfTimeTurn;
          if (isOutOfTimeTextShown) {
            if (outOfTimeTurn == OUT_OF_TIME_ON_TURNS) {
              outOfTimeTurn = 0;
              isOutOfTimeTextShown = false;
            }
          } else {
            if (outOfTimeTurn == OUT_OF_TIME_OFF_TURNS) {
              outOfTimeTurn = 0;
              isOutOfTimeTextShown = true;
            }
          }
        }

        //////////////////////

        if (turnsTillGameRestart > 0) {
          --turnsTillGameRestart;
          if (turnsTillGameRestart == 0) {
            isGameStart = true;
          }
        } else if (turnsTillExitingCave > 0) {
          --turnsTillExitingCave;
          if (turnsTillExitingCave == 0) {
            tileCoverTicksLeft = TILE_COVER_TICKS;
          }
        } else if (isExitingCave) {
          // Do nothing
        } else if (cellCoverTurnsLeft > 0) {
          //
          // Update cell cover
          //

          cellCoverTurnsLeft--;
          if (cellCoverTurnsLeft > 1) {
            for (int row = 0; row < CAVE_HEIGHT; ++row) {
              for (int i = 0; i < 3; ++i) {
                cellCover[row][rand()%CAVE_WIDTH] = false;
              }
            }
            playSound(SND_UPDATE_CELL_COVER);
          } else if (cellCoverTurnsLeft == 1) {
            pauseTurnsLeft = COVER_PAUSE;
          } else if (cellCoverTurnsLeft == 0) {
            for (int row = 0; row < CAVE_HEIGHT; ++row) {
              for (int col = 0; col < CAVE_WIDTH; ++col) {
                cellCover[row][col] = false;
              }
            }
          }
        } else {
          //
          // Before cave scanning
          //

          ++turnsSinceRockfordSeenAlive;

          /////

          totalAmoebaFoundLastTurn = numberOfAmoebaFoundThisTurn;
          numberOfAmoebaFoundThisTurn = 0;

          amoebaSuffocatedLastTurn = !atLeastOneAmoebaFoundThisTurnWhichCanGrow;
          atLeastOneAmoebaFoundThisTurnWhichCanGrow = false;

          //
          // Scan cave
          //

          for (int row = 0; row < CAVE_HEIGHT; ++row) {
            for (int col = 0; col < CAVE_WIDTH; ++col) {
              switch (map[row][col]) {
                case OBJ_PRE_ROCKFORD_1:
                  turnsSinceRockfordSeenAlive = 0;
                  if (rockfordTurnsTillBirth == 0) {
                    map[row][col] = OBJ_PRE_ROCKFORD_2;
                  } else if (cellCoverTurnsLeft == 0) {
                    rockfordTurnsTillBirth--;
                  }
                  break;

                case OBJ_PRE_ROCKFORD_2:
                  turnsSinceRockfordSeenAlive = 0;
                  map[row][col] = OBJ_PRE_ROCKFORD_3;
                  break;

                case OBJ_PRE_ROCKFORD_3:
                  turnsSinceRockfordSeenAlive = 0;
                  map[row][col] = OBJ_PRE_ROCKFORD_4;
                  break;

                case OBJ_PRE_ROCKFORD_4:
                  turnsSinceRockfordSeenAlive = 0;
                  map[row][col] = OBJ_ROCKFORD;
                  playSound(SND_ROCKFORD_BIRTH);
                  break;

                  //
                  // Update Rockford
                  //

                case OBJ_ROCKFORD: {
                  turnsSinceRockfordSeenAlive = 0;

                  int newRow = row;
                  int newCol = col;

                  rockfordIsMoving = false;

                  if (!isOutOfTime && tileCoverTicksLeft == 0) {
                    if (isKeyDown(KEY_RIGHT)) {
                      rockfordIsMoving = true;
                      rockfordIsFacingRight = true;
                      ++newCol;
                    } else if (isKeyDown(KEY_LEFT)) {
                      rockfordIsMoving = true;
                      rockfordIsFacingRight = false;
                      --newCol;
                    } else if (isKeyDown(KEY_DOWN)) {
                      rockfordIsMoving = true;
                      ++newRow;
                    } else if (isKeyDown(KEY_UP)) {
                      rockfordIsMoving = true;
                      --newRow;
                    }
                  }

                  bool actuallyMoved = false;

                  switch (map[newRow][newCol]) {
                    case OBJ_SPACE:
                      actuallyMoved = true;
                      playSound(SND_ROCKFORD_MOVE_SPACE);
                      break;

                    case OBJ_DIRT:
                      actuallyMoved = true;
                      playSound(SND_ROCKFORD_MOVE_DIRT);
                      break;

                    case OBJ_DIAMOND_STATIONARY:
                    case OBJ_DIAMOND_STATIONARY_SCANNED:
                      //
                      // Pick up a diamond
                      //

                      actuallyMoved = true;
                      addScore(currentDiamondValue);
                      playSound(SND_DIAMOND);

                      // Check if all the needed diamonds for this cave were collected
                      ++diamondsCollected;
                      if (diamondsCollected == caveInfo->diamondsNeeded[difficultyLevel]) {
                        currentDiamondValue = caveInfo->extraDiamondValue;
                        borderColor = flashBorderColor;
                      }
                      break;

                    case OBJ_BOULDER_STATIONARY:
                    case OBJ_BOULDER_STATIONARY_SCANNED:
                      // Pushing boulders
                      if (rand() % 4 == 0) {
                        if (isKeyDown(KEY_RIGHT) && map[newRow][newCol+1] == OBJ_SPACE) {
                          map[newRow][newCol+1] = OBJ_BOULDER_STATIONARY_SCANNED;
                          actuallyMoved = true;
                          playSound(SND_BOULDER);
                        } else if (isKeyDown(KEY_LEFT) && map[newRow][newCol-1] == OBJ_SPACE) {
                          map[newRow][newCol-1] = OBJ_BOULDER_STATIONARY_SCANNED;
                          actuallyMoved = true;
                          playSound(SND_BOULDER);
                        }
                      }
                      break;

                    case OBJ_FLASHING_OUTBOX:
                      actuallyMoved = true;
                      isAddingTimeToScore = true;
                      break;
                  }

                  if (actuallyMoved) {
                    if (isKeyDown(KEY_FIRE)) {
                      map[newRow][newCol] = OBJ_SPACE;
                    } else {
                      map[row][col] = OBJ_SPACE;
                      map[newRow][newCol] = OBJ_ROCKFORD_SCANNED;
                      rockfordRow = newRow;
                      rockfordCol = newCol;
                    }
                  }

                  //
                  // Update Rockford idle animation
                  //

                  if (rockfordIsMoving) {
                    rockfordIsBlinking = false;
                    rockfordIsTapping = false;
                  } else {
                    if (tick % 8 == 0) {
                      rockfordIsBlinking = rand() % 4 == 0;
                      if (rand() % 16 == 0) {
                        rockfordIsTapping = !rockfordIsTapping;
                      }
                    }
                  }
                  break;
                }

                  //
                  // Update boulders and diamonds
                  //

                case OBJ_BOULDER_STATIONARY:
                case OBJ_BOULDER_FALLING:
                  updateBoulderAndDiamond(row, col, map[row][col] == OBJ_BOULDER_FALLING, true);
                  break;

                case OBJ_DIAMOND_STATIONARY:
                case OBJ_DIAMOND_FALLING:
                  updateBoulderAndDiamond(row, col, map[row][col] == OBJ_DIAMOND_FALLING, false);
                  break;

                  //
                  // Update explosion
                  //

                case OBJ_EXPLODE_TO_SPACE_0: map[row][col] = OBJ_EXPLODE_TO_SPACE_1; break;
                case OBJ_EXPLODE_TO_SPACE_1: map[row][col] = OBJ_EXPLODE_TO_SPACE_2; break;
                case OBJ_EXPLODE_TO_SPACE_2: map[row][col] = OBJ_EXPLODE_TO_SPACE_3; break;
                case OBJ_EXPLODE_TO_SPACE_3: map[row][col] = OBJ_EXPLODE_TO_SPACE_4; break;
                case OBJ_EXPLODE_TO_SPACE_4: map[row][col] = OBJ_SPACE; break;

                case OBJ_EXPLODE_TO_DIAMOND_0: map[row][col] = OBJ_EXPLODE_TO_DIAMOND_1; break;
                case OBJ_EXPLODE_TO_DIAMOND_1: map[row][col] = OBJ_EXPLODE_TO_DIAMOND_2; break;
                case OBJ_EXPLODE_TO_DIAMOND_2: map[row][col] = OBJ_EXPLODE_TO_DIAMOND_3; break;
                case OBJ_EXPLODE_TO_DIAMOND_3: map[row][col] = OBJ_EXPLODE_TO_DIAMOND_4; break;
                case OBJ_EXPLODE_TO_DIAMOND_4: map[row][col] = OBJ_DIAMOND_STATIONARY; break;

                  //
                  // Update out box
                  //

                case OBJ_PRE_OUTBOX:
                  if (diamondsCollected >= caveInfo->diamondsNeeded[difficultyLevel]) {
                    map[row][col] = OBJ_FLASHING_OUTBOX;
                  }
                  break;

                  //
                  // Update fireflies and butterflies
                  //

                case OBJ_FIREFLY_LEFT:
                case OBJ_FIREFLY_UP:
                case OBJ_FIREFLY_RIGHT:
                case OBJ_FIREFLY_DOWN:
                  updateFly(row, col, true);
                  break;

                case OBJ_BUTTERFLY_LEFT:
                case OBJ_BUTTERFLY_UP:
                case OBJ_BUTTERFLY_RIGHT:
                case OBJ_BUTTERFLY_DOWN:
                  updateFly(row, col, false);
                  break;

                  //
                  // Update amoeba
                  //

                case OBJ_AMOEBA:
                  ++numberOfAmoebaFoundThisTurn;
                  if (totalAmoebaFoundLastTurn >= TOO_MANY_AMOEBA) {
                    map[row][col] = OBJ_BOULDER_STATIONARY;
                  } else if (amoebaSuffocatedLastTurn) {
                    map[row][col] = OBJ_DIAMOND_STATIONARY;
                  } else {
                    if (!atLeastOneAmoebaFoundThisTurnWhichCanGrow) {
                      atLeastOneAmoebaFoundThisTurnWhichCanGrow =
                        canAmoebaGrowHere(row-1, col) ||
                        canAmoebaGrowHere(row+1, col) ||
                        canAmoebaGrowHere(row, col-1) ||
                        canAmoebaGrowHere(row, col+1);
                    }
                    int amoebaRandomFactor = amoebaSlowGrowthTimeLeft > 0 ? AMOEBA_FACTOR_SLOW : AMOEBA_FACTOR_FAST;
                    if ((rand() % amoebaRandomFactor) < 4) {
                      int newRow, newCol;
                      getRandomCellNear(row, col, &newRow, &newCol);
                      if (canAmoebaGrowHere(newRow, newCol)) {
                        map[newRow][newCol] = OBJ_AMOEBA;
                      }
                    }
                  }
                  playSound(SND_AMOEBA);
                  break;
              }
            }
          }

          //
          // Remove scanned status for cells
          //

          for (int row = 0; row < CAVE_HEIGHT; ++row) {
            for (int col = 0; col < CAVE_WIDTH; ++col) {
              switch (map[row][col]) {
                case OBJ_FIREFLY_LEFT_SCANNED:       map[row][col] = OBJ_FIREFLY_LEFT;       break;
                case OBJ_FIREFLY_UP_SCANNED:         map[row][col] = OBJ_FIREFLY_UP;         break;
                case OBJ_FIREFLY_RIGHT_SCANNED:      map[row][col] = OBJ_FIREFLY_RIGHT;      break;
                case OBJ_FIREFLY_DOWN_SCANNED:       map[row][col] = OBJ_FIREFLY_DOWN;       break;
                case OBJ_BOULDER_STATIONARY_SCANNED: map[row][col] = OBJ_BOULDER_STATIONARY; break;
                case OBJ_BOULDER_FALLING_SCANNED:    map[row][col] = OBJ_BOULDER_FALLING;    break;
                case OBJ_DIAMOND_STATIONARY_SCANNED: map[row][col] = OBJ_DIAMOND_STATIONARY; break;
                case OBJ_DIAMOND_FALLING_SCANNED:    map[row][col] = OBJ_DIAMOND_FALLING;    break;
                case OBJ_BUTTERFLY_DOWN_SCANNED:     map[row][col] = OBJ_BUTTERFLY_DOWN;     break;
                case OBJ_BUTTERFLY_LEFT_SCANNED:     map[row][col] = OBJ_BUTTERFLY_LEFT;     break;
                case OBJ_BUTTERFLY_UP_SCANNED:       map[row][col] = OBJ_BUTTERFLY_UP;       break;
                case OBJ_BUTTERFLY_RIGHT_SCANNED:    map[row][col] = OBJ_BUTTERFLY_RIGHT;    break;
                case OBJ_ROCKFORD_SCANNED:           map[row][col] = OBJ_ROCKFORD;           break;
                case OBJ_AMOEBA_SCANNED:             map[row][col] = OBJ_AMOEBA;             break;
              }
            }
          }

          //
          // Handle failure
          //

          if (tileCoverTicksLeft == 0 && rockfordTurnsTillBirth == 0 &&
              ((isFailed() && isKeyDown(KEY_FIRE)) || isKeyDown(KEY_FAIL))) {
            tileCoverTicksLeft = TILE_COVER_TICKS;
            if (isIntermission()) {
              incrementCaveNumber();
            } else {
              --livesLeft;
            }
          }
        }
      }
    }

    //
    // Update tile cover
    //

    if (tileCoverTicksLeft > 0) {
      --tileCoverTicksLeft;

      if (tileCoverTicksLeft == 0) {
        if (livesLeft == 0) {
          for (int row = 0; row < PLAYFIELD_HEIGHT_IN_TILES; ++row) {
            for (int col = 0; col < PLAYFIELD_WIDTH_IN_TILES; ++col) {
              tileCover[row][col] = true;
            }
          }
          turnsTillGameRestart = TURNS_TILL_GAME_RESTART;
        } else {
          pauseTurnsLeft = COVER_PAUSE;
          isCaveStart = true;
        }
      } else {
        for (int i = 0; i < 7; ++i) {
          int row = rand() % PLAYFIELD_HEIGHT_IN_TILES;
          int col = rand() % PLAYFIELD_WIDTH_IN_TILES;
          tileCover[row][col] = true;
        }
        playSound(SND_UPDATE_TILE_COVER);
      }
    }
  }
}

/** @brief GameAPI draw callback. */
static void boulder_draw(void) {
  //
  // Update status bar
  //

  int p = 0;
  if (livesLeft == 0) {
    append(statusBarText, &p, "        G A M E  O V E R");
  } else if (isOutOfTimeTextShown && tileCoverTicksLeft == 0) {
    append(statusBarText, &p, "     O U T   O F   T I M E");
  } else {
    if (rockfordTurnsTillBirth > 0 || tileCoverTicksLeft > 0 || isCaveStart) {
      if (isIntermission()) {
        append(statusBarText, &p, "       B O N U S  L I F E");
      } else {
        append(statusBarText, &p, "  PLAYER 1,  ");
        append_uint_padded(statusBarText, &p, livesLeft, 1);
        append(statusBarText, &p, " MEN,  ROOM ");
        statusBarText[p++] = getCurrentCaveLetter();
        statusBarText[p++] = '/';
        append_uint_padded(statusBarText, &p, difficultyLevel+1, 1);
      }
    } else {
      if (diamondsCollected < caveInfo->diamondsNeeded[difficultyLevel]) {
        append(statusBarText, &p, "   ");
        append_uint_padded(statusBarText, &p, caveInfo->diamondsNeeded[difficultyLevel], 2);
        statusBarText[p++] = '*';
        append_uint_padded(statusBarText, &p, currentDiamondValue, 2);
        append(statusBarText, &p, "   ");
        append_uint_padded(statusBarText, &p, diamondsCollected, 2);
        append(statusBarText, &p, "   ");
        append_uint_padded(statusBarText, &p, caveTimeLeft, 3);
        append(statusBarText, &p, "   ");
        append_uint_padded(statusBarText, &p, score, 6);
      } else {
        append(statusBarText, &p, "   ***");
        append_uint_padded(statusBarText, &p, currentDiamondValue, 2);
        append(statusBarText, &p, "   ");
        append_uint_padded(statusBarText, &p, diamondsCollected, 2);
        append(statusBarText, &p, "   ");
        append_uint_padded(statusBarText, &p, caveTimeLeft, 3);
        append(statusBarText, &p, "   ");
        append_uint_padded(statusBarText, &p, score, 6);
      }
    }
  }
  statusBarText[p] = 0;

  //
  // Render
  //

  // Draw border - only the margin outside the viewport. The viewport
  // itself gets fully overdrawn by the cave/status-bar drawing below, so
  // filling it here too was pure wasted per-pixel work every frame (the
  // STM32F411 at 100MHz feels ~45000 pointless setPixel() calls/frame in
  // a way a modern PC never would - this was the biggest single
  // contributor to the occasional frame hitches on real hardware).
  drawFilledRect(0, 0, BACKBUFFER_WIDTH - 1, VIEWPORT_TOP - 1, borderColor);
  drawFilledRect(0, VIEWPORT_BOTTOM + 1, BACKBUFFER_WIDTH - 1, BACKBUFFER_HEIGHT - 1, borderColor);
  drawFilledRect(0, VIEWPORT_TOP, VIEWPORT_LEFT - 1, VIEWPORT_BOTTOM, borderColor);
  drawFilledRect(VIEWPORT_RIGHT + 1, VIEWPORT_TOP, BACKBUFFER_WIDTH - 1, VIEWPORT_BOTTOM, borderColor);

  // Draw cave - only the cells that can possibly be visible through the
  // camera, not the whole 40x22 grid (of which at most ~16x11 cells ever
  // show at once). drawSprite() already clips per-tile, so this is just
  // cutting the ~700 always-off-screen cells' switch()-dispatch/function-
  // call overhead, not a visibility fix - a 1-cell margin covers camera
  // positions that aren't an exact multiple of CELL_SIZE.
  int colStart = cameraX / CELL_SIZE - 1;
  int colEnd   = (cameraX + PLAYFIELD_WIDTH) / CELL_SIZE + 1;
  int rowStart = cameraY / CELL_SIZE - 1;
  int rowEnd   = (cameraY + PLAYFIELD_HEIGHT) / CELL_SIZE + 1;
  if (colStart < 0) colStart = 0;
  if (rowStart < 0) rowStart = 0;
  if (colEnd >= CAVE_WIDTH) colEnd = CAVE_WIDTH - 1;
  if (rowEnd >= CAVE_HEIGHT) rowEnd = CAVE_HEIGHT - 1;

  for (int row = rowStart; row <= rowEnd; ++row) {
    for (int col = colStart; col <= colEnd; ++col) {
      int x = PLAYFIELD_LEFT + col*CELL_SIZE - cameraX;
      int y = PLAYFIELD_TOP + row*CELL_SIZE - cameraY;

      if (cellCover[row][col]) {
        drawSprite(spriteSteelWall, 0, x, y, curColors.boulderFg, BLACK, turn);
      } else {
        switch (map[row][col]) {
          case OBJ_SPACE:
            if (spaceFlashingTurnsLeft > 0 && !isAddingTimeToScore && turnsTillExitingCave == 0) {
              drawSprite(spriteSpaceFlash, turn, x, y, WHITE, BLACK, 0);
            } else {
              drawSprite(spriteSpace, 0, x, y, BLACK, BLACK, 0);
            }
            break;

          case OBJ_STEEL_WALL:
          case OBJ_PRE_OUTBOX:
            drawSprite(spriteSteelWall, 0, x, y, curColors.boulderFg, BLACK, 0);
            break;

          case OBJ_FLASHING_OUTBOX:
            if (turn % 2 == 0) {
              drawSprite(spriteOutbox, 0, x, y, curColors.boulderFg, BLACK, 0);
            } else {
              drawSprite(spriteSteelWall, 0, x, y, curColors.boulderFg, BLACK, 0);
            }
            break;

          case OBJ_DIRT:
            drawSprite(spriteDirt, 0, x, y, curColors.dirtFg, BLACK, 0);
            break;

          case OBJ_BRICK_WALL:
            drawSprite(spriteBrickWall, 0, x, y, curColors.brickWallFg, curColors.brickWallBg, 0);
            break;

          case OBJ_MAGIC_WALL: {
            int frame = (magicWallStatus == MAGIC_WALL_ON) ? turn : 0;
            drawSprite(spriteBrickWall, frame, x, y, curColors.brickWallFg, curColors.brickWallBg, 0);
            break;
          }

          case OBJ_BOULDER_STATIONARY:
          case OBJ_BOULDER_FALLING:
            drawSprite(spriteBoulder, 0, x, y, curColors.boulderFg, BLACK, 0);
            break;

          case OBJ_DIAMOND_STATIONARY:
          case OBJ_DIAMOND_FALLING:
            drawSprite(spriteDiamond, turn, x, y, WHITE, BLACK, 0);
            break;

          case OBJ_FIREFLY_LEFT:
          case OBJ_FIREFLY_UP:
          case OBJ_FIREFLY_RIGHT:
          case OBJ_FIREFLY_DOWN:
            drawSprite(spriteFirefly, turn, x, y, curColors.flyFg, curColors.flyBg, 0);
            break;

          case OBJ_BUTTERFLY_LEFT:
          case OBJ_BUTTERFLY_UP:
          case OBJ_BUTTERFLY_RIGHT:
          case OBJ_BUTTERFLY_DOWN:
            drawSprite(spriteButterfly, turn, x, y, curColors.flyFg, curColors.flyBg, 0);
            break;

            //
            // Draw Rockford birth
            //

          case OBJ_PRE_ROCKFORD_1:
            if (rockfordTurnsTillBirth > 0) {
              if (rockfordTurnsTillBirth % 2) {
                drawSprite(spriteSteelWall, 0, x, y, curColors.boulderFg, BLACK, 0);
              } else {
                drawSprite(spriteOutbox, 0, x, y, curColors.boulderFg, BLACK, 0);
              }
            } else {
              drawSprite(spriteExplosion, 0, x, y, WHITE, BLACK, 0);
            }
            break;
          case OBJ_PRE_ROCKFORD_2:
            drawSprite(spriteExplosion, 1, x, y, WHITE, BLACK, 0);
            break;
          case OBJ_PRE_ROCKFORD_3:
            drawSprite(spriteExplosion, 2, x, y, WHITE, BLACK, 0);
            break;
          case OBJ_PRE_ROCKFORD_4:
            drawSprite(spriteRockfordRight, turn, x, y, GRAY, BLACK, 0);
            break;

            //
            // Draw rockford
            //

          case OBJ_ROCKFORD:
            if (rockfordIsMoving) {
              if (rockfordIsFacingRight) {
                drawSprite(spriteRockfordRight, tick, x, y, GRAY, BLACK, 0);
              } else {
                drawSprite(spriteRockfordLeft, tick, x, y, GRAY, BLACK, 0);
              }
            } else if (rockfordIsBlinking && rockfordIsTapping) {
              drawSprite(spriteRockfordBlinkTap, tick, x, y, GRAY, BLACK, 0);
            } else if (rockfordIsBlinking) {
              drawSprite(spriteRockfordBlink, tick, x, y, GRAY, BLACK, 0);
            } else if (rockfordIsTapping) {
              drawSprite(spriteRockfordTap, tick, x, y, GRAY, BLACK, 0);
            } else {
              drawSprite(spriteRockfordIdle, 0, x, y, GRAY, BLACK, 0);
            }
            break;

            //
            // Draw explosion
            //

          case OBJ_EXPLODE_TO_SPACE_1:
          case OBJ_EXPLODE_TO_DIAMOND_1:
            drawSprite(spriteExplosion, 1, x, y, WHITE, BLACK, 0);
            break;
          case OBJ_EXPLODE_TO_SPACE_2:
          case OBJ_EXPLODE_TO_DIAMOND_2:
            drawSprite(spriteExplosion, 2, x, y, WHITE, BLACK, 0);
            break;
          case OBJ_EXPLODE_TO_SPACE_3:
          case OBJ_EXPLODE_TO_DIAMOND_3:
            drawSprite(spriteExplosion, 1, x, y, WHITE, BLACK, 0);
            break;
          case OBJ_EXPLODE_TO_SPACE_4:
          case OBJ_EXPLODE_TO_DIAMOND_4:
            drawSprite(spriteExplosion, 0, x, y, WHITE, BLACK, 0);
            break;

          case OBJ_AMOEBA:
            drawSprite(spriteAmoeba, turn, x, y, GREEN, BLACK, 0);
            break;
        }
      }
    }
  }

  //
  // Draw tile cover
  //

  for (int row = 0; row < PLAYFIELD_HEIGHT_IN_TILES; ++row) {
    for (int col = 0; col < PLAYFIELD_WIDTH_IN_TILES; ++col) {
      if (tileCover[row][col]) {
        int x = PLAYFIELD_LEFT + col*TILE_SIZE;
        int y = PLAYFIELD_TOP + row*TILE_SIZE;
        drawSprite(spriteSteelWallTile, 0, x, y, curColors.boulderFg, BLACK, turn);
      }
    }
  }

  //
  // Draw status bar
  //

  drawFilledRect(VIEWPORT_LEFT, VIEWPORT_TOP, VIEWPORT_RIGHT, VIEWPORT_TOP+STATUS_BAR_HEIGHT, BLACK);
  draw_text(SCREEN_OFFSET_X + VIEWPORT_LEFT, SCREEN_OFFSET_Y + VIEWPORT_TOP + TILE_SIZE,
            statusBarText, boulderPalette[GRAY]);
}

static const GameAPI boulderdash_game = {
    .name   = "boulderdash",
    .init   = boulder_init,
    .update = boulder_update,
    .draw   = boulder_draw,
};

/** @brief Entry point: runs Boulder Dash via the generic GameAPI loop. */
int main(void) {
  game_run(&boulderdash_game, 34);
  return 0;
}
