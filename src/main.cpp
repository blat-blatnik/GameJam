#include "art.h"
#include "utils.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "game.h"
#define FIXED_WIDTH 480 // 1920/4
#define FIXED_HEIGHT 270

#define FIXED_SIZE Vector2{ FIXED_WIDTH, FIXED_HEIGHT }

#ifdef DEV_MODE
static uint32_t selectedTextureIndex = 0;
static uint32_t selectedLayer = 0;

#endif

RenderTexture FixedRenderTexture{};
// Game gets rendered to this (SCREEN_WIDTH x SCREEN_HEIGHT) texture, and then upscaled.
Vector2 MousePos{}; // Cursor position relative to the inner FIXED_SIZE screen, always within (0,0) .. FIXED_SIZE
GameState gameState{}; // (oualid) idk how to pass variable to emscripting's loop :(

// Dialog script stuff

Script* CurrentScript = &IntroScript;
int ScriptParagraphIndex;
float ScriptParagraphTime;
Texture ScriptFadeInOutTexture;
float ScriptFadeInOutTime;
float ScriptFadeInOutIncrement;

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
		ScriptFadeInOutIncrement = +1.5f;
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
					ClearBackground(BLACK);
					//DrawText("Hello :)", 100, 100, 64, RED);

					{
						// draw level and player
						for (uint32_t layer = 0; layer < gameState.grid.layers; layer++)
						{
							for (uint32_t y = 0; y < gameState.grid.height; y++)
							{
								for (uint32_t x = 0; x < gameState.grid.width; x++)
								{
									uint32_t textureIndex = getGridTile(&gameState.grid, x, y, layer);
									DrawTextureRec(Bathroom, Rectangle{
													   (float)(textureIndex * 16 % Bathroom.width),
													   (float)(textureIndex * 16 / Bathroom.width), 16, 16
												   }, Vector2{x * 16.f, y * 16.f},WHITE);
								}
							}
						}

						DrawRectangleV(gameState.player.position, Vector2{16, 32},DARKGREEN);
					}

				
		
			// Update and render script. Just a test for now.
			if (CurrentScript)
			{
				// Draw background.
				float fadeAlpha = 1;
				if (ScriptFadeInOutTexture.id)
				{
					fadeAlpha = Clamp(ScriptFadeInOutTime, 0, 1);
					fadeAlpha = floorf(fadeAlpha * 6) / 6; // Quantize

					DrawRectangle(0, 0, FIXED_WIDTH, FIXED_HEIGHT, BLACK);
					Color fadeTint = ColorAlpha(WHITE, fadeAlpha);
					DrawTexture(ScriptFadeInOutTexture, 0, 0, fadeTint);
					ScriptFadeInOutTime += GetFrameTime() * ScriptFadeInOutIncrement;
					ScriptFadeInOutTime = Clamp(ScriptFadeInOutTime, 0, 1);
				}

						Paragraph paragraph = CurrentScript->paragraphs[ScriptParagraphIndex];
						const char* speaker = paragraph.speaker;

						ScriptParagraphTime += GetFrameTime();
						float time = 30 * ScriptParagraphTime;
						const char* expression = GetScriptExpression(*CurrentScript, ScriptParagraphIndex, time);

						const Color SPEAKER_COLOR = { 122, 156, 150, 255 };
						const Color TEXT_COLOR = { 209, 191, 176, 255 };
						const Rectangle TEXT_AREA = { 159, 220, 222, 46 };

						DrawTexture(DialogBoxTexture, 95, 198, WHITE);

						const char* characterPortraitName = TextFormat("%s-%s.png", speaker, expression);
						Texture characterPortrait = GetTexture(characterPortraitName);
						DrawTexture(characterPortrait, 105, 209, WHITE);

						DrawTextEx(Arial8Font, speaker, { 159 + 1, 205 + 1 }, 13, 0, ColorLerp(SPEAKER_COLOR, BLACK, 0.8f));
						DrawTextEx(Arial8Font, speaker, { 159, 205 }, 13, 0, SPEAKER_COLOR);
						bool doneWithScriptParagraph = DrawScriptParagraph(CurrentScript, ScriptParagraphIndex, TEXT_AREA, 7, TEXT_COLOR, ColorLerp(TEXT_COLOR, BLACK, 0.8f), time);

						// Advance on any of these inputs.
						bool advance =
							IsKeyPressed(KEY_ENTER) or
							IsKeyPressed(KEY_SPACE) or
							IsKeyPressed(KEY_E) or
							IsKeyPressed(KEY_F) or
							IsMouseButtonPressed(MOUSE_BUTTON_LEFT) or
							IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
						if (advance or (ScriptFadeInOutIncrement < 0 and ScriptFadeInOutTime <= 0))
						{
							if (doneWithScriptParagraph and ScriptFadeInOutTime <= 0)
							{
								PlaySound(PlaceholderSound);
								ScriptParagraphIndex++;
								ScriptParagraphTime = 0;
								CurrentScript->commandIndex = 0; // Such a mess :(
								if (ScriptParagraphIndex >= CurrentScript->paragraphs.count)
								{
									CurrentScript = nullptr; // Done with the script! Go back to gameplay, or end the cutscene.
									ScriptFadeInOutTexture = {};
									ScriptFadeInOutIncrement = 0;
								}
							}
							else if (not doneWithScriptParagraph)
								ScriptParagraphTime = 9999; // Enough to go over everything.
							else if (ScriptFadeInOutTime > 0 and ScriptFadeInOutIncrement > 0)
								ScriptFadeInOutIncrement = -ScriptFadeInOutIncrement; // Start fading out.
							else
								assert(ScriptFadeInOutIncrement < 0);
						}
					}

					DrawTextureV(PlaceholderTexture, MousePos, WHITE);
				}
				EndTextureMode();
		
				// Draw game render texture in the center of the screen.
				DrawTexturePro(FixedRenderTexture.texture, { 0, 0, FIXED_WIDTH, -FIXED_HEIGHT }, { innerWindowOffset.x, innerWindowOffset.y, scaledWidth, scaledHeight }, {}, 0, WHITE);

#if DEV_MODE
				/*if (GuiButton(Rectangle{16,16,64,24},"Tile type1"))
				{
					selectedTile = GridTile_Floor;
				}*/
				if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
				{
					setGridTile(&gameState.grid, (uint32_t)nearbyintf(MousePos.x / 16.f),
								(uint32_t)nearbyintf(MousePos.y / 16.f), selectedLayer, (GridTile)selectedTextureIndex);
				}

				if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
				{
					setGridTile(&gameState.grid, (uint32_t)nearbyintf(MousePos.x / 16.f),
								(uint32_t)nearbyintf(MousePos.y / 16.f), selectedLayer, GridTile_None);
				}


				static bool showMessageBox = false;
		if (GuiButton(Rectangle{ 24, 24, 120, 30 }, "#191#Editor Tool")) showMessageBox = true;

		if (showMessageBox)
		{
			int btnActive = -1;
			GuiMessageBox(Rectangle{ 85, 70, 250, 100 },
				"#191#Message Box", "Hi! This is a message!", "Prev tile;Next tile;Layer 0;Layer 1", &btnActive);
			switch (btnActive)
			{
				case -1:
				break;
				case 1:
					selectedTextureIndex--;
				break;
				case 2:
					selectedTextureIndex++;
				break;
			case 3:
				selectedLayer =0;
				break;
			case 4:
				selectedLayer =1;
				break;
			default:
				showMessageBox = false;
			}
			selectedTextureIndex%=(Bathroom.width *Bathroom.height/16);

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
