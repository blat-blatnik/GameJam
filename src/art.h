#pragma once
#include "raylib.h"

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

// Globals

#define DefineTexture(name, filepath) inline Texture name;
X_TEXTURES(DefineTexture)

#define DefineSound(name, filepath) inline Sound name;
X_SOUNDS(DefineSound)

inline void LoadAllArt()
{
	#define LoadTexture(name, filepath) name = LoadTexture("art/" filepath);
	X_TEXTURES(LoadTexture);
	#undef LoadTexture

	#define LoadSound(name, filepath) name = LoadSound("art/" filepath);
	X_SOUNDS(LoadSound);
	#undef LoadSound
}
