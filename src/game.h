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


struct GameState
{
    Player player;
    Grid grid;
};

static Grid initializeGrid(uint32_t width, uint32_t height,uint32_t layers)
{
    Grid grid{};
    uint64_t gridSize =width * height * layers * sizeof(GridTile);
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

inline GridTile getGridTile(Grid* grid, uint32_t x, uint32_t y,uint32_t layer)
{
    GridTile result=GridTile_None;
    if (grid != nullptr)
    {
        if (x<grid->width && y<grid->height && layer<grid->layers)
        {
            result = grid->tiles[x+y*grid->width+ layer * grid->width*grid->height] ;
        }
        
    }
    return result;
}
inline bool setGridTile(Grid* grid,  uint32_t x, uint32_t y,uint32_t layer, GridTile tile)
{
    bool result=true;
    if (grid != nullptr)
    {
        if (x<grid->width && y<grid->height)
        {
            grid->tiles[x+y*grid->width+ layer*grid->width*grid->height]= tile;
        }else
        {
            result= false;
        }
    }else
    {
        result= false;
    }
    return result;
}

static bool initializeGameState(GameState* gameState)
{
    bool success = gameState != nullptr;
    if (success)
    {
        gameState->grid=initializeGrid(480/16+1, 270/16+1,2);
        setGridTile(&gameState->grid, 6, 9,0, GridTile_Floor);
        setGridTile(&gameState->grid, 8, 2,0, GridTile_Floor);
        setGridTile(&gameState->grid, 4, 3,0, GridTile_Floor);
    }
    return success;
}
