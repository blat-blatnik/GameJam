#include "utils.h"
#define RAYGUI_IMPLEMENTATION
#pragma warning(disable:4996)
#include "raygui.h"
void GameLoop()
{
	if (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_ALT))
		ToggleFullscreen_FixedForWeb();

	BeginDrawing();
	{
		ClearBackground(RAYWHITE);
		DrawText("Hello :)", 100, 100, 64, RED);
		DrawFPS(10, 10);
	static bool showMessageBox = false;
		if (GuiButton({ 24, 24, 120, 30 }, "#191#Show Message")) showMessageBox = true;

		if (showMessageBox)
		{
			int btnActive = -1;
			GuiMessageBox({ 85, 70, 250, 100 },
				"#191#Message Box", "Hi! This is a message!", "Nice;Cool", &btnActive);

			if (btnActive >= 0) showMessageBox = false;
		}
		Vector2 pos = GetMousePosition();
		DrawCircleV(pos, 20, BLUE);
	}
	EndDrawing();
}

int main()
{
	// Typically working directory on windows starts as the EXE directory, but during dev that's in bin/Debug/Game.exe, and it could change.
	// Better to always start from project root directory.
	#if DEV_MODE
	{
		ChangeDirectory(GetApplicationDirectory());
		for (int attempt = 0; attempt < 3 and not DirectoryExists("art"); attempt++)
			ChangeDirectory("..");
	}
	#endif

	// Initialize raylib.
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(1280, 720, "Jammin");

	// Load and apply previous window placement.
	struct WindowPlacement
	{
		Rectangle rectangle;
		bool maximized;
	};
	int fileSize;
	WindowPlacement* prevPlacement = (WindowPlacement*)LoadFileData("window.bin", &fileSize);
	if (prevPlacement)
	{
		SetWindowPosition((int)prevPlacement->rectangle.x, (int)prevPlacement->rectangle.y);
		SetWindowSize((int)prevPlacement->rectangle.width, (int)prevPlacement->rectangle.height);
		if (prevPlacement->maximized) MaximizeWindow();
		MemFree(prevPlacement);
	}

	// The browser wants to control the animation loop on the web.
	#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(GameLoop, 0, false);
	#else
	while (not WindowShouldClose()) GameLoop();
	#endif

	// Save window placement for next run.
	WindowPlacement placement = {};
	placement.rectangle.x = GetWindowPosition().x;
	placement.rectangle.y = GetWindowPosition().y;
	placement.rectangle.width = (float)GetScreenWidth(); // This is window width, not screen width. Poor naming by Raylib .
	placement.rectangle.height = (float)GetScreenHeight();
	placement.maximized = IsWindowMaximized();
	SaveFileData("window.bin", &placement, sizeof placement);
}
