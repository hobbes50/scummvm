/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/file.h"
#include "common/str.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/winexe_ne.h"
#include "common/winexe_pe.h"
#include "graphics/surface.h"
#include "graphics/fonts/winfont.h"

namespace Graphics {

WinFont::WinFont() {
	_glyphs = 0;
	close();
}

WinFont::~WinFont() {
	close();
}

void WinFont::close() {
	_pixHeight = 0;
	_maxWidth = 0;
	_firstChar = 0;
	_lastChar = 0;
	_defaultChar = 0;
	_glyphCount = 0;
	delete[] _glyphs;
	_glyphs = 0;
}

// Reads a null-terminated string
static Common::String readString(Common::SeekableReadStream &stream) {
	Common::String string;

	char c = stream.readByte();
	while (c && stream.pos() < stream.size()) {
		string += c;
		c = stream.readByte();
	}

	return string;
}

static WinFontDirEntry readDirEntry(Common::SeekableReadStream &stream) {
	WinFontDirEntry entry;

	stream.skip(68); // Useless
	entry.points = stream.readUint16LE();
	stream.skip(43); // Useless (for now, maybe not in the future)
	readString(stream); // Skip Device Name
	entry.faceName = readString(stream);

	return entry;
}

bool WinFont::loadFromFON(const Common::String &fileName, const WinFontDirEntry &dirEntry) {
	Common::WinResources *exe = Common::WinResources::createFromEXE(fileName);
	if (!exe)
		return false;

	bool ok = loadFromEXE(exe, fileName, dirEntry);
	delete exe;
	return ok;
}

bool WinFont::loadFromFON(Common::SeekableReadStream &stream, const WinFontDirEntry &dirEntry) {
	Common::WinResources *exe = Common::WinResources::createFromEXE(&stream);
	if (!exe)
		return false;

	bool ok = loadFromEXE(exe, Common::String("stream"), dirEntry);
	delete exe;
	return ok;
}

bool WinFont::loadFromEXE(Common::WinResources *exe, const Common::String &fileName, const WinFontDirEntry &dirEntry) {
	// Let's pull out the font directory
	Common::SeekableReadStream *fontDirectory = exe->getResource(Common::kWinFontDir, Common::String("FONTDIR"));
	if (!fontDirectory) {
		warning("No font directory in '%s'", fileName.c_str());
		return false;
	}

	uint32 fontId = getFontIndex(*fontDirectory, dirEntry);

	delete fontDirectory;

	// Couldn't match the face name
	if (fontId == 0xffffffff) {
		warning("Could not find face '%s' in '%s'", dirEntry.faceName.c_str(), fileName.c_str());
		return false;
	}

	// Actually go get our font now...
	Common::SeekableReadStream *fontStream = exe->getResource(Common::kWinFont, fontId);
	if (!fontStream) {
		warning("Could not find font %d in %s", fontId, fileName.c_str());
		return false;
	}

	bool ok = loadFromFNT(*fontStream);
	delete fontStream;
	return ok;
}

uint32 WinFont::getFontIndex(Common::SeekableReadStream &stream, const WinFontDirEntry &dirEntry) {
	uint16 numFonts = stream.readUint16LE();

	// Probably not possible, so this is really a sanity check
	if (numFonts == 0) {
		warning("No fonts in exe");
		return 0xffffffff;
	}

	// Scour the directory for our matching name
	for (uint16 i = 0; i < numFonts; i++) {
		uint16 id = stream.readUint16LE();

		// Use the first name when empty
		if (dirEntry.faceName.empty()) {
			_name = getFONFontName(stream);
			return id;
		}

		WinFontDirEntry entry = readDirEntry(stream);

		if (dirEntry.faceName.equalsIgnoreCase(entry.faceName) && dirEntry.points == entry.points) // Match!
			return id;
	}

	return 0xffffffff;
}

Common::String WinFont::getFONFontName(Common::SeekableReadStream& stream) {
	// Currently only works when dirEntry.faceName in getFontIndex is empty
	// But this can be used for each FONTDIR entry
	stream.seek(117);
	/* Device Name = */ stream.readString();
	Common::String fontName = stream.readString();
	return fontName;
}

bool WinFont::loadFromFNT(const Common::String &fileName) {
	Common::File file;

	return file.open(fileName) && loadFromFNT(file);
}

char WinFont::indexToCharacter(uint16 index) const {
	// Use a space for the sentinel value
	if (index == _glyphCount - 1)
		return ' ';

	return index + _firstChar;
}

uint16 WinFont::characterToIndex(uint32 character) const {
	// Go to the default character if we didn't find a mapping
	if (character < _firstChar || character > _lastChar)
		character = _defaultChar;

	return character - _firstChar;
}

int WinFont::getCharWidth(uint32 chr) const {
	return _glyphs[characterToIndex(chr)].charWidth;
}

bool WinFont::loadFromFNT(Common::SeekableReadStream &stream) {
	uint16 version = stream.readUint16LE();

	// We'll accept Win1, Win2, and Win3 fonts
	if (version != 0x100 && version != 0x200 && version != 0x300) {
		warning("Bad FNT version %04x", version);
		return false;
	}

	/* uint32 size = */ stream.readUint32LE();
	stream.skip(60); // Copyright info
	uint16 fontType = stream.readUint16LE();
	/* uint16 points = */ stream.readUint16LE();
	/* uint16 vertRes = */ stream.readUint16LE();
	/* uint16 horizRes = */ stream.readUint16LE();
	_ascent = stream.readUint16LE();
	/* uint16 internalLeading = */ stream.readUint16LE();
	/* uint16 externalLeading = */ stream.readUint16LE();
	_italic = stream.readByte();
	_underline = stream.readByte();
	_strikethrough = stream.readByte();
	_weight = stream.readUint16LE();
	/* byte charSet = */ stream.readByte();
	uint16 pixWidth = stream.readUint16LE();
	_pixHeight = stream.readUint16LE();
	/* byte pitchAndFamily = */ stream.readByte();
	/* uint16 avgWidth = */ stream.readUint16LE();
	_maxWidth = stream.readUint16LE();
	_firstChar = stream.readByte();
	_lastChar = stream.readByte();
	_defaultChar = stream.readByte();
	/* byte breakChar = */ stream.readByte();
	/* uint16 widthBytes = */ stream.readUint16LE();
	/* uint32 device = */ stream.readUint32LE();
	/* uint32 face = */ stream.readUint32LE();
	/* uint32 bitsPointer = */ stream.readUint32LE();
	uint32 bitsOffset = stream.readUint32LE();
	/* byte reserved = */ stream.readByte();

	if (version == 0x100) {
		// Seems Win1 has an extra byte?
		stream.readByte();
	} else if (version == 0x300) {
		// For Windows 3.0, Microsoft added 6 new fields. All of which are
		// guaranteed to be 0. Which leads to the question: Why add these at all?

		/* uint32 flags = */ stream.readUint32LE();
		/* uint16 aSpace = */ stream.readUint16LE();
		/* uint16 bSpace = */ stream.readUint16LE();
		/* uint16 cSpace = */ stream.readUint16LE();
		/* uint32 colorPointer = */ stream.readUint32LE();
		stream.skip(16); // Reserved
	}

	// Begin loading in the glyphs
	_glyphCount = (_lastChar - _firstChar) + 2;
	_glyphs = new GlyphEntry[_glyphCount];

	for (uint16 i = 0; i < _glyphCount; i++) {
		_glyphs[i].charWidth = stream.readUint16LE();

		// Use the default if present
		if (pixWidth)
			_glyphs[i].charWidth = pixWidth;

		_glyphs[i].offset = (version == 0x300) ? stream.readUint32LE() : stream.readUint16LE();

		// Seems the offsets in the Win1 font format are based on bitsOffset
		if (version == 0x100)
			_glyphs[i].offset += bitsOffset;
	}

	// TODO: Currently only raster fonts are supported!
	if (fontType & 1) {
		warning("Vector FNT files not supported yet");
		return false;
	}

	// Read in the bitmaps for the raster images
	for (uint16 i = 0; i < _glyphCount - 1; i++) {
		stream.seek(_glyphs[i].offset);

		_glyphs[i].bitmap = new byte[_pixHeight * _glyphs[i].charWidth];

		// Calculate the amount of columns
		byte colCount = (_glyphs[i].charWidth + 7) / 8;

		for (uint16 j = 0; j < colCount; j++) {
			for (uint16 k = 0; k < _pixHeight; k++) {
				byte x = stream.readByte();
				uint offset = j * 8 + k * _glyphs[i].charWidth;

				for (byte l = 0; l < 8 && j * 8 + l < _glyphs[i].charWidth; l++)
					_glyphs[i].bitmap[offset + l] = (x & (1 << (7 - l))) ? 1 : 0;
			}
		}

#if 0
		// Debug print
		debug("Character %02x '%c' at %08x", indexToCharacter(i), indexToCharacter(i), _glyphs[i].offset);
		for (uint16 j = 0; j < _pixHeight; j++) {
			for (uint16 k = 0; k < _glyphs[i].charWidth; k++)
				debugN("%c", _glyphs[i].bitmap[k + j * _glyphs[i].charWidth] ? 'X' : ' ');

			debugN("\n");
		}
#endif
	}

	return true;
}

void WinFont::drawChar(Surface *dst, uint32 chr, int x, int y, uint32 color) const {
	assert(dst);
	assert(dst->format.bytesPerPixel == 1 || dst->format.bytesPerPixel == 2 || dst->format.bytesPerPixel == 4);
	assert(_glyphs);

	GlyphEntry &glyph = _glyphs[characterToIndex(chr)];

	for (uint16 i = 0; i < _pixHeight; i++) {
		for (uint16 j = 0; j < glyph.charWidth; j++) {
			if (glyph.bitmap[j + i * glyph.charWidth]) {
				if (dst->format.bytesPerPixel == 1)
					*((byte *)dst->getBasePtr(x + j, y + i)) = color;
				else if (dst->format.bytesPerPixel == 2)
					*((uint16 *)dst->getBasePtr(x + j, y + i)) = color;
				else if (dst->format.bytesPerPixel == 4)
					*((uint32 *)dst->getBasePtr(x + j, y + i)) = color;
			}
		}
	}
}

int WinFont::getStyle() {
	int style = kFontStyleRegular;

	// This has been taken from Wine Source
	// https://github.com/wine-mirror/wine/blob/b9a61cde89e5dc6264b4c152f4dc24ecf064f8f6/include/wingdi.h#L728

	if (_weight >= 700)
		style |= kFontStyleBold;
	if (_italic)
		style |= kFontStyleItalic;
	if (_underline)
		style |= kFontStyleUnderline;

	return style;
}

} // End of namespace Graphics
