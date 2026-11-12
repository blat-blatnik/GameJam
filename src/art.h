#pragma once
#include "utils.h"

#ifndef X_TEXTURES
#define X_TEXTURES(X)\
	/* X(variable_name, filepath) */\
	X(PlaceholderTexture, "placeholder.png")\
	X(GridTest, "gridtest.png")\

#endif

#ifndef X_SOUNDS
#define X_SOUNDS(X)\
	/* X(variable_name, filepath) */\
	X(PlaceholderSound, "placeholder.wav")\

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

	Font defaultFont = GetFontDefault();
	#define LoadScript(name, filepath) name = LoadScript("art/" filepath, defaultFont, defaultFont, defaultFont, defaultFont);
	X_SCRIPTS(LoadScript);
	#undef LoadScript
}
