#include "raylib.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

void ToggleFullscreen_FixedForWeb()
{
	// Raylib's ToggleFullscreen doesn't work on Web for some reason.
	#ifdef __EMSCRIPTEN__
	{
		EmscriptenFullscreenChangeEvent status;
		emscripten_get_fullscreen_status(&status);
		if (not status.isFullscreen)
			emscripten_request_fullscreen("#canvas", true);
		else
			emscripten_exit_fullscreen();
	}
	#else
	ToggleBorderlessWindowed();
	#endif
}

void GameLoop()
{
	if (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_ALT))
		ToggleFullscreen_FixedForWeb();

	BeginDrawing();
	{
		ClearBackground(RAYWHITE);
		DrawText("Hello :)", 100, 100, 64, RED);
		DrawFPS(10, 10);

		Vector2 pos = GetMousePosition();
		DrawCircleV(pos, 20, BLUE);
	}
	EndDrawing();
}

int main()
{
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(1280, 720, "Jammin");

	// The browser wants to control the animation loop on the web.
	#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(GameLoop, 0, false);
	#else
	while (not WindowShouldClose()) GameLoop();
	#endif
}
