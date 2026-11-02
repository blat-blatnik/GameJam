#include "raylib.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void GameLoop()
{
	if (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_ALT))
	{
		ToggleBorderlessWindowed();
	}

	BeginDrawing();
	{
		ClearBackground(RAYWHITE);
		DrawText("Hello :)", 100, 100, 64, RED);
		DrawFPS(10, 10);
	}
	EndDrawing();
}

int main()
{
	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(1280, 720, "Jammin");

	// The browser wants to control the animation loop on the web.
	#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(GameLoop, 0, false);
	#else
	while (not WindowShouldClose()) GameLoop();
	#endif
}
