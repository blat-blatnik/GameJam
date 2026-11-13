#pragma once
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// Generic

constexpr auto Abs(auto x) { return x < 0 ? -x : x; }
constexpr auto Min(auto a, auto b) { return a < b ? a : b; }
constexpr auto Max(auto a, auto b) { return a > b ? a : b; }
constexpr int ClampInt(int x, int min, int max) { return Min(Max(x, min), max); }

// Vector

inline Vector2 Vector2Floor(Vector2 v)
{
	return { floorf(v.x), floorf(v.y) };
}
inline Vector2 RotateClockwise(Vector2 v)
{
	return { +v.y, -v.x };
}
inline Vector2 RotateCounterClockwise(Vector2 v)
{
	return { -v.y, +v.x };
}
inline Vector2 Vector2Polar(float length, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return { length * c, length * s };
}

// Color

inline Color Grayscale(float gray)
{
	unsigned char l = (unsigned char)(Clamp(gray, 0, 1) * 255.0f + 0.5f);
	return { l, l, l, 255 };
}

// Rectangle

inline Vector2 RectangleCenter(Rectangle r)
{
	return { r.x + r.width * 0.5f, r.y + r.height * 0.5f };
}
inline Rectangle ExpandRectangle(Rectangle r, float left, float right, float up, float down)
{
	r.x -= left;
	r.y -= up;
	r.width += left + right;
	r.height += up + down;
	return r;
}
inline Rectangle ExpandRectangle(Rectangle r, float x, float y)
{
	r.x -= x / 2;
	r.y -= y / 2;
	r.width += x;
	r.height += y;
	return r;
}
inline Rectangle ExpandRectangle(Rectangle r, float expand)
{
	r.x -= expand / 2;
	r.y -= expand / 2;
	r.width += expand;
	r.height += expand;
	return r;
}

// Interpolation

inline float Fract(float x) // Return fractional part of x.
{
	return x - floorf(x);
}
inline float Smoothstep(float t) // Smoothly interpolate t in [0,1].
{
	t = Clamp(t, 0, 1);
	return t * t * (3 - 2 * t);
}
inline float Smootherstep(float t) // Smoothly interpolate t in [0,1].
{
	t = Clamp(t, 0, 1);
	return t * t * t * (t * (6 * t - 15) + 10);
}
inline float TriangleWave(float t) // Alternate between 0 -> 1 -> 0 -> 1...
{
	float x = t - 2 * floorf(t / 2); // t mod 2 -> [0,2)
	return 1 - fabsf(x - 1);
}

// Noise

inline unsigned BitNoise(unsigned seed, int x, int y)
{
	// Get "random" bits at (x,y) position.
	// https://youtu.be/LWFzPP8ZbdU?t=2800.
	// https://nullprogram.com/blog/2018/07/31/
	// http://jonkagstrom.com/tuning-bit-mixers/index.html
	// I randomly cobbled together the magic numbers for this bit mixer.
	// They generate random looking 2D perlin noise, which means they're good enough in my book.
	unsigned v = seed;
	v *= 0xB5297A4Du;
	v ^= v >> 16;
	v += (unsigned)x;
	v *= 0x6CF7E237u;
	v ^= v >> 16;
	v += (unsigned)y;
	v *= 0x1B56C4E9u;
	v ^= v >> 16;
	v *= 0x846CA68Bu;
	v ^= v >> 16;
	return v;
}
inline int IntNoise(unsigned seed, int x, int min, int max)
{
	// Get "random" [min,max) int at (x) position: https://github.com/apple/swift/pull/39143
	unsigned long long r = (unsigned long long)(max - min);
	unsigned long long a = r * BitNoise(seed, x, 0);
	unsigned long long b = r * BitNoise(seed, x, 1);
	b = (b >> 32) + (unsigned)a;
	return min + (unsigned)(a >> 32) + (unsigned)(b >> 32);
}
inline float FloatNoise(unsigned seed, int x, int y)
{
	// Get "random" [0,1) float at (x,y) position. https://prng.di.unimi.it/#remarks
	unsigned bits = BitNoise(seed, x, y);
	return (bits >> 8) * 0x1.0p-24f;
}
inline float FloatNoise(unsigned seed, int x, int y, float min, float max)
{
	// Get "random" [min,max) float at (x,y) position.
	return min + FloatNoise(seed, x, y) * (max - min);
}
inline float NormalNoise(unsigned seed, int x)
{
	// Get "random" float with standard normal distribution at (x) position: https://marc-b-reynolds.github.io/distribution/2021/03/18/CheapGaussianApprox.html
	long long a = (long long)BitNoise(seed, x, 0);
	long long b = (long long)BitNoise(seed, x, 1);
	long long c = (long long)BitNoise(seed, x, 2);
	long long d = (long long)BitNoise(seed, x, 3);

	// The "magic" constant is sqrt(3) * 2^-32.
	return 0x1.B566E2p-32f * ((a + b) - (c + d));
}
inline float NormalNoise(unsigned seed, int x, float mean, float sdev)
{
	// Get "random" float with normal distribution at (x) position.
	return mean + sdev * NormalNoise(seed, x);
}
inline float ValueNoise(unsigned seed, float x)
{
	// Sample from grid of [0,1) floats with interpolation.
	float fx = floorf(x);
	int ix = (int)fx;
	float v0 = FloatNoise(seed, ix + 0, 0) * 2 - 1;
	float v1 = FloatNoise(seed, ix + 1, 0) * 2 - 1;
	return v0 + (v1 - v0) * Smoothstep(x - fx);
}
inline float GradientNoise(unsigned seed, float x)
{
	// Basically 1D Perlin noise.
	float fx = floorf(x);
	float dx = x - fx;
	int ix = (int)fx;
	float v0 = FloatNoise(seed, ix + 0, 0) * 2 - 1;
	float v1 = FloatNoise(seed, ix + 1, 0) * 2 - 1;
	float g0 = (dx + 0) * v0;
	float g1 = (dx - 1) * v1;
	return g0 + (g1 - g0) * Smoothstep(dx);
}

// Random

inline unsigned RNGSeed = 0x42; // Global RNG.
inline int RNGIndex = 0;

inline int RandomInt(int min, int max) // Get random int in [min,max).
{
	return IntNoise(RNGSeed, RNGIndex++, min, max);
}
inline float RandomFloat() // Get random float in [0,1).
{
	return FloatNoise(RNGSeed, RNGIndex++, 0);
}
inline float RandomFloat(float min, float max) // Get random float in [min,max).
{
	return FloatNoise(RNGSeed, RNGIndex++, 0, min, max);
}
inline float RandomNormal() // Get random float with standard normal distribution.
{
	return NormalNoise(RNGSeed, RNGIndex++);
}
inline float RandomNormal(float mean, float sdev) // Get random float with normal distribution.
{
	return NormalNoise(RNGSeed, RNGIndex++, mean, sdev);
}

// "Coroutines"

struct ClipContext
{
	float time; // Initialize to 0 and update using += GetFrameTime().
	float clipStart; // The start time of the next clip.
};

inline float ClipTime(ClipContext* ctx, float duration) // Return [0,1] normalized time inside the current clip, and advance to the next clip.
{
	float t0 = ctx->clipStart;
	float t1 = t0 + duration;
	ctx->clipStart = t1;

	float t = ctx->time - t0;
	float normalized = t / duration;
	return Clamp(normalized, 0, 1);
}

// String

inline unsigned Hash(const char* string) // FNV-1a
{
	unsigned hash = 0x811C9DC5;
	while (*string)
		hash = (hash ^ (unsigned char)*string++) * 0x01000193;
	return hash;
}

// List

template<class T> struct List
{
	T* items;
	int capacity;
	int count;

	T* Alloc(int numItems)
	{
		int minCapacity = count + numItems;
		if (minCapacity > capacity)
		{
			capacity = Max(capacity, 64);
			while (capacity < minCapacity) capacity *= 2;
			items = (T*)MemRealloc(items, capacity * sizeof(T));
		}
		T* block = &items[count];
		count += numItems;
		return block;
	}
	void Add(T item)
	{
		if (count >= capacity)
		{
			capacity = Max(capacity * 2, 64);
			items = (T*)MemRealloc(items, capacity * sizeof(T));
		}
		items[count++] = item;
	}
	T Pop()
	{
		assert(count > 0);
		return items[--count];
	}
	void Free()
	{
		MemFree(items);
		items = nullptr;
		capacity = count = 0;
	}
	T& operator[](int index)
	{
		assert(index >= 0 and index < count);
		return items[index];
	}
};

// Font

inline float GetLineHeight(Font font, float fontSize)
{
	return font.baseSize * (fontSize / font.baseSize);
}
inline Font LoadPixelFont(const char* filepath)
{
	Image image = LoadImage(filepath);
	Font font = LoadFontFromImage(image, MAGENTA, ' ');
	SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
	return font;
}

// Text

inline void DrawTextCentered(Font font, const char* text, Vector2 center, float fontSize, float spacing, Color tint)
{
	Vector2 size = MeasureTextEx(font, text, fontSize, spacing);
	DrawTextEx(font, text, center - size / 2, fontSize, spacing, tint);
}

// Assets

Texture GetTexture(const char* filepath);
Sound GetSound(const char* filepath);

// Slideshow

struct Slide
{
	List<Texture> frames;
	List<char*> lines;
};
struct Slideshow
{
	List<Slide> slides;
};

Slideshow LoadSlideshow(const char* filepath);

// Script

struct Paragraph
{
	char *speaker;
	char *text; // [textLength] NOT 0 TERMINATED!
	int textLength;
	float duration;
	List<int> codepoints;
};

struct Script
{
	Font font;
	Font boldFont;
	Font italicFont;
	Font boldItalicFont;
	int commandIndex; // Keeps track of which commands have already run so they don't run twice.
	char *text;
	List<char> stringPool; // This is where all expressions and speaker names are stored.
	List<Paragraph> paragraphs;
};

Script LoadScript(const char* path, Font regular, Font bold, Font italic, Font boldItalic); // Loads a script from the given text file.
void UnloadScript(Script* script); // Unloads all script memory and nullifies the script.
bool DrawScriptParagraph(Script* script, int paragraphIndex, Rectangle textBox, float fontSize, Color color, Color shadowColor, float time); // Draws the script paragraph at the given index. NOTE: This also executes console commands within the paragraph! Returns true if we're done with this paragraph.
const char* GetScriptExpression(Script script, int paragratphIndex, float time); // Gets the current speaker expression at the given time in the paragraph.

// Windowing

inline void ToggleFullscreen_FixedForWeb()
{
	// Raylib's ToggleFullscreen doesn't work on Web for some reason.
	#ifdef __EMSCRIPTEN__
	{
		EmscriptenFullscreenChangeEvent status;
		emscripten_get_fullscreen_status(&status);
		if (not status.isFullscreen)
			emscripten_request_fullscreen("#canvas", true);
		else
			emscripten_exit_fullscreen();
	}
	#else
	ToggleBorderlessWindowed();
	#endif
}
