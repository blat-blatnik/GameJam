#include "art.h"
#include "utils.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "game.h"

#define FIXED_WIDTH 480 // 1920/4
#define FIXED_HEIGHT 270

#define FIXED_SIZE Vector2{ FIXED_WIDTH, FIXED_HEIGHT }

RenderTexture FixedRenderTexture; // Game gets rendered to this (SCREEN_WIDTH x SCREEN_HEIGHT) texture, and then upscaled.
Vector2 MousePos; // Cursor position relative to the inner FIXED_SIZE screen, always within (0,0) .. FIXED_SIZE
GameState gameState{}; // (oualid) idk how to pass variable to emscripting's loop :(

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
							DrawRectangleV(Vector2{x*16.f,y*16.f}, Vector2{16,16},DARKBLUE);
						}
					}
				}
            
				DrawRectangleV(gameState.player.position, Vector2{16,32},DARKGREEN);

				
			}
		
			// Draw the script. Just a test for now.
			static int ScriptParagraphIndex {};
			static float ScriptParagraphTime{};
			{
				Script script = ExampleScript;
				if (ScriptParagraphIndex < script.paragraphs.count)
				{
					Paragraph paragraph = script.paragraphs[ScriptParagraphIndex];
					const char* speaker = paragraph.speaker;
					if (not speaker) speaker = "default speaker";

					ScriptParagraphTime += GetFrameTime();
					float time = 20 * ScriptParagraphTime;
					const char* expression = GetScriptExpression(script, ScriptParagraphIndex, time);

					float textboxWidth = 30;
					float textboxHeight = 100;
					Rectangle textbox = { textboxWidth, FIXED_HEIGHT - textboxHeight - 20, FIXED_WIDTH - 2 * textboxWidth, textboxHeight };
					float TEXT_SIZE = 9;
					//// Portrait
					//{
					//	Rectangle portraitBox = textbox;
					//	portraitBox.x = 30;
					//	portraitBox.width = 300;
					//
					//	Rectangle indented = ExpandRectangle(portraitBox, -5);
					//	Rectangle textArea = ExpandRectangle(portraitBox, -15);
					//	Rectangle dropShadow = { portraitBox.x + 10, portraitBox.y + 10, portraitBox.width, portraitBox.height };
					//
					//	DrawRectangleRounded(dropShadow, 0.1f, 5, BLACK);
					//	DrawRectangleRounded(portraitBox, 0.1f, 5, WHITE);
					//	DrawRectangleRounded(indented, 0.1f, 5, ColorBrightness(WHITE, -0.2f));
					//
					//	//DrawTextureCentered(*portrait, RectangleCenter(portraitBox), WHITE);
					//}
					// Text
					{
						Rectangle indented = ExpandRectangle(textbox, -5);
						Rectangle textArea = ExpandRectangle(textbox, -15);
						Rectangle dropShadow = { textbox.x + 4, textbox.y + 4, textbox.width, textbox.height };

						DrawRectangleRounded(dropShadow, 0.0f, 5, BLACK);
						DrawRectangleRounded(textbox, 0.0f, 5, WHITE);
						DrawRectangleRounded(indented, 0.0f, 5, ColorBrightness(WHITE, -0.2f));

						const char* speakerString = TextFormat("[%s] [%s]", speaker, expression);
						DrawTextEx(script.font, speakerString, { textArea.x + 1, textArea.y + 1 }, TEXT_SIZE, 0, ColorLerp(RED, BLACK, 0.8f));
						DrawTextEx(script.font, speakerString, { textArea.x, textArea.y }, TEXT_SIZE, 0, RED);
						float yAdvance = 2 * GetLineHeight(script.font, TEXT_SIZE);
						textArea = ExpandRectangle(textArea, 0, 0, -yAdvance, 0);

						DrawScriptParagraph(&script, ScriptParagraphIndex, textArea, TEXT_SIZE, RED, ColorLerp(PINK, BLACK, 0.8f), time);
					}
				}
			}

			DrawTextureV(PlaceholderTexture, MousePos, WHITE);
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			{
				PlaySound(PlaceholderSound);
				ScriptParagraphIndex++;
				ScriptParagraphTime = 0;
			}
		}
		EndTextureMode();
		
		// Draw game render texture in the center of the screen.
		DrawTexturePro(FixedRenderTexture.texture, { 0, 0, FIXED_WIDTH, -FIXED_HEIGHT }, { innerWindowOffset.x, innerWindowOffset.y, scaledWidth, scaledHeight }, {}, 0, WHITE);

        // Draw GUI on top of everything on the real window coordinates.
        DrawFPS(10, 10);
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
