#include "art.h"
#include "utils.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "game.h"
#define FIXED_WIDTH 480 // 1920/4
#define FIXED_HEIGHT 270

static const char* collisionFile ="collision.bin";

#define FIXED_SIZE Vector2{ FIXED_WIDTH, FIXED_HEIGHT }

enum Mode :uint32_t
{
	Mode_Normal,
	Mode_TileSelection,
	Mode_Paint,
	Mode_Console,
};

static Mode mode = Mode_Normal;
#ifdef DEV_MODE
static uint32_t selectedTextureIndex = 0;
static uint32_t nextSelectedTextureIndex = 0;
static uint32_t selectedLayer = 0;
#endif

char currentLevelName[128] = "test.level";


RenderTexture FixedRenderTexture{}; // Game gets rendered to this (SCREEN_WIDTH x SCREEN_HEIGHT) texture, and then upscaled.
Vector2 MousePos{}; // Cursor position relative to the inner FIXED_SIZE screen, always within (0,0) .. FIXED_SIZE
GameState gameState{}; // (oualid) idk how to pass variable to emscripting's loop :(

// Script Commands

char ConsoleText[512];
Mode ConsoleBackupMode = Mode_Normal;

void ExecuteScriptCommand(const char* commandAndParams)
{
	int numSplit;
	char** split = TextSplit(commandAndParams, ' ', &numSplit);
	int numParameters = numSplit - 1;
	assert(numSplit >= 1);

	const char* command = split[0];
	if (TextIsEqual(command, "sound"))
	{
		if (numParameters >= 1)
		{
			const char* filepath = split[1];
			const char* volumeString = numParameters >= 2 ? split[2] : "1";
			const char* pitchString = numParameters >= 3 ? split[3] : "1";

			float volume = strtof(volumeString, nullptr);
			float pitch = strtof(pitchString, nullptr);

			Sound sound = GetSound(filepath);
			SetSoundVolume(sound, volume);
			SetSoundPitch(sound, pitch);
			PlaySound(sound);
		}
		else TraceLog(LOG_ERROR, "Missing parameter 'sound_filename' for 'sound' command.");
	}
	else if (TextIsEqual(command, "level"))
	{
		if (numParameters >= 1)
		{
			const char* filename = split[1];
			const char* extension = GetFileExtension(filename);
			if (not extension) filename = TextFormat("%s.level", filename);

			if (loadLevel(filename, &gameState))
				TraceLog(LOG_INFO, "Loaded level '%s'", filename);
			else
				TraceLog(LOG_ERROR, "Level '%s' not found!", filename);
			TextCopy(currentLevelName, filename);
		}
		else TraceLog(LOG_ERROR, "Missing parameter 'level_filename' for 'level' command.");
	}
	else TraceLog(LOG_ERROR, "Unknown command '%s'", command);
}

// Slideshow stuff

Slideshow* CurrentSlideshow = nullptr; //&IntroSlideshow;
int CurrentSlideIndex = 0;
float CurrentSlideAlpha;
float CurrentSlideAlphaIncrement = +1.5f;

bool IsDialogAdvanceButtonPressed()
{
	return
		IsKeyPressed(KEY_ENTER) or
		IsKeyPressed(KEY_SPACE) or
		IsKeyPressed(KEY_E) or
		IsKeyPressed(KEY_F) or
		IsMouseButtonPressed(MOUSE_BUTTON_LEFT) or
		IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}

// Dialog script stuff

Script* CurrentScript = &IntroScript;
int ScriptParagraphIndex ;
float ScriptParagraphTime;

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

	if (IsKeyPressed(KEY_GRAVE))
	{
		if (mode != Mode_Console)
		{
			ConsoleBackupMode = mode;
			mode = Mode_Console;
			memset(ConsoleText, 0, sizeof ConsoleText);
		}
		else mode = ConsoleBackupMode;
	}

	// Let you type anything into the console.
	if (mode != Mode_Console)
	{
		if (IsKeyPressed(KEY_ONE))
		{
			mode = Mode_Normal;
		}
		if (IsKeyPressed(KEY_TWO))
		{
			mode = Mode_TileSelection;
		}
		if (IsKeyPressed(KEY_THREE))
		{
			mode = Mode_Paint;
		}
	}

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
	MousePos = Vector2Clamp(MousePos, {0, 0}, FIXED_SIZE);
	Level *currentLevel = &gameState.level;
	BeginDrawing();
	{
		ClearBackground(BLACK);
		// Render game content to a fixed size SCREEN_WIDTH x SCREEN_HEIGHT texture.
		BeginTextureMode(FixedRenderTexture);
		{
			ClearBackground(BLACK);

			{
				// draw level and player
				for (uint32_t layer = 0; layer < currentLevel->grid.layers; layer++)
				{
					for (uint32_t y = 0; y < currentLevel->grid.height; y++)
					{
						for (uint32_t x = 0; x < currentLevel->grid.width; x++)
						{
							uint32_t textureIndex = getGridTile(&currentLevel->grid, x, y, layer);
							DrawTextureRec(Tilemap, Rectangle{
												(float)(textureIndex % (Tilemap.width/16))*16.f,
												(float)(textureIndex  /(Tilemap.width/16))*16.f, 16, 16
											}, Vector2{x * 16.f, y * 16.f}, WHITE);
						}
					}
				}

				DrawRectangleV(gameState.player.position, Vector2{16, 32}, DARKGREEN);
			}

			// Update and render slideshow.
			if (CurrentSlideshow)
			{
				DrawRectangle(0, 0, FIXED_WIDTH, FIXED_HEIGHT, BLACK);

				CurrentSlideAlpha += GetFrameTime() * CurrentSlideAlphaIncrement;
				CurrentSlideAlpha = Clamp(CurrentSlideAlpha, 0, 1);

				float fadeAlpha = floorf(CurrentSlideAlpha * 6) / 6; // Quantize
				Color fadeTint = ColorAlpha(WHITE, fadeAlpha);
				Color textColor = ColorAlpha({ 209, 191, 176 }, fadeAlpha);

				// Draw the slide picture.
				Slide slide = CurrentSlideshow->slides[CurrentSlideIndex];
				Texture frame = slide.frames[0];
				int padX = (FIXED_WIDTH - frame.width) / 2;
				int padY = (FIXED_HEIGHT - frame.height) / 2;
				DrawTexture(frame, padX, padY, fadeTint);

				// Draw the slide text.
				float textCursorX = 0;
				float textCursorY = 200;
				const int TEXT_SIZE = 13;
				for (int i = 0; i < slide.lines.count; i++)
				{
					if (i > 0) textCursorY += GetLineHeight(Arial8Font, TEXT_SIZE);

					const char* line = slide.lines[i];
					Vector2 size = MeasureTextEx(Arial8Font, line, TEXT_SIZE, 0);
					Vector2 center = { FIXED_WIDTH / 2, textCursorY };
					DrawTextEx(Arial8Font, line, center - size / 2, TEXT_SIZE, 0, textColor);
					textCursorX = center.x + size.x / 2;

				}

				// Draw advance triangle.
				DrawTextCentered(Arial8Font, "\x7F", { textCursorX + 6, textCursorY }, TEXT_SIZE, 0, textColor);

				if (CurrentSlideAlphaIncrement > 0)
				{
					bool advance = IsDialogAdvanceButtonPressed();
					if (CurrentSlideAlpha >= 1 and advance)
						CurrentSlideAlphaIncrement = -CurrentSlideAlphaIncrement;
				}
				else
				{
					if (CurrentSlideAlpha <= 0)
					{
						CurrentSlideIndex++;
						CurrentSlideAlphaIncrement = -CurrentSlideAlphaIncrement;
						if (CurrentSlideIndex >= CurrentSlideshow->slides.count)
						{
							CurrentSlideshow = nullptr;
							CurrentSlideAlpha = 0;
						}
					}
				}
			}
			else if (CurrentScript) // Update and render script.
			{
				Paragraph paragraph = CurrentScript->paragraphs[ScriptParagraphIndex];
				const char* speaker = paragraph.speaker;

				ScriptParagraphTime += GetFrameTime();
				float time = 30 * ScriptParagraphTime;
				const char* expression = GetScriptExpression(*CurrentScript, ScriptParagraphIndex, time);

				const Color SPEAKER_COLOR = {122, 156, 150, 255};
				const Color TEXT_COLOR = {209, 191, 176, 255};
				const Rectangle TEXT_AREA = {159, 220, 222, 46};

				DrawTexture(DialogBoxTexture, 95, 198, WHITE);

				const char* characterPortraitName = TextFormat("%s-%s.png", speaker, expression);
				Texture characterPortrait = GetTexture(characterPortraitName);
				DrawTexture(characterPortrait, 105, 209, WHITE);

				DrawTextEx(Arial8Font, speaker, {159 + 1, 205 + 1}, 13, 0, ColorLerp(SPEAKER_COLOR, BLACK, 0.8f));
				DrawTextEx(Arial8Font, speaker, {159, 205}, 13, 0, SPEAKER_COLOR);
				bool doneWithScriptParagraph = DrawScriptParagraph(CurrentScript, ScriptParagraphIndex, TEXT_AREA, 7, TEXT_COLOR, ColorLerp(TEXT_COLOR, BLACK, 0.8f), time);

				bool advance = IsDialogAdvanceButtonPressed();
				if (advance)
				{
					if (doneWithScriptParagraph)
					{
						ScriptParagraphIndex++;
						ScriptParagraphTime = 0;
						CurrentScript->commandIndex = 0; // Such a mess :(
						if (ScriptParagraphIndex >= CurrentScript->paragraphs.count)
							CurrentScript = nullptr; // Done with the script!
					}
					else ScriptParagraphTime = 9999; // Enough to go over everything.
				}
			}

			DrawTextureV(PlaceholderTexture, MousePos, WHITE);
		}
		EndTextureMode();

		// Draw game render texture in the center of the screen.
		DrawTexturePro(FixedRenderTexture.texture, { 0, 0, FIXED_WIDTH, -FIXED_HEIGHT }, { innerWindowOffset.x, innerWindowOffset.y, scaledWidth, scaledHeight }, {}, 0, WHITE);

#if DEV_MODE
		if (mode == Mode_Paint)
		{
			if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
			{
				uint32_t selectedTextureX=  selectedTextureIndex % (Tilemap.width/16);
				uint32_t selectedTextureY=  selectedTextureIndex / (Tilemap.width/16);
				
				uint32_t lastX=  nextSelectedTextureIndex % (Tilemap.width/16) -selectedTextureX +1;
				uint32_t lastY=  nextSelectedTextureIndex / (Tilemap.width/16) -selectedTextureY +1;
				for (uint32_t y = 0; y < lastY; y++)
				{
					for (uint32_t x = 0; x < lastX; x++){
						setGridTile(&currentLevel->grid, (uint32_t)floorf(MousePos.x / 16.f)+x,
								(uint32_t)floorf(MousePos.y / 16.f)+y, selectedLayer, (GridTile)(selectedTextureIndex+x +y*(Tilemap.width/16)));
					}
				}
			}

			if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
			{
				setGridTile(&currentLevel->grid, (uint32_t)floorf(MousePos.x / 16.f),
							(uint32_t)floorf(MousePos.y / 16.f), selectedLayer, 0);
			}

		}
		else if (mode == Mode_TileSelection)
		{
			DrawTextureEx(Tilemap, {}, 0, 4,WHITE);
			{// draw collision bound
				for (uint32_t y = 0; y < Tilemap.height/16; y++)
				{
					for (uint32_t x = 0; x < Tilemap.width/16; x++)
					{
						if (gameState.collision[x+y*Tilemap.width/16])
						{
							DrawRectangleLines(x*16*4,y*16*4,16*4,16*4,RED);
						}
					}
				}
			}
			{// draw selected region
				uint32_t x=  selectedTextureIndex % (Tilemap.width/16);
				uint32_t y=  selectedTextureIndex / (Tilemap.width/16);
				uint32_t width = 16*4;
				uint32_t height = 16*4;
				if (selectedTextureIndex!=nextSelectedTextureIndex){

					uint32_t nextX=  nextSelectedTextureIndex % (Tilemap.width/16);
					uint32_t nextY=  nextSelectedTextureIndex / (Tilemap.width/16);
					width *= nextX-x +1;
					height *= nextY-y +1;
				
				}
			
				DrawRectangleLines(x*16*4,y*16*4,width,height,BLUE);
			}
			{// draw hovered tile bounds
				
				Vector2 mousePos= GetMousePosition();
				uint32_t mouseHoverCell = (int32_t)mousePos.x/(16*4)+ (int32_t)mousePos.y/(16*4) * Tilemap.width/16;
				uint32_t mouseHoverCellX=  mouseHoverCell % (Tilemap.width/16);
				uint32_t mouseHoverCellY=  mouseHoverCell / (Tilemap.width/16);
				if (mousePos.x< Tilemap.width*4 && mousePos.y< Tilemap.height*4)
				{
					DrawRectangleLines(mouseHoverCellX*16*4,mouseHoverCellY*16*4,16*4,16*4,GREEN);
				}
			}
			
			if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
			{
				Vector2 mousePos= GetMousePosition();
				if (mousePos.x< Tilemap.width*4 && mousePos.y< Tilemap.height*4)
				{
					uint32_t clickTileIndex = (int32_t)mousePos.x/(16*4)+ (int32_t)mousePos.y/(16*4) * Tilemap.width/16;
					gameState.collision[clickTileIndex] = !gameState.collision[clickTileIndex];
				}
			}
			if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
			{
				Vector2 mousePos= GetMousePosition();
				if (mousePos.x< Tilemap.width*4 && mousePos.y< Tilemap.height*4)
				{
					if (IsKeyDown(KEY_LEFT_SHIFT))
					{
						nextSelectedTextureIndex =(int32_t)mousePos.x/(16*4)+ (int32_t)mousePos.y/(16*4) * (Tilemap.width/16);
						if (selectedTextureIndex>nextSelectedTextureIndex)
						{
							uint32_t temp =nextSelectedTextureIndex;
							nextSelectedTextureIndex = selectedTextureIndex;
							selectedTextureIndex = temp;
						}
					}else
					{
						selectedTextureIndex = (int32_t)mousePos.x/(16*4)+ (int32_t)mousePos.y/(16*4) * (Tilemap.width/16);
						nextSelectedTextureIndex = selectedTextureIndex;
					}
				}
			}
			if (selectedLayer==0)
			{
				if (GuiButton(Rectangle{ Tilemap.width*4.f,0,64,32},"Layer 0"))
				{
					selectedLayer = 1;
				}
			}else
			{
				if (GuiButton(Rectangle{ Tilemap.width*4.f,0,64,32},"Layer 1"))
				{
					selectedLayer = 0;
				}
			}
			if (GuiButton(Rectangle{ Tilemap.width*4.f +  64,0,64,32},"Save collision"))
			{
				saveCollision(collisionFile,gameState.collision,(Tilemap.width/16) * (Tilemap.height/16));
			}
			if (GuiButton(Rectangle{ Tilemap.width*4.f + 128,0,64,32},"load collision"))
			{
				loadCollision(collisionFile,&Tilemap,&gameState.collision);
			}
			if (GuiButton(Rectangle{ Tilemap.width*4.f + 192,0,64,32},"save player pos to level"))
			{
				gameState.level.startingPosition =gameState.player.position;
			}
			
		}
		else if (mode == Mode_Console)
		{
			int backupTextSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
			GuiSetStyle(DEFAULT, TEXT_SIZE, 24);
			if (GuiTextBox(Rectangle{ 0, 0, windowWidth, 32 }, ConsoleText, sizeof ConsoleText, true))
			{
				ExecuteScriptCommand(ConsoleText);
				memset(ConsoleText, 0, sizeof ConsoleText);
				mode = ConsoleBackupMode; // Close the console.
			}
			if (ConsoleText[0] == '`') memmove(ConsoleText, ConsoleText + 1, sizeof ConsoleText - 1); //@HACK pressing ` to bring up the console also types ` into the text box, so just skip that.
			GuiSetStyle(DEFAULT, TEXT_SIZE, backupTextSize);
		}

		if (mode != Mode_Console)
		{
			if (IsKeyPressed(KEY_P))
			{
				saveLevel(currentLevelName, currentLevel);
			}
			if (IsKeyPressed(KEY_L))
			{
				loadLevel(currentLevelName, &gameState);
			}
		}

		static Mode lastFrameMode = mode;
		static float modeTime =0;
		if (lastFrameMode!=mode)
		{
			lastFrameMode = mode;
			modeTime =1.f;
		}
		if ( modeTime>0)
		{
			modeTime-= GetFrameTime() *2.f;
			
			switch (mode)
			{
				case Mode_Console:
				GuiDrawText("Mode Console",{32,0,128,64},0,  RED);
				break;
			case Mode_Normal:
				GuiDrawText("Mode Normal",{32,0,128,64},0,  RED);
				break;
			case Mode_TileSelection:
				GuiDrawText("Mode Tile Selection",{32,0,128,64},0,  RED);
				break;
			case Mode_Paint:
				GuiDrawText("Mode Paint",{32,0,128,64},0,RED);
				break;
			default: ;
			}
			
		}
		
		// Draw GUI on top of everything on the real window coordinates.
		DrawFPS(10, 60);
#endif
	}
	EndDrawing();
}
static bool initalizeLevel(Level* level)
{
	level->grid = initializeGrid(480 / 16 + 1, 270 / 16 + 1, 2);
	return true;
}

static bool initializeGameState(GameState* gameState)
{
	bool success = gameState != nullptr;
	if (success)
	{
		if (!loadLevel(currentLevelName,gameState))
		{
			initalizeLevel(&gameState->level);
		}
		if (gameState->collision==nullptr)
		{
			if (!loadCollision(collisionFile, &Tilemap,&gameState->collision))
			{
				gameState->collision= (uint8_t*)malloc(Tilemap.width/16*Tilemap.height/16);
				memset(gameState->collision,0,Tilemap.width/16*Tilemap.height/16);
			}
		}
		
	}
	return success;
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
