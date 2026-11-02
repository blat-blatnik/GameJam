#include "raylib.h"

int main()
{
	InitWindow(1280, 720, "Jammin");
	SetTargetFPS(60);
	while (not WindowShouldClose())
	{
		if (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_ALT) )
		{
			ToggleBorderlessWindowed();
		}
		
		BeginDrawing();
		{
			ClearBackground(RAYWHITE);
			DrawText("Hello :)", 100, 100, 64, RED);
		}
		EndDrawing();

		
	}
}
