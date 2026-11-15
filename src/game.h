#pragma once
#include <cstdint>


typedef uint32_t GridTile;

enum Facing
{
	Direction_Right = 0,
	Direction_Up = 1,
	Direction_Left = 2,
	Direction_Down = 3,
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
	Facing direction;
};
struct Event
{
	uint32_t cellX;
	uint32_t cellY;
	char scriptName[128];
};

struct Level
{
	Vector2 startingPosition;
	Grid grid;
	uint32_t eventsCount;
	Event events[32];
};

struct GameState
{
	Player player;
	uint8_t* collision;
	Level level;
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
		if (fileSize >= sizeof(Grid))
		{
			Level *level = (Level*) data;
			if (fileSize == sizeof(Level) + level->grid.width * level->grid.height * level->grid.layers *
				sizeof(GridTile))
			{
				level->grid.tiles = (GridTile*)(data + sizeof(Level));
				gameState->level = *level;
				gameState->player.position = gameState->level.startingPosition;
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

