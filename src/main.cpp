#include "raylib.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void GameLoop()
{
	BeginDrawing();
	{
		ClearBackground(RAYWHITE);
		DrawText("Hello :)", 100, 100, 64, RED);
	}
	EndDrawing();
}

int main()
{
	InitWindow(1280, 720, "Jammin");
	SetTargetFPS(60);

	#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(GameLoop, 0, false);
	#else
	while (not WindowShouldClose())
	{
		GameLoop();
	}
	#endif
}
