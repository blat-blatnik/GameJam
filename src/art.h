#pragma once
#include "utils.h"

#ifndef X_TEXTURES
#define X_TEXTURES(X)\
	/* X(variable_name, filepath) */\
	X(PlaceholderTexture, "placeholder.png")\
	X(GridTest, "gridtest.png")\
	X(DialogBoxTexture, "dialog-box.png")\
	X(Bathroom, "bathroom_sprites.png")\

#endif

#ifndef X_SOUNDS
#define X_SOUNDS(X)\
	/* X(variable_name, filepath) */\
	X(PlaceholderSound, "placeholder.wav")\

#endif

#ifndef X_FONTS
#define X_FONTS(X)\
	/* X(variable_name, filepath) */\
	X(Arial8Font, "arial8.png")\
	X(AsepriteFont, "aseprite-font.png")\

#endif

#ifndef X_SCRIPTS
#define X_SCRIPTS(X)\
	/* X(variable_name, filepath) */\
	X(IntroScript, "intro.script")\

#endif

#ifndef X_SLIDESHOWS
#define X_SLIDESHOWS(X)\
	/* X(variable_name, filepath) */\
	X(IntroSlideshow, "intro.slideshow")\

#endif

// Globals

#define DefineTexture(name, filepath) inline Texture name;
X_TEXTURES(DefineTexture)

#define DefineSound(name, filepath) inline Sound name;
X_SOUNDS(DefineSound)

#define DefineFont(name, filepath) inline Font name;
X_FONTS(DefineFont)

#define DefineScript(name, filepath) inline Script name;
X_SCRIPTS(DefineScript)

#define DefineSlideshow(name, filepath) inline Slideshow name;
X_SLIDESHOWS(DefineSlideshow)

inline void LoadAllArt()
{
	#define LoadTexture(name, filepath) name = GetTexture(filepath);
	X_TEXTURES(LoadTexture);
	#undef LoadTexture

	#define LoadSound(name, filepath) name = GetSound(filepath);
	X_SOUNDS(LoadSound);
	#undef LoadSound

	#define LoadFont(name, filepath) name = LoadPixelFont("art/" filepath);
	X_FONTS(LoadFont);
	#undef LoadFont

	#define LoadScript(name, filepath) name = LoadScript("art/" filepath, AsepriteFont, AsepriteFont, AsepriteFont, AsepriteFont);
	X_SCRIPTS(LoadScript);
	#undef LoadScript

	#define LoadSlideshow(name, filepath) name = LoadSlideshow("art/" filepath);
	X_SLIDESHOWS(LoadSlideshow);
	#undef LoadSlideshow
}
