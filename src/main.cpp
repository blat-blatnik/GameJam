#include "raylib.h"

int main()
{
	InitWindow(1280, 720, "Jammin");
	SetTargetFPS(60);
	while (not WindowShouldClose())
	{
		BeginDrawing();
		{
			ClearBackground(RAYWHITE);
			DrawText("Hello :)", 100, 100, 64, RED);
		}
		EndDrawing();
	}
}
