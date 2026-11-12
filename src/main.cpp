#include "art.h"
#include "utils.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#define FIXED_WIDTH 480 // 1920/4
#define FIXED_HEIGHT 270
#define FIXED_SIZE Vector2{ FIXED_WIDTH, FIXED_HEIGHT }

RenderTexture FixedRenderTexture; // Game gets rendered to this (SCREEN_WIDTH x SCREEN_HEIGHT) texture, and then upscaled.

void GameLoop()
{
	if (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_ALT))
		ToggleFullscreen_FixedForWeb();

	BeginDrawing();
	{
		ClearBackground(BLACK);

		// Scale the game content to fit the *actual* window size, draw it centered in the window, and add black bars.
		// This way we don't have to worry about actual window size at runtime.
		float windowWidth = (float)GetRenderWidth();
		float windowHeight = (float)GetRenderHeight();
		float maxScaleX = Max(windowWidth / FIXED_WIDTH, 1);
		float maxScaleY = Max(windowHeight / FIXED_HEIGHT, 1);
		float scale = Min(maxScaleX, maxScaleY);
		#if false // Force integer scaling.
		scale = floorf(scale);
		#endif
		float scaledWidth = FIXED_WIDTH * scale;
		float scaledHeight = FIXED_HEIGHT * scale;
		Vector2 scaledSize = {
			FIXED_WIDTH * scale,
			FIXED_HEIGHT * scale,
		};
		float remainingWidth = windowWidth - scaledWidth;
		float remainingHeight = windowHeight - scaledHeight;
		Vector2 innerWindowOffset = {
			remainingWidth / 2,
			remainingHeight / 2,
		};
		Vector2 mousePosInWindow = GetMousePosition();

		// Calculate mouse cursor position inside of the inner fixed window, from (0,0) to FIXED_SIZE.
		Vector2 mousePos = (mousePosInWindow - innerWindowOffset) / scaledSize * FIXED_SIZE;
		
		// Render game content to a fixed size SCREEN_WIDTH x SCREEN_HEIGHT texture.
		BeginTextureMode(FixedRenderTexture);
		{
			ClearBackground(RAYWHITE);
			DrawText("Hello :)", 100, 100, 64, RED);
			DrawTextureV(PlaceholderTexture, mousePos, WHITE);

			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
				PlaySound(PlaceholderSound);
		}
		EndTextureMode();
		
		// Draw game render texture in the center of the screen.
		DrawTexturePro(FixedRenderTexture.texture, { 0, 0, FIXED_WIDTH, -FIXED_HEIGHT }, { innerWindowOffset.x, innerWindowOffset.y, scaledWidth, scaledHeight }, {}, 0, WHITE);

		// Draw GUI on top of everything on the real window coordinates.
		DrawFPS(10, 10);
		static bool showMessageBox = false;
		if (GuiButton({ 24, 48, 120, 30 }, "#191#Show Message")) showMessageBox = true;
		if (showMessageBox)
		{
			int btnActive = -1;
			GuiMessageBox({ 85, 70, 250, 100 },
				"#191#Message Box", "Hi! This is a message!", "Nice;Cool", &btnActive);

			if (btnActive >= 0) showMessageBox = false;
		}
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
	#ifndef __EMSCRIPTEN__
	SetWindowMinSize(FIXED_WIDTH, FIXED_HEIGHT); // This doesn't work on web, IDFK why, but whatever :')
	#endif
	InitWindow(1280, 720, "Jammin");
	SetExitKey(0); // prevent exiting by pressing esc
	InitAudioDevice();

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

	// Create fixed size render target.
	FixedRenderTexture = LoadRenderTexture(FIXED_WIDTH, FIXED_HEIGHT);

	// Load everything upfront.
	LoadAllArt();

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
