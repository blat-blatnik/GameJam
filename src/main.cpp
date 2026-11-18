#include "art.h"
#include "utils.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "game.h"
#define FIXED_WIDTH 480 // 1920/4
#define FIXED_HEIGHT 270
#define TILE_SIZE 16
#define ROOM_WIDTH_IN_TILES 30 // 480/16
#define ROOM_HEIGHT_IN_TILES 17 // 270/16
#define PLAYER_WIDTH_IN_TILES 1 // in tiles
#define PLAYER_HEIGHT_IN_TILES 2 // in tiles

static const char* collisionFile ="collision.bin";

#define FIXED_SIZE Vector2{ FIXED_WIDTH, FIXED_HEIGHT }

enum Mode :uint32_t
{
	Mode_Normal,
	Mode_TileSelection,
	Mode_Paint,
	Mode_Script,
	Mode_Console,
	Mode_GhostEdit,
	Mode_MoveRoom,
};

static Mode mode = Mode_Normal;
#ifdef DEV_MODE
static uint32_t selectedTextureIndex = 0;
static uint32_t nextSelectedTextureIndex = 0;
static uint32_t selectedLayer = 0;
#endif

RenderTexture FixedRenderTexture{}; // Game gets rendered to this (SCREEN_WIDTH x SCREEN_HEIGHT) texture, and then upscaled.
Vector2 MousePos{}; // Cursor position relative to the inner FIXED_SIZE screen, always within (0,0) .. FIXED_SIZE
GameState gameState{// (oualid) idk how to pass variable to emscripting's loop :(
	.numGhosts = 1,
	.ghosts = { { .position = {200,200} } },
};

// Slideshow stuff

Slideshow* CurrentSlideshow = nullptr; // &IntroSlideshow;
int CurrentSlideIndex = 0;
float CurrentSlideAlpha;
float CurrentSlideAlphaIncrement = +1.5f;

bool IsInteractButtonPressed()
{
	return
		IsKeyPressed(KEY_ENTER) or
		IsKeyPressed(KEY_SPACE) or
		IsKeyPressed(KEY_E) or
		IsKeyPressed(KEY_F) or
		IsKeyPressed(KEY_KP_0) or
		IsKeyPressed(KEY_KP_ENTER) or
		IsMouseButtonPressed(MOUSE_BUTTON_LEFT) or
		IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}

// Dialog script stuff

Script* CurrentScript = nullptr; // &IntroScript;
int ScriptParagraphIndex ;
float ScriptParagraphTime;

// Music

#define MUSIC_FADE_TIME 1.0f

Music CurrentMusic;
Music NextQueuedMusic;
float MusicFade;
float MusicMaxVolume;

// Room transitions

char currentLevelName[128] = "bedroom.level";
char QueuedLevelName[128] = "";
float LevelTransitionFade = 1;
Vector2 QueuedLevelPlayerPosition;

// Ghost death sparkes

struct GhostDeathPuff
{
	Vector2 position;
	float frame;
};

GhostDeathPuff GhostDeathPuffs[16];
int NumGhostDeathPuffs;

// Script Commands

char ConsoleText[512];
Mode ConsoleBackupMode = Mode_Normal;

void EndCurrentScript()
{
	if (CurrentScript)
	{
		CurrentScript->commandIndex = 0; // Yes, all of this stuff really needs to be reset to 0 (I think), and yes, it is a total mess :')
		CurrentScript = nullptr;
	}
	ScriptParagraphIndex = 0;
	ScriptParagraphTime = 0;
}
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
		else TraceLog(LOG_ERROR, "Missing parameter 'filename' for 'sound' command.");
	}
	else if (TextIsEqual(command, "music"))
	{
		if (numParameters >= 1)
		{
			const char* filepath = split[1];
			const char* volumeString = numParameters >= 2 ? split[2] : "1";
			const char* pitchString = numParameters >= 3 ? split[3] : "1";

			MusicMaxVolume = strtof(volumeString, nullptr);
			float pitch = strtof(pitchString, nullptr);
			NextQueuedMusic = GetMusic(filepath);
			SetMusicPitch(NextQueuedMusic, pitch);
		}
		else TraceLog(LOG_ERROR, "Missing parameter 'filename' for 'music' command.");
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
		else TraceLog(LOG_ERROR, "Missing parameter 'filename' for 'level' command.");
	}
	else if (TextIsEqual(command, "enter"))
	{
		if (numParameters >= 3)
		{
			const char* filename = split[1];
			const char* extension = GetFileExtension(filename);
			if (not extension) filename = TextFormat("%s.level", filename);
			TextCopy(QueuedLevelName, filename);
			PlaySound("s2-door-open.ogg", 1, RandomFloat(0.95f, 1.05f));

			// Move the player.
			float tileX = Clamp(strtof(split[2], nullptr), 0, (float)gameState.level.grid.width);
			float tileY = Clamp(strtof(split[3], nullptr), 0, (float)gameState.level.grid.height);
			QueuedLevelPlayerPosition = { tileX * TILE_SIZE, (tileY - 1) * TILE_SIZE };

			EndCurrentScript();
		}
		else if (numParameters >= 2) TraceLog(LOG_ERROR, "Missing parameter 'y' for 'enter' command.");
		else if (numParameters >= 1) TraceLog(LOG_ERROR, "Missing parameter 'x' for 'enter' command.");
		else TraceLog(LOG_ERROR, "Missing parameter 'level_filename' for 'enter' command.");
	}
	else if (TextIsEqual(command, "slideshow"))
	{
		if (numParameters >= 1)
		{
			const char* filename = split[1];
			const char* extension = GetFileExtension(filename);
			if (not extension) filename = TextFormat("%s.slideshow", filename);

			EndCurrentScript();
			CurrentSlideshow = GetSlideshow(filename);
		}
		else TraceLog(LOG_ERROR, "Missing parameter 'filename' for 'slideshow' command.");
	}
	else if (TextIsEqual(command, "sethealth"))
	{
		if (numParameters >= 1)
		{
			float health = strtof(split[1], nullptr);
			gameState.player.health = Clamp(health, 0, 100);
		}
		else TraceLog(LOG_ERROR, "Missing parameter 'value' for 'sethealth' command.");
	}
	else if (TextIsEqual(command, "setprogress"))
	{
		if (numParameters >= 1)
		{
			int progress = (int)strtol(split[1], nullptr, 0);
			progress = ClampInt(progress, 0, GameProgress_EnumCount - 1);
			gameState.progress = (GameProgress)progress;
		}
		else TraceLog(LOG_ERROR, "Missing parameter 'value' for 'setprogress' command.");
	}
	else if (TextIsEqual(command, "ifprogress"))
	{
		if (numParameters >= 2)
		{
			GameProgress progress = (GameProgress)ClampInt((int)strtol(split[1], nullptr, 0), 0, GameProgress_EnumCount - 1);
			const char* gotoScriptFilename = split[2];
			if (gameState.progress >= progress)
			{
				CurrentScript->commandIndex = 0;
				CurrentScript = GetScript(gotoScriptFilename);
				CurrentScript->commandIndex = 0;
				ScriptParagraphIndex = 0;
				ScriptParagraphTime = 0;
			}
		}
		else
		{
			if (numParameters >= 1) TraceLog(LOG_ERROR, "Missing parameter 'goto_script_filename' for 'ifprogress' command.");
			else TraceLog(LOG_ERROR, "Missing parameter 'compare_value' for 'ifprogress' command.");
		}
	}
	else if (TextIsEqual(command, "end"))
	{
		EndCurrentScript();
	}
	else TraceLog(LOG_ERROR, "Unknown command '%s'", command);
}

// Game

void GameLoop()
{
	if (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_ALT))
		ToggleFullscreen_FixedForWeb();

	// Update music.
	if (NextQueuedMusic.stream.buffer != nullptr)
	{
		MusicFade -= GetFrameTime();
		if (MusicFade <= 0)
		{
			MusicFade = 0;
			CurrentMusic = NextQueuedMusic;
			NextQueuedMusic = {};
			PlayMusicStream(CurrentMusic);
		}
	}
	else if (CurrentMusic.stream.buffer != nullptr)
	{
		MusicFade += GetFrameTime();
		MusicFade = Min(MusicFade, MUSIC_FADE_TIME);
	}
	if (CurrentMusic.stream.buffer != nullptr)
	{
		float volume = Smoothstep(MusicFade);
		SetMusicVolume(CurrentMusic, volume);
		UpdateMusicStream(CurrentMusic);
	}

	// Update level transition.
	const float LEVEL_TRANSITION_FADE_SPEED = 1;
	if (QueuedLevelName[0] != '\0')
	{
		LevelTransitionFade -= GetFrameTime() * LEVEL_TRANSITION_FADE_SPEED;
		if (LevelTransitionFade <= 0)
		{
			LevelTransitionFade = 0;
			loadLevel(QueuedLevelName, &gameState);
			TextCopy(currentLevelName, QueuedLevelName);
			memset(QueuedLevelName, 0, sizeof QueuedLevelName);
			
			gameState.player.position = QueuedLevelPlayerPosition;
		}
	}
	else if (LevelTransitionFade < 1)
	{
		LevelTransitionFade += GetFrameTime() * LEVEL_TRANSITION_FADE_SPEED;
		LevelTransitionFade = Min(LevelTransitionFade, 1);
	}

	// Calculate input velocity.
	Vector2 inputVelocity{};
	if (IsGamepadAvailable(0))
	{
		inputVelocity.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
		inputVelocity.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
		if (inputVelocity.x > -0.25f && inputVelocity.x < 0.25f)
		{
			inputVelocity.x = 0;
		}
		if (inputVelocity.y > -0.25f && inputVelocity.y < 0.25f)
		{
			inputVelocity.y = 0;
		}
	}
	if (IsKeyDown(KEY_A) or IsKeyDown(KEY_KP_4) or IsKeyDown(KEY_LEFT))
	{
		inputVelocity.x -= 1.0f;
	}
	if (IsKeyDown(KEY_D) or IsKeyDown(KEY_KP_6) or IsKeyDown(KEY_RIGHT))
	{
		inputVelocity.x += 1.0f;
	}
	if (IsKeyDown(KEY_W) or IsKeyDown(KEY_KP_8) or IsKeyDown(KEY_UP))
	{
		inputVelocity.y -= 1.0f;
	}
	if (IsKeyDown(KEY_S) or IsKeyDown(KEY_KP_2) or IsKeyDown(KEY_KP_5) or IsKeyDown(KEY_DOWN))
	{
		inputVelocity.y += 1.0f;
	}
	if (Vector2LengthSqr(inputVelocity)>1.f)
	{
		inputVelocity = Vector2Normalize(inputVelocity);
	}
	if (CurrentScript or CurrentSlideshow) inputVelocity = {0,0};

	// Update.
	bool interactInput = IsInteractButtonPressed();
	if (not CurrentSlideshow and not CurrentScript and mode == Mode_Normal and QueuedLevelName[0] == '\0')
	{
		// Update player position.
		Player* player = &gameState.player;
		if (player->attackTime <= 0)
		{
			Vector2 playerVelocity = PLAYER_SPEED * inputVelocity;
			player->position += playerVelocity * GetFrameTime();
			if (playerVelocity.x != 0 or playerVelocity.y != 0)
			{
				// Update the facing direction.
				player->facing = FacingFromVector(playerVelocity);

				// Play footstep sounds.
				Sound s1 = GetSound("s1-walk-fast.mp3");
				Sound s2 = GetSound("s1-walk-medium.mp3");
				if (not IsSoundPlaying(s1) and not IsSoundPlaying(s2))
				{
					if (RandomCoinflip(1.0f))
						PlaySound(s1, RandomFloat(0.2f, 0.3f), RandomFloat(1.0f, 1.2f));
					else
						PlaySound(s2, RandomFloat(0.2f, 0.3f), RandomFloat(0.8f, 1.2f));
				}
			}
		}

		// Resolve any collisions with tiles.
		Rectangle playerFeetHitbox = { player->position.x, player->position.y + 26, TILE_SIZE, 6 };
		int tileX0 = (int)floorf(player->position.x / TILE_SIZE);
		int tileY0 = (int)floorf(player->position.y / TILE_SIZE);
		for (int tileY = tileY0; tileY <= tileY0 + PLAYER_HEIGHT_IN_TILES; tileY++)
		{
			for (int tileX = tileX0; tileX <= tileX0 + PLAYER_WIDTH_IN_TILES; tileX++)
			{
				// Loop over all tiles neighboring the player.
				if (tileX >= 0 and tileX < ROOM_WIDTH_IN_TILES and tileY >= 0 and tileY < ROOM_HEIGHT_IN_TILES)
				{
					for (int layer = 0; layer < 2; layer++)
					{
						int tileIndex = layer * (gameState.level.grid.width * gameState.level.grid.height) + tileY * (gameState.level.grid.width) + tileX;
						GridTile tileID = gameState.level.grid.tiles[tileIndex];
						uint8_t impassible = gameState.collision[tileID];
						if (impassible)
						{
							Rectangle tileHitbox = { (float)(tileX * TILE_SIZE), (float)(tileY * TILE_SIZE), TILE_SIZE, TILE_SIZE };
							ResolveCollisions(&playerFeetHitbox, tileHitbox);
						}
					}
				}
			}
		}
		player->position.x = playerFeetHitbox.x;
		player->position.y = playerFeetHitbox.y - 26;

		// Player is invincible for a few moments after being hit.
		player->invicibilityTime -= GetFrameTime();
		player->invicibilityTime = Max(player->invicibilityTime, 0);

		// Update animation.
		double playerAnimationFPS = 10;
		player->animationFrame += playerAnimationFPS * GetFrameTime();
		if (player->animationFrame > 10000) player->animationFrame -= 10000; // Wrap around to 0 to avoid float imprecision.

		// Attack! (or interact)
		if ((interactInput and player->attackTime <= 0 and gameState.numGhosts) or player->attackTime > 0)
		{
			if (player->attackTime <= 0)
			{
				interactInput = false; // consume the input
				player->attackSequenceCounter++;
				player->animationFrame = 0;

				if (RandomCoinflip(0.50f))
					PlaySound("s9-weapon-swing1.ogg", RandomFloat(0.5f, 0.6f), RandomFloat(0.8f, 1.2f));
				else
					PlaySound("s9-weapon-swing2.ogg", RandomFloat(0.5f, 0.6f), RandomFloat(0.8f, 1.2f));
			}

			player->attackTime += GetFrameTime();
			ClipContext clip = { player->attackTime };
			float windupT = ClipTime(&clip, PLAYER_ATTACK_WINDUP_TIME);
			float attackT = ClipTime(&clip, PLAYER_ATTACK_DAMAGE_TIME);
			if (attackT > 0)
			{
				Vector2 playerCenter = { player->position.x + PLAYER_WIDTH_IN_TILES * TILE_SIZE / 2, player->position.y + PLAYER_HEIGHT_IN_TILES * TILE_SIZE / 2 };
				Vector2 attackOffset = {};
				float attackRadius = 0;
				switch (player->facing)
				{
					case Facing_Up: attackOffset = { +0, -4 }; attackRadius = 20; break;
					case Facing_Down: attackOffset = { +0, +4 }; attackRadius = 20; break;
					case Facing_Left: attackOffset = { -8, +0 }; attackRadius = 17; break;
					case Facing_Right: attackOffset = { +8, +0 }; attackRadius = 17; break;
				}
				Vector2 attackCenter = playerCenter + attackOffset;

				for (int ghostIndex = 0; ghostIndex < gameState.numGhosts; ghostIndex++)
				{
					Ghost* ghost = &gameState.ghosts[ghostIndex];
					Rectangle ghostHitbox = { ghost->position.x, ghost->position.y, TILE_SIZE, 2 * TILE_SIZE };
					if (CheckCollisionCircleRec(attackCenter, attackRadius, ghostHitbox) and ghost->lastAttackSequenceCounter != player->attackSequenceCounter)
					{
						ghost->health -= 1;
						ghost->lastAttackSequenceCounter = player->attackSequenceCounter;
						if (ghost->health <= 0)
						{
							// Spawn death puff.
							if (NumGhostDeathPuffs < arrLength(GhostDeathPuffs))
							{
								GhostDeathPuff* puff = &GhostDeathPuffs[NumGhostDeathPuffs++];
								puff->position = ghost->position;
								puff->frame = 0;
							}
							gameState.ghosts[ghostIndex--] = gameState.ghosts[--gameState.numGhosts]; // Swap remove.
							
							// I'm not sure if I like s11... Maybe make it very unlikely to occur.
							PlaySound("s8-ghost-explosion.ogg", RandomFloat(0.9f, 1.2f), RandomFloat(1.0f, 1.3f));
						}
						else PlaySound("s10-ghost-hit.ogg", RandomFloat(0.5f, 0.6f), RandomFloat(0.8f, 1.2f));
					}
				}
			}
			if (attackT >= 1) player->attackTime = 0;
		}
		else if (interactInput) // Interact!
		{
			Rectangle hitbox = {};
			switch (player->facing)
			{
				case Facing_Up: hitbox = { player->position.x, player->position.y + 0.2f * TILE_SIZE, TILE_SIZE, 0.8f * TILE_SIZE }; break;
				case Facing_Down: hitbox = { player->position.x, player->position.y + 1.5f * TILE_SIZE, TILE_SIZE, 1.0f * TILE_SIZE }; break;
				case Facing_Left: hitbox = { player->position.x - 0.5f * TILE_SIZE, player->position.y, 0.8f * TILE_SIZE, 1.8f * TILE_SIZE }; break;
				case Facing_Right: hitbox = { player->position.x + 0.5f * TILE_SIZE, player->position.y, 0.8f * TILE_SIZE, 1.8f * TILE_SIZE }; break;
			}
			Vector2 hitboxCenter = { hitbox.x + hitbox.width / 2, hitbox.y + hitbox.height / 2 };
			int minTileX = (int)floorf(hitbox.x / TILE_SIZE);
			int minTileY = (int)floorf(hitbox.y / TILE_SIZE);
			int maxTileX = (int)floorf((hitbox.x + hitbox.width) / TILE_SIZE);
			int maxTileY = (int)floorf((hitbox.y + hitbox.height) / TILE_SIZE);
			Event* closestEvent = nullptr;
			float closestDistance = INFINITY;
			for (int tileY = minTileY; tileY <= maxTileY; tileY++)
			{
				for (int tileX = minTileX; tileX <= maxTileX; tileX++)
				{
					for (int i = 0; i < arrLength(gameState.level.events); i++)
					{
						Event* event = &gameState.level.events[i];
						if (event->cellX == tileX and event->cellY == tileY and event->scriptName[0] != '\0')
						{
							Vector2 eventCenter = { ((float)event->cellX + 0.5f) * TILE_SIZE, ((float)event->cellY + 0.5f) * TILE_SIZE };
							float distance = Vector2Distance(hitboxCenter, eventCenter);
							if (distance < closestDistance)
							{
								closestDistance = distance;
								closestEvent = event;
							}
						}
					}
				}
			}
			if (closestEvent)
			{
				const char* scriptName = closestEvent->scriptName;
				CurrentScript = GetScript(closestEvent->scriptName);
				ScriptParagraphIndex = 0;
				ScriptParagraphTime = 0;
				interactInput = false; // consume the input
			}
		}

		// Update ghosts.
		Rectangle playerHitbox = { player->position.x, player->position.y, TILE_SIZE, 2 * TILE_SIZE };
		for (int ghostIndex = 0; ghostIndex < gameState.numGhosts; ghostIndex++)
		{
			Ghost* ghost = &gameState.ghosts[ghostIndex];
			if (ghost->attackTime > 0)
			{
				ghost->attackTime += GetFrameTime();
				ClipContext clip = { ghost->attackTime };
				float windupT = ClipTime(&clip, GHOST_ATTACK_WINDUP_TIME);
				float attackT = ClipTime(&clip, GHOST_ATTACK_DAMAGE_TIME);
				float winddownT = ClipTime(&clip, GHOST_ATTACK_WINDDOWN_TIME);

				if (attackT > 0 and winddownT <= 0)
				{
					Rectangle attackHitbox = { ghost->position.x, ghost->position.y, TILE_SIZE, 2 * TILE_SIZE };
					attackHitbox = ExpandRectangle(attackHitbox, GHOST_ATTACK_RADIUS);
					if (CheckCollisionRecs(playerHitbox, attackHitbox) and player->invicibilityTime <= 0)
					{
						player->invicibilityTime = Max(player->invicibilityTime, PLAYER_INVINCIBILITY_TIME);
						player->health -= GHOST_ATTACK_DAMAGE;
						player->health = Max(player->health, 0);

						PlaySound("s12-damage.ogg", 0.5f, RandomFloat(0.8f, 1.2f));
					}
				}
				if (winddownT >= 1)
				{
					ghost->attackTime = 0;
					ghost->attackCooldownTime = GHOST_ATTACK_COOLDOWN_TIME;
				}
			}
			else
			{
				ghost->attackCooldownTime -= GetFrameTime();
				ghost->attackCooldownTime = Max(ghost->attackCooldownTime, 0);

				// Get repelled by other ghosts.
				Vector2 repelForce = {};
				for (int otherIndex = 0; otherIndex < gameState.numGhosts; otherIndex++)
				{
					if (otherIndex != ghostIndex)
					{
						Ghost* otherGhost = &gameState.ghosts[otherIndex];
						Vector2 awayFromOtherGhost = ghost->position - otherGhost->position;
						float distance = Vector2Length(awayFromOtherGhost);
						float ramp = 1 - Clamp(distance / (TILE_SIZE * 1.5f), 0, 1);
						repelForce += Vector2Normalize(awayFromOtherGhost) * ramp * 100;
					}
				}
				ghost->position += repelForce * GetFrameTime();

				// Face towards player.
				Vector2 delta = player->position - ghost->position;
				if (ghost->attackTime == 0)
					ghost->facing = FacingFromVector(delta);

				// Attack!
				float distance = Vector2Length(delta);
				if (distance < 25 and ghost->attackCooldownTime == 0)
				{
					ghost->attackTime += GetFrameTime();
					PlaySound("s11-ghost-die.ogg", RandomFloat(0.5f, 0.6f), RandomFloat(1.8f, 2.0f));
				}
				else
				{
					if (distance >= TILE_SIZE)
					{
						// Float towards the player.
						Vector2 direction = Vector2Normalize(delta);
						Vector2 velocity = direction * GHOST_SPEED;
						ghost->position += velocity * GetFrameTime();
					}
				}
			}
		}

		// Update ghost puffs.
		for (int puffIndex = 0; puffIndex < NumGhostDeathPuffs; puffIndex++)
		{
			GhostDeathPuff* puff = &GhostDeathPuffs[puffIndex];
			float puffAnimationFPS = 10;
			puff->frame += puffAnimationFPS * GetFrameTime();
			if (puff->frame >= 6)
				GhostDeathPuffs[puffIndex--] = GhostDeathPuffs[--NumGhostDeathPuffs]; // Swap remove.
		}
	}

	Mode prevMode = mode;
	#if DEV_MODE
	{
		// Toggle console with ` key (next to 1).
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
		// The console swallows all other input keys.
		if (mode != Mode_Console && IsKeyDown(KEY_LEFT_CONTROL))
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
			if (IsKeyPressed(KEY_FOUR))
			{
				mode = Mode_Script;
			}
			if (IsKeyPressed(KEY_FIVE))
			{
				mode = Mode_GhostEdit;
			}
			if (IsKeyPressed(KEY_SIX))
			{
				mode = Mode_MoveRoom;
			}
		}
	}
	#endif
	bool modeWasSwitchedThisFrame = mode != prevMode;

	// Scale the game content to fit the *actual* window size, draw it centered in the window, and add black bars.
	// This way we don't have to worry about actual window size at runtime.
	float windowWidth = (float)GetRenderWidth();
	float windowHeight = (float)GetRenderHeight();
	float maxScaleX = Max(windowWidth / FIXED_WIDTH, 1);
	float maxScaleY = Max(windowHeight / FIXED_HEIGHT, 1);
	float scale = Min(maxScaleX, maxScaleY);
	scale = floorf(scale); // Force integer scaling.
	float scaledWidth = FIXED_WIDTH * scale;
	float scaledHeight = FIXED_HEIGHT * scale;
	Vector2 scaledSize = {
		FIXED_WIDTH * scale,
		FIXED_HEIGHT * scale,
	};
	float remainingWidth = windowWidth - scaledWidth;
	float remainingHeight = windowHeight - scaledHeight;
	Vector2 innerWindowOffset = {
		roundf(remainingWidth / 2),
		roundf(remainingHeight / 2),
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
			ClearBackground({ 57, 43, 53, 255 });

			auto IsHackyTile = [](uint32_t tileid)
			{
				static const uint32_t HACKY_TILES[] =
				{
					58, 59, 60, // couch large
					119, 120, // couch small
					57, 41,//lamp
					157, 158, 159, // tv
					170, 200, 204,// dinning chairs
				};
				for (int i = 0; i < arrLength(HACKY_TILES); i++)
					if (HACKY_TILES[i] == tileid)
						return true;
				return false;
			};

			// Draw level.
			for (uint32_t layer = 0; layer < currentLevel->grid.layers; layer++)
			{
				for (uint32_t y = 0; y < currentLevel->grid.height; y++)
				{
					for (uint32_t x = 0; x < currentLevel->grid.width; x++)
					{
						uint32_t textureIndex = getGridTile(&currentLevel->grid, x, y, layer);
						if (textureIndex!=0 and not IsHackyTile(textureIndex))
						{
							DrawTextureRec(Tilemap, Rectangle{
								(float)(textureIndex % (Tilemap.width / 16)) * 16.f,
								(float)(textureIndex / (Tilemap.width / 16)) * 16.f, 16, 16
								}, Vector2{ x * 16.f, y * 16.f }, WHITE);
						}
					}
				}
			}

			// Draw player.
			Player player = gameState.player;
			{
				// Figure out which animation frames to use.
				int minFrame = 0;
				int maxFrame = 0; // inclusive
				if (player.attackTime > 0) // Attacking
				{
					minFrame = 50;
					maxFrame = 55;
				}
				else if (inputVelocity.x != 0 or inputVelocity.y != 0) // Moving
				{
					minFrame = 26;
					maxFrame = 33;
				}
				else // Idle
				{
					minFrame = 1;
					maxFrame = 8;
				}
				int numFrames = maxFrame - minFrame + 1;
				bool flipX = player.facing == Facing_Left;
				switch (player.facing)
				{
					case Facing_Down: break;
					case Facing_Up: minFrame += 2 * numFrames; maxFrame += 2 * numFrames; break;
					case Facing_Left:
					case Facing_Right: minFrame += numFrames; maxFrame += numFrames; break;
				}
				if (player.attackTime <= 0 and inputVelocity.x == 0 and inputVelocity.y == 0 and player.facing == Facing_Up)
				{
					minFrame = 18; // Idle up is the only idle animation that has 7 frames instead of 8.
					maxFrame = 25;
				}

				// Drop shadow.
				float shadowX = player.position.x + TILE_SIZE / 2;
				float shadowY = player.position.y + 2 * TILE_SIZE;
				if (player.facing == Facing_Left or player.facing == Facing_Right) shadowY -= 2;
				DrawEllipseV({ shadowX, shadowY }, 6, 2, ColorAlpha(BLACK, 0.25f));

				// Figure out which animation frame to use.
				int frame = minFrame + (int)player.animationFrame % numFrames;

				// Each player frame is 32x32.
				int offset = frame * 32;
				Rectangle sourceRect = { (float)offset, 0, 32, 32 };
				if (flipX) sourceRect.width *= -1;

				// Flash red while damaged.
				Color tint = WHITE;
				if (player.invicibilityTime > 0)
				{
					float t = TriangleWave(player.invicibilityTime * 12);
					tint = ColorLerp(ColorBrightness(WHITE, -0.5f), ColorBrightness(RED, -0.5f), t);
				}

				Rectangle destinationRect = { player.position.x - 8, player.position.y, 32, 32 };
				DrawTexturePro(PlayerSpritesheet, sourceRect, destinationRect, {}, 0, tint);
			}

			// Draw level (hacky tiles).
			for (uint32_t layer = 0; layer < currentLevel->grid.layers; layer++)
			{
				for (uint32_t y = 0; y < currentLevel->grid.height; y++)
				{
					for (uint32_t x = 0; x < currentLevel->grid.width; x++)
					{
						uint32_t textureIndex = getGridTile(&currentLevel->grid, x, y, layer);
						if (textureIndex != 0 and IsHackyTile(textureIndex))
						{
							DrawTextureRec(Tilemap, Rectangle{
								(float)(textureIndex % (Tilemap.width / 16)) * 16.f,
								(float)(textureIndex / (Tilemap.width / 16)) * 16.f, 16, 16
								}, Vector2{ x * 16.f, y * 16.f }, WHITE);
						}
					}
				}
			}

			// Draw attack hitbox for debug purposes.
			#if DEV_MODE
			if (player.attackTime)
			{
				ClipContext clip = { player.attackTime };
				float windupT = ClipTime(&clip, PLAYER_ATTACK_WINDUP_TIME);
				float attackT = ClipTime(&clip, PLAYER_ATTACK_DAMAGE_TIME);
				if (attackT > 0)
				{
					Vector2 playerCenter = { player.position.x + PLAYER_WIDTH_IN_TILES * TILE_SIZE / 2, player.position.y + PLAYER_HEIGHT_IN_TILES * TILE_SIZE / 2 };
					Vector2 attackOffset = {};
					float attackRadius = 0;
					switch (player.facing)
					{
						case Facing_Up: attackOffset = { +0, -4 }; attackRadius = 20; break;
						case Facing_Down: attackOffset = { +0, +4 }; attackRadius = 20; break;
						case Facing_Left: attackOffset = { -8, +0 }; attackRadius = 17; break;
						case Facing_Right: attackOffset = { +8, +0 }; attackRadius = 17; break;
					}
					Vector2 attackCenter = playerCenter + attackOffset;
					DrawCircleV(attackCenter, attackRadius, ColorAlpha(RED, 0.5f));
				}
			}
			#endif

			// Draw puffs.
			for (int puffIndex = 0; puffIndex < NumGhostDeathPuffs; puffIndex++)
			{
				GhostDeathPuff puff = GhostDeathPuffs[puffIndex];
				int frame = (int)puff.frame;
				int offset = 96 + 16 * frame;
				DrawTexturePro(GhostSpritesheet, { (float)offset, 0, 16, 32 }, { puff.position.x, puff.position.y, 16, 32 }, {}, 0, WHITE);
			}

			// Draw ghosts.
			for (int ghostIndex = 0; ghostIndex < gameState.numGhosts; ghostIndex++)
			{
				Ghost ghost = gameState.ghosts[ghostIndex];
				if (ghost.attackTime > 0)
				{
					float animationFPS = 20;
					float frameT = Clamp(ghost.attackTime / (GHOST_ATTACK_WINDUP_TIME + GHOST_ATTACK_DAMAGE_TIME + GHOST_ATTACK_WINDDOWN_TIME), 0, 1);
					int frame = ClampInt((int)(frameT * 9), 0, 9);
					int offset = 48 + 48 * frame;
					DrawTexturePro(GhostAttackSpritesheet, { (float)offset, 0, 48, 64 }, { ghost.position.x - 16, ghost.position.y - 16, 48, 64 }, {}, 0, WHITE);

					ClipContext clip = { ghost.attackTime };
					float windupT = ClipTime(&clip, GHOST_ATTACK_WINDUP_TIME);
					float attackT = ClipTime(&clip, GHOST_ATTACK_DAMAGE_TIME);
					float winddownT = ClipTime(&clip, GHOST_ATTACK_WINDDOWN_TIME);
					
					float alpha = 0;
					if (winddownT > 0) alpha = 1 - Smoothstep(winddownT);
					else alpha = Smoothstep(windupT);

					#if DEV_MODE
					Rectangle attackHitbox = { ghost.position.x, ghost.position.y, TILE_SIZE, 2 * TILE_SIZE };
					attackHitbox = ExpandRectangle(attackHitbox, GHOST_ATTACK_RADIUS);
					DrawRectangleRec(attackHitbox, ColorAlpha(RED, alpha * 0.5f));
					#endif
				}
				else
				{
					float animationFPS = 10;
					int frame = (ghostIndex + (int)fmod(GetTime() * animationFPS, 5)) % 5;
					int offset = 16 + 16 * frame;
					DrawTexturePro(GhostSpritesheet, { (float)offset, 0, 16, 32 }, { ghost.position.x, ghost.position.y, 16, 32 }, {}, 0, WHITE);
				}

				// Drop shadow.
				float shadowX = ghost.position.x + TILE_SIZE / 2;
				float shadowY = ghost.position.y + 2 * TILE_SIZE;
				DrawEllipseV({ shadowX, shadowY }, 6, 2, ColorAlpha(BLACK, 0.25f));
			}

			// Draw healthbar.
			float healtbarEnd = Remap(player.health, 0, 100, 0, 106);
			DrawTexturePro(HealthbarSpritesheet, { 0, 0, 110, 10 }, { 2, 2, 112, 10 }, {}, 0, WHITE);
			Color healthbarTint = player.health < 25 ? Color{255,80,80,255} : WHITE;
			DrawTexturePro(HealthbarSpritesheet, { 2, 11, 2 + healtbarEnd, 6 }, { 4, 4, 4 + healtbarEnd, 6 }, {}, 0, healthbarTint);
			
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
				float textCursorY = 256;
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
					bool advance = interactInput and mode == Mode_Normal;
					if (CurrentSlideAlpha >= 1 and advance)
					{
						CurrentSlideAlphaIncrement = -CurrentSlideAlphaIncrement;
						interactInput = false; // consume the input
					}
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
				ScriptParagraphIndex = ClampInt(ScriptParagraphIndex, 0, CurrentScript->paragraphs.count);

				Paragraph paragraph = CurrentScript->paragraphs[ScriptParagraphIndex];
				const char* speaker = paragraph.speaker;

				ScriptParagraphTime += GetFrameTime();
				float time = 30 * ScriptParagraphTime;
				const char* expression = GetScriptExpression(*CurrentScript, ScriptParagraphIndex, time);

				const Color SPEAKER_COLOR = {122, 156, 150, 255};
				const Color TEXT_COLOR = {209, 191, 176, 255};
				const Rectangle TEXT_AREA = {160, 220, 222, 46};

				DrawTexture(DialogBoxTexture, 92, 198, WHITE);

				const char* characterPortraitName = TextFormat("%s-%s.png", speaker, expression);
				Texture characterPortrait = GetTexture(characterPortraitName);
				DrawTextureCentered(characterPortrait, 125, 236, WHITE);

				DrawTextEx(Arial8Font, speaker, {160 + 1, 205 + 1}, 13, 0, ColorLerp(SPEAKER_COLOR, BLACK, 0.8f));
				DrawTextEx(Arial8Font, speaker, {160, 205}, 13, 0, SPEAKER_COLOR);
				bool doneWithScriptParagraph = DrawScriptParagraph(CurrentScript, ScriptParagraphIndex, TEXT_AREA, 7, TEXT_COLOR, ColorLerp(TEXT_COLOR, BLACK, 0.8f), time);
				if (CurrentScript) // Check again because "{end}" command can set CurrentScript to null.
				{
					bool advance = interactInput and mode == Mode_Normal;
					if (advance)
					{
						interactInput = false; // consume the input
						if (doneWithScriptParagraph)
						{
							PlaySound("s4-dialogue.mp3", RandomFloat(0.4f, 0.6f), RandomFloat(1.2f, 1.3f));

							ScriptParagraphIndex++;
							ScriptParagraphTime = 0;
							CurrentScript->commandIndex = 0; // Such a mess :(
							if (ScriptParagraphIndex >= CurrentScript->paragraphs.count)
							{
								ScriptParagraphIndex = 0;
								CurrentScript = nullptr; // Done with the script!
							}
						}
						else ScriptParagraphTime = 9999; // Enough to go over everything.
					}
				}
			}

			// Tint everything black so tilemap selection is easier to see.
			if (mode == Mode_TileSelection)
				DrawRectangle(0, 0, FIXED_WIDTH, FIXED_HEIGHT, ColorAlpha(BLACK, 0.5f));

			// Level transition to and from black.
			if (LevelTransitionFade < 1)
			{
				float alpha = Smoothstep(1 - LevelTransitionFade);
				DrawRectangle(0, 0, FIXED_WIDTH, FIXED_HEIGHT, ColorAlpha(BLACK, alpha));
			}
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
		else if (mode == Mode_Script)
		{
			static Vector2 selectedCell{};
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			{
				selectedCell = MousePos / 16.f;
				selectedCell.x=floorf(selectedCell.x);
				selectedCell.y=floorf(selectedCell.y);
			}
			
			char* script = nullptr;
			uint32_t selectedEvent {};
			for (uint32_t scriptIndex=0 ;scriptIndex< arrLength(Level::events);scriptIndex++)
			{
				if (script ==nullptr && gameState.level.events[scriptIndex].scriptName[0]==0)
				{
					script = gameState.level.events[scriptIndex].scriptName;
					selectedEvent = scriptIndex;
				}
				if (gameState.level.events[scriptIndex].cellX== (uint32_t)selectedCell.x && gameState.level.events[scriptIndex].cellY== (uint32_t)selectedCell.y)
				{
					script = gameState.level.events[scriptIndex].scriptName;
					selectedEvent = scriptIndex;
					break;
				}
			}
			gameState.level.events[selectedEvent].cellX =(uint32_t)selectedCell.x;
			gameState.level.events[selectedEvent].cellY =(uint32_t)selectedCell.y;
			
			GuiDrawText(TextFormat("cell:%i,%i",(uint32_t)selectedCell.x,(uint32_t)selectedCell.y),{8,8,128,24},TEXT_ALIGN_RIGHT,RAYWHITE);
			if (not modeWasSwitchedThisFrame) // Don't activate the text box on the first frame after a mode switch, otherwise it will enter "4" into the text box.
			{
				GuiTextBox({ 128 + 4.f, 8.f, 128.f, 24.f }, script, 128, true);
			}
			{
				Vector2 pos = selectedCell *16.f * scaledSize / FIXED_SIZE +innerWindowOffset;
				DrawRectangleLines((int32_t)pos.x,(int32_t)pos.y, (int32_t)(int32_t)(scaledSize.x/ FIXED_SIZE.x *16.f) ,(int32_t)(scaledSize.y/FIXED_SIZE.y *16.f) , BLUE);
			}
			for (uint32_t eventIndex = 0; eventIndex < arrLength(Level::events); eventIndex++)
			{
				if (currentLevel->events[eventIndex].scriptName[0]!=0)
				{
					Vector2 pos = Vector2{(float)currentLevel->events[eventIndex].cellX,(float)currentLevel->events[eventIndex].cellY} *16.f * scaledSize / FIXED_SIZE +innerWindowOffset;
					DrawRectangleLines((int32_t)pos.x,(int32_t)pos.y, (int32_t)(int32_t)(scaledSize.x/ FIXED_SIZE.x *16.f) ,(int32_t)(scaledSize.y/FIXED_SIZE.y *16.f) , GREEN);
				}
			}
			
		}
		else if (mode == Mode_GhostEdit)
		{
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			{
				for (uint32_t ghostIndex = 0; ghostIndex < arrLength(Level::events); ghostIndex++)
				{
					
				Vector2 pos = gameState.level.ghostPositions[ghostIndex];
					if (Vector2LengthSqr(pos)==0)
					{
						gameState.level.ghostPositions[ghostIndex]=  MousePos;
						break;
					}
				}
			
			}
			if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
			{
				for (uint32_t ghostIndex = 0; ghostIndex < arrLength(Level::events); ghostIndex++)
				{
					
					Vector2 pos = gameState.level.ghostPositions[ghostIndex];
					if (Vector2DistanceSqr(pos, MousePos)<= 24.f*24.f)
					{
						gameState.level.ghostPositions[ghostIndex]=  {};
						break;
					}
				}
				
			
			}
			
			for (uint32_t ghostIndex=0; ghostIndex<arrLength(Level::ghostPositions); ghostIndex++)
			{
				Vector2 pos = gameState.level.ghostPositions[ghostIndex] ;
				if (Vector2LengthSqr(pos)!=0.f)
				{
					//pos += innerWindowOffset;
					
					//y =(x+ innerWindowOffset) / scaledSize * FIXED_SIZE;
					 // y*scaledSize/ fixed size - innerWindowOffset;
					pos= pos *scaledSize/FIXED_SIZE +innerWindowOffset;
					DrawCircleLines((int32_t)pos.x,(int32_t)pos.y , 24, RED);
				}
			}

		}
		else if (mode == Mode_MoveRoom)
		{
			// Move every tile.
			Level* level = &gameState.level;
			Grid* grid = &level->grid;
			int dx = 0;
			int dy = 0;
			if (IsKeyPressed(KEY_LEFT)) dx--;
			if (IsKeyPressed(KEY_RIGHT)) dx++;
			if (IsKeyPressed(KEY_UP)) dy--;
			if (IsKeyPressed(KEY_DOWN)) dy++;
			if (dx != 0 or dy != 0)
			{
				GridTile* dstTiles = grid->tiles;
				GridTile* srcTiles = (GridTile*)MemAlloc(grid->width * grid->height * grid->layers * sizeof(GridTile));
				memcpy(srcTiles, dstTiles, grid->width * grid->height * grid->layers * sizeof(GridTile));
				for (int layer = 0; layer < grid->layers; layer++)
				{
					for (int dstY = 0; dstY < grid->height; dstY++)
					{
						for (int dstX = 0; dstX < grid->width; dstX++)
						{
							int srcY = dstY - dy;
							int srcX = dstX - dx;
							GridTile tile = {};
							if (srcX >= 0 and srcX < grid->width and srcY >= 0 and srcY < grid->height)
								tile = srcTiles[layer * grid->width * grid->height + srcY * grid->width + srcX];
							dstTiles[layer * grid->width * grid->height + dstY * grid->width + dstX] = tile;
						}
					}
				}
				MemFree(srcTiles);
			}

			// Move all script triggers.
			for (int i = 0; i < arrLength(level->events); i++)
			{
				Event* event = &level->events[i];
				if (event->scriptName[0] != '\0')
				{
					event->cellX += dx;
					event->cellY += dy;
					if (event->cellX < 0 or event->cellX >= grid->width or event->cellY < 0 or event->cellY >= grid->height)
						memset(event->scriptName, '\0', sizeof event->scriptName);
				}
			}

			// Move all ghosts.
			for (int i = 0; i < arrLength(level->ghostPositions); i++)
			{
				Vector2* position = &level->ghostPositions[i];
				if (Vector2LengthSqr(*position) != 0)
				{
					position->x += dx * TILE_SIZE;
					position->y += dy * TILE_SIZE;
					if (position->x < 0 or position->x >= grid->width * TILE_SIZE or position->y < 0 or position->y >= grid->height * TILE_SIZE)
						memset(position, 0, sizeof(*position));
				}
			}
		}

		if (mode != Mode_Console)
		{
			if (IsKeyPressed(KEY_P)&& IsKeyDown(KEY_LEFT_CONTROL))
			{
				saveLevel(currentLevelName, currentLevel);
			}
			if (IsKeyPressed(KEY_L)&& IsKeyDown(KEY_LEFT_CONTROL))
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
			case Mode_Script:
				GuiDrawText("Mode Scripting",{32,0,128,64},0,  RED);
				break;
			case Mode_Paint:
				GuiDrawText("Mode Paint",{32,0,128,64},0,RED);
				break;
			case Mode_GhostEdit:
				GuiDrawText("Mode Ghost edit",{32,0,128,64},0,RED);
				break;
			case Mode_MoveRoom:
				GuiDrawText("Mode Room Move", { 32, 0, 128, 64 }, 0, RED);
				break;
			default: ;
			}
			
		}
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
