#pragma once
#include "utils.h"

#ifndef X_TEXTURES
#define X_TEXTURES(X)\
	/* X(variable_name, filepath) */\
	X(PlaceholderTexture, "placeholder.png")\

#endif

#ifndef X_SOUNDS
#define X_SOUNDS(X)\
	/* X(variable_name, filepath) */\
	X(PlaceholderSound, "placeholder.wav")\

#endif

#ifndef X_FONTS
#define X_FONTS(X)\
	/* X(variable_name, filepath, size) */\
	X(DialogueBoxFont, "pressstart2p.ttf", 9)\

#endif

#ifndef X_SCRIPTS
#define X_SCRIPTS(X)\
	/* X(variable_name, filepath) */\
	X(ExampleScript, "example-script.txt")\

#endif

// Globals

#define DefineTexture(name, filepath) inline Texture name;
X_TEXTURES(DefineTexture)

#define DefineSound(name, filepath) inline Sound name;
X_SOUNDS(DefineSound)

#define DefineFont(name, filepath, size) inline Font name;
X_FONTS(DefineFont)

#define DefineScript(name, filepath) inline Script name;
X_SCRIPTS(DefineScript)

inline void LoadAllArt()
{
	#define LoadTexture(name, filepath) name = LoadTexture("art/" filepath);
	X_TEXTURES(LoadTexture);
	#undef LoadTexture

	#define LoadSound(name, filepath) name = LoadSound("art/" filepath);
	X_SOUNDS(LoadSound);
	#undef LoadSound

	#define LoadFont(name, filepath, size) name = LoadFontEx("art/" filepath, size, nullptr, 0);
	X_FONTS(LoadFont);
	#undef LoadFont

	Font defaultFont = GetFontDefault();
	#define LoadScript(name, filepath) name = LoadScript("art/" filepath, DialogueBoxFont, DialogueBoxFont, DialogueBoxFont, DialogueBoxFont);
	X_SCRIPTS(LoadScript);
	#undef LoadScript
}
