#pragma once
#include <cstdint>


enum GridTile: uint32_t
{
	GridTile_None = 0,
	GridTile_Floor,
	GridTile_Wall,
	GridTile_Count
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
	float rotation;
};

struct Level
{
	char name[128];
	Vector2 gridSize;
};

struct GameState
{
	Player player;
	Grid grid;
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
	GridTile result = GridTile_None;
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
	SaveFileData(fileName, collision, size);
}

static bool loadCollision(const char* fileName, Texture* texture, uint8_t** collision, uint32_t size)
{
	bool success = true;
	if (FileExists(fileName))
	{
		int fileSize = 0;
		uint8_t* data = LoadFileData(fileName, &fileSize);
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


static bool saveLevel(const char* fileName, Grid* grid)
{
	const char* filePath = TextFormat("art/%s", fileName);
	FILE* file = fopen(filePath, "wb");
	bool success = file != nullptr;
	fwrite(grid, sizeof(Grid), 1, file);
	fwrite(grid->tiles, getGridSize(grid), 1, file);
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
			gameState->grid = *(Grid*)data;
			if (fileSize == sizeof(Grid) + gameState->grid.width * gameState->grid.height * gameState->grid.layers *
				sizeof(GridTile))
			{
				gameState->grid.tiles = (GridTile*)(data + sizeof(Grid));
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

