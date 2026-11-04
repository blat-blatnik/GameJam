#pragma once
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <stdio.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// Generic

constexpr auto Abs(auto x) { return x < 0 ? -x : x; }
constexpr auto Min(auto a, auto b) { return a < b ? a : b; }
constexpr auto Max(auto a, auto b) { return a > b ? a : b; }

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
