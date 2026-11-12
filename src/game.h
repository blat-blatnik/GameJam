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

static Grid initializeGrid(uint32_t width, uint32_t height)
{
    Grid grid{};
    grid.tiles = (GridTile*)malloc(width * height * sizeof(GridTile));
    memset(grid.tiles, 0, width * height * sizeof(GridTile));
    if (grid.tiles != nullptr)
    {
        grid.width = width;
        grid.height = height;
    }
    return grid;
}

inline GridTile getGridTile(Grid* grid, uint32_t x, uint32_t y)
{
    GridTile result=GridTile_None;
    if (grid != nullptr)
    {
        if (x<grid->width && y<grid->height)
        {
            result = grid->tiles[x+y*grid->width] ;
        }
        
    }
    return result;
}
inline bool setGridTile(Grid* grid, uint32_t x, uint32_t y, GridTile tile)
{
    bool result=true;
    if (grid != nullptr)
    {
        if (x<grid->width && y<grid->height)
        {
            grid->tiles[x+y*grid->width]= tile;
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
        gameState->grid=initializeGrid(480/16+1, 270/16+1);
        setGridTile(&gameState->grid, 6, 9, GridTile_Floor);
        setGridTile(&gameState->grid, 8, 2, GridTile_Floor);
        setGridTile(&gameState->grid, 4, 3, GridTile_Floor);
    }
    return success;
}
