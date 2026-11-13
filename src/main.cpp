#include "art.h"
#include "utils.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "game.h"

#define FIXED_WIDTH 480 // 1920/4
#define FIXED_HEIGHT 270

#define FIXED_SIZE Vector2{ FIXED_WIDTH, FIXED_HEIGHT }

#define EDITOR

RenderTexture FixedRenderTexture; // Game gets rendered to this (SCREEN_WIDTH x SCREEN_HEIGHT) texture, and then upscaled.
Vector2 MousePos{}; // Cursor position relative to the inner FIXED_SIZE screen, always within (0,0) .. FIXED_SIZE
GameState gameState{}; // (oualid) idk how to pass variable to emscripting's loop :(

// Dialog script stuff

Script* CurrentScript;
int ScriptParagraphIndex;
float ScriptParagraphTime;
Texture ScriptFadeInOutTexture;

void ExecuteScriptCommand(const char* commandAndParams)
{
	int numSplit;
	char** split = TextSplit(commandAndParams, ' ', &numSplit);
	int numParameters = numSplit - 1;
	assert(numSplit >= 1);

	const char* command = split[0];
	if (TextIsEqual(command, "FadeInOut"))
	{
		assert(numParameters == 1);
		const char* filepath = split[1];
		ScriptFadeInOutTexture = GetTexture(filepath);
	}
	else TraceLog(LOG_ERROR, "Unknown command '%s'", command);
}

// Game

void GameLoop()
{
	if (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_ALT))
		ToggleFullscreen_FixedForWeb();

	// update
	Vector2 inputVelocity{};
	if (IsGamepadAvailable(0))
	{
		inputVelocity.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
		inputVelocity.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
		if (inputVelocity.x > -0.02f && inputVelocity.x < 0.02f)
		{
			inputVelocity.x = 0;
		}
		if (inputVelocity.y > -0.02f && inputVelocity.y < 0.02f)
		{
			inputVelocity.y = 0;
		}
	}

	if (IsKeyDown(KEY_A))
	{
		inputVelocity.x -= 1.0f;
	}
	if (IsKeyDown(KEY_D))
	{
		inputVelocity.x += 1.0f;
	}
	if (IsKeyDown(KEY_W))
	{
		inputVelocity.y -= 1.0f;
	}
	if (IsKeyDown(KEY_S))
	{
		inputVelocity.y += 1.0f;
	}
	inputVelocity = Vector2Normalize(inputVelocity);
	gameState.player.position += inputVelocity * 100.f * GetFrameTime();

		// Scale the game content to fit the *actual* window size, draw it centered in the window, and add black bars.
		// This way we don't have to worry about actual window size at runtime.
		float windowWidth = (float)GetRenderWidth();
		float windowHeight = (float)GetRenderHeight();
		float maxScaleX = Max(windowWidth / FIXED_WIDTH, 1);
		float maxScaleY = Max(windowHeight / FIXED_HEIGHT, 1);
		float scale = Min(maxScaleX, maxScaleY);
		#if true // Force integer scaling.
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

		// Calculate mouse cursor position inside of the inner fixed window, from (0,0) to FIXED_SIZE.
		MousePos = (GetMousePosition() - innerWindowOffset) / scaledSize * FIXED_SIZE;
		MousePos = Vector2Clamp(MousePos, { 0, 0 }, FIXED_SIZE);
		
	BeginDrawing();
	{
		ClearBackground(BLACK);
		// Render game content to a fixed size SCREEN_WIDTH x SCREEN_HEIGHT texture.
		BeginTextureMode(FixedRenderTexture);
		{
			ClearBackground(RAYWHITE);
			//DrawText("Hello :)", 100, 100, 64, RED);

			{// draw level and player

				for (uint32_t y = 0; y < gameState.grid.height; y++){
					for (uint32_t x = 0; x < gameState.grid.width; x++){
						if (gameState.grid.tiles[x+y*gameState.grid.width] == GridTile_Floor)
						{
							DrawTextureRec(GridTest,Rectangle{0,0,16,16},Vector2{x*16.f,y*16.f},WHITE);
						}else
						{
							DrawTextureRec(GridTest,Rectangle{16*3,16*3,16,16},Vector2{x*16.f,y*16.f},WHITE);
							
						}
					}
				}
			
				DrawRectangleV(gameState.player.position, Vector2{16,32},DARKGREEN);

				
			}
		
			// Update and render script. Just a test for now.
			{
				CurrentScript = &IntroScript;
				if (ScriptParagraphIndex < CurrentScript->paragraphs.count)
				{
					//// Draw background.
					//if (ScriptFadeInOutTexture.id)
					//{
					//	DrawTexture(ScriptFadeInOutTexture, 0, 0, WHITE);
					//}

					Paragraph paragraph = CurrentScript->paragraphs[ScriptParagraphIndex];
					const char* speaker = paragraph.speaker;

					ScriptParagraphTime += GetFrameTime();
					float time = 30 * ScriptParagraphTime;
					const char* expression = GetScriptExpression(*CurrentScript, ScriptParagraphIndex, time);

					const Color SPEAKER_COLOR = { 122, 156, 150, 255 };
					const Color TEXT_COLOR = { 209, 191, 176, 255 };
					const Rectangle TEXT_AREA = { 159, 213, 222, 46 };

					DrawTexture(DialogBoxTexture, 95, 198, WHITE);
					DrawTextEx(Arial8Font, speaker, { 159 + 1, 205 + 1 }, 13, 0, ColorLerp(SPEAKER_COLOR, BLACK, 0.8f));
					DrawTextEx(Arial8Font, speaker, { 159, 205 }, 13, 0, SPEAKER_COLOR);
					bool doneWithScriptParagraph = DrawScriptParagraph(CurrentScript, ScriptParagraphIndex, TEXT_AREA, 7, TEXT_COLOR, ColorLerp(TEXT_COLOR, BLACK, 0.8f), time);
					
					// Advance on enter for now.
					if (IsKeyPressed(KEY_ENTER))
					{
						if (doneWithScriptParagraph)
						{
							PlaySound(PlaceholderSound);
							ScriptParagraphIndex++;
							ScriptParagraphTime = 0;
							CurrentScript->commandIndex = 0; // Such a mess :(
						}
						else ScriptParagraphTime = 9999; // Enough to go over everything.
					}
				}
			}

			DrawTextureV(PlaceholderTexture, MousePos, WHITE);
		}
		EndTextureMode();
		
		// Draw game render texture in the center of the screen.
		DrawTexturePro(FixedRenderTexture.texture, { 0, 0, FIXED_WIDTH, -FIXED_HEIGHT }, { innerWindowOffset.x, innerWindowOffset.y, scaledWidth, scaledHeight }, {}, 0, WHITE);

#ifdef EDITOR
		static GridTile selectedTile = GridTile_Floor;
		/*if (GuiButton(Rectangle{16,16,64,24},"Tile type1"))
		{
			selectedTile = GridTile_Floor;
		}*/
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
		{
			setGridTile(&gameState.grid,(uint32_t)nearbyintf(MousePos.x/16.f),(uint32_t)nearbyintf(MousePos.y/16.f), selectedTile);
		}

		if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
		{
			setGridTile(&gameState.grid,(uint32_t)nearbyintf(MousePos.x/16.f),(uint32_t)nearbyintf(MousePos.y/16.f), GridTile_None);
		}
		// Draw GUI on top of everything on the real window coordinates.
		DrawFPS(10, 10);
#endif
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
	InitWindow(FIXED_WIDTH, FIXED_HEIGHT, "Jammin");
	SetExitKey(0); // prevent exiting by pressing esc
	InitAudioDevice();

	// Load and apply previous window placement.
	struct WindowPlacement
	{
		Rectangle rectangle;
		bool maximized;
	};
	int fileSize{};
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
	initializeGameState(&gameState);
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
	placement.rectangle.width = (float)GetScreenWidth();
	// This is window width, not screen width. Poor naming by Raylib .
	placement.rectangle.height = (float)GetScreenHeight();
	placement.maximized = IsWindowMaximized();
	SaveFileData("window.bin", &placement, sizeof placement);
}
