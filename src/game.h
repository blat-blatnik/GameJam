#pragma once
#include "utils.h"

typedef uint32_t GridTile;

#define PLAYER_SPEED 40.0f
#define PLAYER_INVINCIBILITY_TIME 0.5f
#define PLAYER_ATTACK_WINDUP_TIME 0.10f
#define PLAYER_ATTACK_DAMAGE_TIME 0.40f
#define GHOST_SPEED 25.0f
#define GHOST_ATTACK_WINDUP_TIME 0.40f
#define GHOST_ATTACK_DAMAGE_TIME 0.50f
#define GHOST_ATTACK_WINDDOWN_TIME 0.20f
#define GHOST_ATTACK_COOLDOWN_TIME 0.50f
#define GHOST_ATTACK_RADIUS 30.0f
#define GHOST_ATTACK_DAMAGE 15

enum GameProgress
{
	GameProgress_JustWokeUp = 0,
	GameProgress_GotCat     = 1,
	GameProgress_GetTowel   = 2,
	GameProgress_GotTowel   = 3,
	GameProgress_GetTuna    = 4,
	GameProgress_GotTuna    = 5,
	GameProgress_GoSleep    = 6,
	GameProgress_SecondDay  = 7,
	GameProgress_EnumCount
};

struct Grid
{
	uint32_t width;
	uint32_t height;
	uint32_t layers;
	uint32_t pad;
	GridTile* tiles;
};

struct Player
{
	Vector2 position;
	Facing facing;
	float health = 100; // [0,100]
	float invicibilityTime; // >0 after being hit, while >0 no damage can be taken.
	float attackTime; // >0 means player is in attacking state
	unsigned attackSequenceCounter = 1; // used so enemies don't get damaged twice by the same attack
	double animationFrame = 0;
};
struct Event
{
	uint32_t cellX;
	uint32_t cellY;
	char scriptName[128];
};
struct Ghost
{
	Vector2 position;
	Facing facing;
	float health = 3;
	float attackTime; // >0 means ghost is in attacking state
	float attackCooldownTime; // >0 means ghost can't attack yet
	unsigned lastAttackSequenceCounter;
};

struct Level
{
	Vector2 startingPosition;
	Grid grid;
	uint32_t nextUnused;
	Event events[32];
	Vector2 ghostPositions[16];
};

struct GameState
{
	Player player;
	uint8_t* collision;
	Level level;
	int numGhosts;
	Ghost ghosts[16];
	GameProgress progress = GameProgress_JustWokeUp;
};

static Grid initializeGrid(uint32_t width, uint32_t height, uint32_t layers)
{
	Grid grid{};
	uint64_t gridSize = width * height * layers * sizeof(GridTile);
	grid.tiles = (GridTile*)malloc(gridSize);
	memset(grid.tiles, 0, gridSize);
	if (grid.tiles != nullptr)
	{
		grid.width = width;
		grid.height = height;
		grid.layers = layers;
	}
	return grid;
}

inline GridTile getGridTile(Grid* grid, uint32_t x, uint32_t y, uint32_t layer)
{
	GridTile result = 0;
	if (grid != nullptr)
	{
		if (x < grid->width && y < grid->height && layer < grid->layers)
		{
			result = grid->tiles[x + y * grid->width + layer * grid->width * grid->height];
		}
        
	}
	return result;
}

inline uint32_t getGridSize(Grid* grid)
{
	return grid->width * grid->height * grid->layers * sizeof(GridTile);
}

inline bool setGridTile(Grid* grid, uint32_t x, uint32_t y, uint32_t layer, GridTile tile)
{
	bool result = true;
	if (grid != nullptr)
	{
		if (x < grid->width && y < grid->height)
		{
			grid->tiles[x + y * grid->width + layer * grid->width * grid->height] = tile;
		}
		else
		{
			result = false;
		}
	}
	else
	{
		result = false;
	}
	return result;
}

static bool saveCollision(const char* fileName, uint8_t* collision, uint32_t size)
{
	return SaveFileData(TextFormat("art/%s", fileName), collision, size);
}

static bool loadCollision(const char* fileName, Texture* texture, uint8_t** collision)
{
	bool success = true;
	if (FileExists(TextFormat("art/%s", fileName)))
	{
		int fileSize = 0;
		uint8_t* data = LoadFileData(TextFormat("art/%s", fileName), &fileSize);
		if (fileSize == (texture->width / 16) * (texture->height / 16))
		{
			*collision = data;
		}
	}
	else
	{
		success = false;
	}
	return success;
}


static bool saveLevel(const char* levelName,Level* level)
{
	const char* filePath = TextFormat("art/%s", levelName);
	FILE* file = fopen(filePath, "wb");
	bool success = file != nullptr;
	fwrite(level, sizeof(Level), 1, file);
	fwrite(level->grid.tiles, getGridSize(&level->grid), 1, file);
	fclose(file);
	return success;
}

static bool loadLevel(const char* fileName, GameState* gameState)
{
	const char* filePath = TextFormat("art/%s", fileName);
	bool result = true;
	if (FileExists(filePath))
	{
		int fileSize = 0;
		uint8_t* data = LoadFileData(filePath, &fileSize);
		if (fileSize >= sizeof(Level))
		{
			Level *level = (Level*) data;
			if (fileSize == sizeof(Level) + level->grid.width * level->grid.height * level->grid.layers *
				sizeof(GridTile))
			{
				level->grid.tiles = (GridTile*)(data + sizeof(Level));
				gameState->level = *level;
				gameState->player.position = gameState->level.startingPosition;
					gameState->numGhosts =0;
				for (uint32_t i = 0; i < arrLength(Level::ghostPositions);i++)
				{
				if (Vector2LengthSqr(level->ghostPositions[i])!=0.f)
				{
					gameState->ghosts[i]=Ghost{};
					gameState->ghosts[i].position = level->ghostPositions[i];
					gameState->numGhosts++;
				}
					
				}
			}
			else
			{
				result = false;
			}
		}
		else
		{
			result = false;
		}
	}
	else
	{
		result = false;
	}
	return result;
}

