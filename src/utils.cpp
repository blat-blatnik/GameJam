#include "utils.h"

// Assets

template<class T, int Capacity> struct AssetTable
{
	const char* Keys[Capacity];
	T Values[Capacity];

	T& operator[](const char* key)
	{
		unsigned hash = Hash(key);
		unsigned mask = Capacity - 1;
		for (unsigned i = (hash & mask);; i = (i + 1) & mask)
		{
			if (Keys[i] == nullptr or TextIsEqual(key, Keys[i]))
			{
				Keys[i] = key;
				return Values[i];
			}
		}
	}
};

AssetTable<Texture, 4096> TextureTable;
AssetTable<Sound, 1024> SoundTable;

Texture GetTexture(const char* filepath)
{
	Texture* texture = &TextureTable[filepath];
	if (texture->id == 0)
	{
		const char* artPath = TextFormat("art/%s", filepath);
		*texture = LoadTexture(artPath);
	}
	return *texture;
}
Sound GetSound(const char* filepath)
{
	Sound* sound = &SoundTable[filepath];
	if (sound->stream.buffer == nullptr)
	{
		const char* artPath = TextFormat("art/%s", filepath);
		*sound = LoadSound(artPath);
	}
	return *sound;
}

// Slideshow

Slideshow LoadSlideshow(const char* filepath)
{
	Slideshow slideshow = {};

	char* fileText = LoadFileText(filepath);

	int numLines;
	char** lines = LoadTextLines(fileText, &numLines);

	Slide slide = {};
	for (int lineIndex = 0; lineIndex < numLines; lineIndex++)
	{
		char* line = lines[lineIndex];
		if (*line == '\0' and (slide.lines.count > 0 or slide.frames.count > 0))
		{
			slideshow.slides.Add(slide);
			slide = {};
		}
		else if (*line == ':')
		{
			const char* frameFilenamesString = line + 1;
			int numFrames;
			char** frameFilenames = TextSplit(frameFilenamesString, ',', &numFrames);
			for (int i = 0; i < numFrames; i++)
				slide.frames.Add(GetTexture(frameFilenames[i]));
		}
		else
		{
			int size = TextLength(line) + 1;
			char* copy = (char*)MemAlloc(size);
			memcpy(copy, line, size);
			slide.lines.Add(copy);
		}
	}
	if (slide.lines.count > 0 or slide.frames.count > 0)
	{
		slideshow.slides.Add(slide);
		slide = {};
	}

	UnloadTextLines(lines, numLines);
	UnloadFileText(fileText);
	return slideshow;
}

// Script
// 
// I took this code from an old failed game jam project I worked on. Here is the original comment I added to that code haha:
// 
// > This code is pretty terrible, much harder to understand than I would have liked. 
// > I really hope we don't have to change/add stuff.
// > The problem of parsing these files is pretty complicated for me, so I just made it work as best I could.
// > If I had more time I could have made it better. But I spent like 2 days on this, I should move on to other things..

#define CONTROL(codepoint) -codepoint
#define IS_CONTROL(codepoint) (codepoint < 0)

enum Style
{
	REGULAR = 0,
	BOLD = 1 << 0,
	ITALIC = 1 << 1,
	BOLD_ITALIC = BOLD | ITALIC,
	STYLE_ENUM_COUNT
};

struct Word
{
	int start;
	int length;
	float width;
};

static float GetAdvance(Font font, float fontSize, int glyphIndex)
{
	float scaleFactor = fontSize / font.baseSize;
	if (font.glyphs[glyphIndex].advanceX == 0)
		return scaleFactor * font.recs[glyphIndex].width;
	else
		return scaleFactor * font.glyphs[glyphIndex].advanceX;
}
static List<int> ConvertToCodepoints(const char *text, int length, List<char> *stringPool)
{
	List<int> codepoints = {};

	int lastNonPauseCodepoint = 0;
	for (int i = 0; i < length;)
	{
		int advance;
		int codepoint = GetCodepoint(text + i, &advance);
		if (!codepoint)
			break;
		i += advance;

		int lastIndex = codepoints.count - 1;
		bool isEscaped = lastIndex > 0 and codepoints[lastIndex] == CONTROL('\\');

		if ((codepoint == '[' or codepoint == '{') and not isEscaped)
		{

			// Expressions go into string memory, and get encoded as CONTROL('[') followed by an index into string memory.
			int start = i;
			char closing = codepoint == '[' ? ']' : '}';
			while (i < length and text[i] != closing)
				++i;

			codepoints.Add(CONTROL(codepoint));
			int stringLength = i - start;
			if (stringLength > 0)
			{
				int stringIndex = stringPool->count;
				char *string = stringPool->Alloc(stringLength + 1);
				memcpy(string, text + start, stringLength);
				string[stringLength] = 0;
				codepoints.Add(stringIndex);
			}
			else codepoints.Add(-1); // -1 means "default" expression.

			++i; // Skip the ]
		}
		else if (codepoint == '\\' and not isEscaped)
			codepoints.Add(CONTROL('\\'));
		else if (codepoint == '|' and not isEscaped)
			codepoints.Add(CONTROL('|'));
		else if (codepoint == '_' and not isEscaped)
			codepoints.Add(CONTROL('_'));
		else if (codepoint == '*' and not isEscaped)
			codepoints.Add(CONTROL('*'));
		else if (codepoint == '`' and not isEscaped)
			codepoints.Add(CONTROL('`'));
		else if (codepoint == ' ' and isEscaped)
			codepoints[lastIndex] = ' ';
		else if (codepoint == 'n' and isEscaped)
			codepoints[lastIndex] = '\n';
		else if (codepoint == '[' and isEscaped)
			codepoints[lastIndex] = '[';
		else if (codepoint == ']' and isEscaped)
			codepoints[lastIndex] = ']';
		else if (codepoint == '{' and isEscaped)
			codepoints[lastIndex] = '{';
		else if (codepoint == '}' and isEscaped)
			codepoints[lastIndex] = '}';
		else if (codepoint == '|' and isEscaped)
			codepoints[lastIndex] = '|';
		else if (codepoint == '_' and isEscaped)
			codepoints[lastIndex] = '_';
		else if (codepoint == '*' and isEscaped)
			codepoints[lastIndex] = '*';
		else if (codepoint == '`' and isEscaped)
			codepoints[lastIndex] = '`';
		else if (codepoint == '\\' and isEscaped)
			codepoints[lastIndex] = '\\';
		else if (codepoint == ' ' and not isEscaped and lastNonPauseCodepoint == ' ')
			codepoints.Add(CONTROL('`'));
		else if (codepoint == ',' and not isEscaped)
		{
			codepoints.Add(codepoint);
			codepoints.Add(CONTROL('`'));
			codepoints.Add(CONTROL('`'));
		}
		else if ((codepoint == '.' or codepoint == '!' or codepoint == '?') and not isEscaped)
		{
			codepoints.Add(codepoint);
			codepoints.Add(CONTROL('`'));
			codepoints.Add(CONTROL('`'));
			codepoints.Add(CONTROL('`'));
			codepoints.Add(CONTROL('`'));
		}
		else
			codepoints.Add(codepoint);

		int lastCodepoint = codepoints[codepoints.count - 1];
		if (lastCodepoint != CONTROL('`'))
			lastNonPauseCodepoint = lastCodepoint;
	}

	// Remove pauses at the end.
	while (codepoints.count > 0 and codepoints[codepoints.count - 1] == CONTROL('`'))
		codepoints.Pop();

	return codepoints;
}
static float MeasureDuration(List<int> codepoints)
{
	int numCodepoints = codepoints.count;
	int duration = 0;
	for (int i = 0; i < numCodepoints; ++i)
	{
		int codepoint = codepoints[i];
		if (not IS_CONTROL(codepoint) or codepoint == CONTROL('`'))
			++duration;

		if (codepoint == CONTROL('[') or codepoint == CONTROL('{'))
			++i;
	}

	return (float)duration;
}
static bool IsWhitespace(int codepoint)
{
	return codepoint < 128 && isspace(codepoint);
}

void ExecuteScriptCommand(const char* command); // Defined in main.cpp (yes I know it's a mess D:)

Script LoadScript(const char *path, Font regular, Font bold, Font italic, Font boldItalic)
{
	Script script = {
		.font = regular,
		.boldFont = bold,
		.italicFont = italic,
		.boldItalicFont = boldItalic,
		.text = LoadFileText(path),
	};

	if (not script.text)
	{
		TraceLog(LOG_ERROR, "Failed to load script file '%s'.", path);
		return script;
	}

	int fileCursor = 0;
	for (;;)
	{
		Paragraph paragraph = {};

		while (isspace(script.text[fileCursor]))
			++fileCursor;
		char *text = script.text + fileCursor;
		if (!text[0])
			break;

		// Find out where the paragraph ends: 
		// - either at the end of the text, 
		// - or after 2 consecutive newlines with nothing but whitespace between them.
		int cursor;
		int lastNonWhitespace = 0;
		for (cursor = 1; text[cursor] != '\0'; cursor++)
		{
			if (not isspace(text[cursor]))
			{
				lastNonWhitespace = cursor;
			}
			else if (text[cursor] == '\n')
			{
				bool consecutiveNewline = false;
				while (isspace(text[cursor + 1]))
				{
					if (text[cursor + 1] == '\n')
					{
						consecutiveNewline = true;
						break;
					}
					cursor++;
				}
				if (consecutiveNewline)
					break;
			}
		}
		int textLength = lastNonWhitespace + 1;

		// Extract speaker name.
		bool foundSpeaker = false;
		if (text[0] == '[')
		{
			int speakerStart = 1;
			int speakerEnd = -1;
			char *nextLine = NULL;
			for (int i = speakerStart; i < textLength; ++i)
			{
				if (text[i] == '\n')
					break; // Speaker name has to be on the first line, so if we don't find a ] on the first line we don't have a name.
				if (text[i] == ']')
				{
					// Now we've found the [abc] part, we need to make sure the rest of the line is empty.
					// If the line isn't empty, then this is an expression, not a speaker name.
					char *line = text + i + 1;
					bool onlyWhitespaceAfterSpeaker = true;
					for (int j = 0; j < textLength; ++j)
					{
						if (line[j] == '\n')
						{
							nextLine = line + j + 1; // Record where the actual text starts after the speaker.
							break;
						}
						if (not isspace(line[j]))
						{
							onlyWhitespaceAfterSpeaker = false;
							break;
						}
					}

					if (onlyWhitespaceAfterSpeaker)
					{
						speakerEnd = i;
					}
					break;
				}
			}

			if (speakerEnd != -1)
			{
				foundSpeaker = true;
				int nameLength = speakerEnd - speakerStart;
				if (nameLength > 0)
				{
					//paragraph.speaker = ListAllocate(&script.stringPool, nameLength + 1);
					paragraph.speaker = (char*)MemAlloc(nameLength + 1);
					memcpy(paragraph.speaker, text + speakerStart, nameLength);
					paragraph.speaker[nameLength] = 0;
				}
				else paragraph.speaker = NULL; // Use the default name.

				int skip = (int)(nextLine - text);
				text = nextLine;
				textLength -= skip;
			}
		}
		if (not foundSpeaker)
		{
			// If we didn't find a name, the character stays the same between paragraphs.
			const char *previousName = NULL;
			if (script.paragraphs.count > 0)
				previousName = script.paragraphs[script.paragraphs.count - 1].speaker;

			if (previousName)
			{
				int nameLength = TextLength(previousName);
				//paragraph.speaker = ListAllocate(&script.stringPool, nameLength + 1);
				paragraph.speaker = (char*)MemAlloc(nameLength + 1);
				memcpy(paragraph.speaker, previousName, nameLength);
				paragraph.speaker[nameLength] = 0;
			}
			else paragraph.speaker = NULL;
		}

		fileCursor += cursor;
		paragraph.text = text;
		paragraph.textLength = textLength;
		paragraph.codepoints = ConvertToCodepoints(text, textLength, &script.stringPool);
		paragraph.duration = MeasureDuration(paragraph.codepoints);
		script.paragraphs.Add(paragraph);
	}

	TraceLog(LOG_INFO, "Script '%s' loaded successfully (%d paragraphs).", path, script.paragraphs.count);
	return script;
}
void UnloadScript(Script* script)
{
	if (not script or not script->text)
		return;

	for (int i = 0; i < script->paragraphs.count; ++i)
	{
		Paragraph *paragraph = &script->paragraphs[i];
		paragraph->codepoints.Free();
		MemFree(paragraph->speaker);
	}
	script->paragraphs.Free();
	script->stringPool.Free();
	UnloadFileText(script->text);
	script->text = NULL;
	script->commandIndex = 0;
	TraceLog(LOG_INFO, "Script unloaded.");
}
bool DrawScriptParagraph(Script* script, int paragraphIndex, Rectangle textBox, float fontSize, Color color, Color shadowColor, float time)
{
	paragraphIndex = ClampInt(paragraphIndex, 0, script->paragraphs.count - 1);
	Paragraph paragraph = script->paragraphs[paragraphIndex];
	List<int> codepoints = paragraph.codepoints;
	int numCodepoints = codepoints.count;

	Font fonts[STYLE_ENUM_COUNT] = {};
	fonts[REGULAR] = script->font;
	fonts[BOLD] = script->boldFont;
	fonts[ITALIC] = script->italicFont;
	fonts[BOLD_ITALIC] = script->boldItalicFont;

	float x = textBox.x;
	float y = textBox.y;
	float t = 0;
	Style style = REGULAR;
	bool group = false;
	float maxX = textBox.x + textBox.width;
	int commandIndex = 0;

	int codepointIndex;
	for (codepointIndex = 0; codepointIndex < numCodepoints and (group or t < time); ++codepointIndex)
	{
		int codepoint = codepoints[codepointIndex];
		if (codepoint == CONTROL('['))
		{
			++codepointIndex; // Skip the string index.
		}
		else if (codepoint == CONTROL('{'))
		{
			int stringIndex = codepoints[++codepointIndex];
			char *command = &script->stringPool[stringIndex];
			if (++commandIndex > script->commandIndex)
			{
				script->commandIndex++;
				TraceLog(LOG_INFO, "Script executing command %d: '%s'.", script->commandIndex, command);
				ExecuteScriptCommand(command);
			}
		}
		else if (codepoint == CONTROL('*'))
		{
			style = (Style)(style ^ BOLD);
		}
		else if (codepoint == CONTROL('_'))
		{
			style = (Style)(style ^ ITALIC);
		}
		else if (codepoint == CONTROL('|'))
		{
			group = not group;
		}
		else if (IsWhitespace(codepoint) or codepoint == CONTROL('`'))
		{
			t += 1;
			if (codepoint == '\n')
			{
				x = textBox.x;
				y += GetLineHeight(fonts[style], fontSize);
			}
			else if (codepoint != CONTROL('`'))
			{
				Font font = fonts[style];
				int index = GetGlyphIndex(font, codepoint);
				x += GetAdvance(font, fontSize, index);
				if (x > maxX)
				{
					x = textBox.x;
					y += GetLineHeight(fonts[style], fontSize);
				}
			}
		}
		else
		{
			//@TODO @SPEED This is EXTREMELY inneficient
			// We draw every word character by character, but we scan through it to see if it will fit on the line every single time
			// so it's O(n^2) performance when it could easily be O(1). Could be fixed, but it would require basically copy pasting the
			// above control code checks into the loop of the word drawing itself, and I don't wanna do that right now soo...

			// Check if the word will fit on the line, and if it doesn't, break the line before drawing the word.
			float width = 0;
			Style backupStyle = style;
			for (int j = codepointIndex; j < numCodepoints and not IsWhitespace(codepoints[j]) and codepoints[j] != CONTROL('['); ++j)
			{
				codepoint = codepoints[j];
				if (codepoint == CONTROL('*'))
					style = (Style)(style ^ BOLD);
				else if (codepoint == CONTROL('_'))
					style = (Style)(style ^ ITALIC);
				else if (not IS_CONTROL(codepoint))
				{
					Font font = fonts[style];
					int index = GetGlyphIndex(font, codepoint);
					width += GetAdvance(font, fontSize, index);
				}
			}

			// Word is too long to fit onto current line. Break the line.
			if (x + width > maxX)
			{
				x = textBox.x;
				y += GetLineHeight(fonts[style], fontSize);
			}

			style = backupStyle;
			codepoint = codepoints[codepointIndex];

			// @SPEED Here we could draw the entire word instead of just the one character.
			{
				Font font = fonts[style];
				int index = GetGlyphIndex(font, codepoint);
				DrawTextCodepoint(font, codepoint, { x + 1, y + 1 }, fontSize, shadowColor);
				DrawTextCodepoint(font, codepoint, { x, y }, fontSize, color);
				x += GetAdvance(font, fontSize, index);
				t += 1;
			}
		}
	}

	// Return if we're done.
	return codepointIndex >= numCodepoints;
}
const char* GetScriptExpression(Script script, int paragraphIndex, float time)
{
	//@SPEED This basically has to crawl through the entire script to find the expression.
	const char *prevSpeaker = "";
	const char *expression = "default";
	paragraphIndex = ClampInt(paragraphIndex, 0, script.paragraphs.count - 1);

	for (int i = 0; i <= paragraphIndex; ++i)
	{
		Paragraph paragraph = script.paragraphs[i];
		if (not TextIsEqual(paragraph.speaker, prevSpeaker))
		{
			prevSpeaker = paragraph.speaker;
			expression = "default";
		}

		List<int> codepoints = paragraph.codepoints;
		int numCodepoints = codepoints.count;
		float t = 0;
		for (int j = 0; j < numCodepoints and (t < time or i != paragraphIndex); ++j)
		{
			int codepoint = codepoints[j];
			if (codepoint == CONTROL('['))
			{
				int stringIndex = codepoints[++j];
				if (stringIndex == -1)
					expression = "default";
				else
					expression = &script.stringPool[stringIndex];
			}
			else if (codepoint == CONTROL('{'))
			{
				++j; // Skip the string index.
			}
			else if (not IS_CONTROL(codepoint) or codepoint == CONTROL('`'))
			{
				t += 1;
			}
		}
	}

	return expression;
}
