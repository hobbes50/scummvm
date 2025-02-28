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

#include "common/config-manager.h"
#include "common/system.h"

#include "gui/message.h"
#include "engines/util.h"

#if defined(USE_OPENGL_GAME) || defined(USE_OPENGL_SHADERS) || defined(USE_GLES2)
#include "graphics/opengl/context.h"
#endif

#include "freescape/gfx.h"
#include "freescape/objects/object.h"

namespace Freescape {

Renderer::Renderer(int screenW, int screenH, Common::RenderMode renderMode) {
	_screenW = screenW;
	_screenH = screenH;
	_currentPixelFormat = Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0);
	_palettePixelFormat = Graphics::PixelFormat(3, 8, 8, 8, 0, 0, 8, 16, 0);
	_keyColor = -1;
	_palette = nullptr;
	_colorMap = nullptr;
	_renderMode = renderMode;
	_isAccelerated = false;
}

Renderer::~Renderer() {}

void Renderer::readFromPalette(uint8 index, uint8 &r, uint8 &g, uint8 &b) {
	r = _palette[3 * index + 0];
	g = _palette[3 * index + 1];
	b = _palette[3 * index + 2];
}

bool Renderer::getRGBAt(uint8 index, uint8 &r, uint8 &g, uint8 &b) {

	if (index == _keyColor)
		return false;

	if (index == 0) {
		readFromPalette(0, r, g, b);
		return true;
	}

	if (_renderMode == Common::kRenderAmiga || _renderMode == Common::kRenderAtariST) {
		readFromPalette(index, r, g, b);
		return true;
	}

	// assert(index-1 < _colorMap->size());
	byte *entry = (*_colorMap)[index - 1];
	uint8 color = 0;
	uint8 acc = 1;
	for (int i = 0; i < 4; i++) {
		byte be = *entry;
		if (be != 0 && be != 0xff) {
			// TODO: fix colors for non-DOS releases
			readFromPalette(index, r, g, b);
			return true;
		}

		if (be == 0xff)
			color = color + acc;

		acc = acc << 1;
		entry++;
	}
	assert(color < 16);
	readFromPalette(color, r, g, b);
	return true;
}

void Renderer::convertImageFormatIfNecessary(Graphics::Surface *surface) {
	if (!surface)
		return;

	if (surface->format != _texturePixelFormat)
		surface->convertToInPlace(_texturePixelFormat);
}

Common::Rect Renderer::viewport() const {
	return _screenViewport;
}

bool Renderer::computeScreenViewport() {
	int32 screenWidth = g_system->getWidth();
	int32 screenHeight = g_system->getHeight();

	Common::Rect viewport;
	if (g_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection)) {
			// Aspect ratio correction
			int32 viewportWidth = MIN<int32>(screenWidth, screenHeight * float(_screenW) / _screenH);
			int32 viewportHeight = MIN<int32>(screenHeight, screenWidth * float(_screenH) / _screenW);
			viewport = Common::Rect(viewportWidth, viewportHeight);

			// Pillarboxing
			viewport.translate((screenWidth - viewportWidth) / 2,
				(screenHeight - viewportHeight) / 2);
	} else {
			// Aspect ratio correction disabled, just stretch
			viewport = Common::Rect(screenWidth, screenHeight);
	}

	if (viewport == _screenViewport) {
		return false;
	}

	_screenViewport = viewport;
	return true;
}

void Renderer::renderPyramid(const Math::Vector3d &origin, const Math::Vector3d &size, const Common::Array<uint16> *ordinates, Common::Array<uint8> *colours, int type) {
	Math::Vector3d vertices[8] = { origin, origin, origin, origin, origin, origin, origin, origin };
	switch (type) {
	default:
		error("Invalid pyramid type: %d", type);
	case kEastPyramidType:
		vertices[0] += Math::Vector3d(0, 0, size.z());
		vertices[1] += Math::Vector3d(0, size.y(), size.z());
		vertices[2] += Math::Vector3d(0, size.y(), 0);

		vertices[4] += Math::Vector3d(size.x(), (*ordinates)[0], (*ordinates)[3]);
		vertices[5] += Math::Vector3d(size.x(), (*ordinates)[2], (*ordinates)[3]);
		vertices[6] += Math::Vector3d(size.x(), (*ordinates)[2], (*ordinates)[1]);
		vertices[7] += Math::Vector3d(size.x(), (*ordinates)[0], (*ordinates)[1]);
		break;
	case kWestPyramidType:

		vertices[0] += Math::Vector3d(size.x(), 0, 0);
		vertices[1] += Math::Vector3d(size.x(), size.y(), 0);
		vertices[2] += Math::Vector3d(size.x(), size.y(), size.z());
		vertices[3] += Math::Vector3d(size.x(), 0, size.z());

		vertices[4] += Math::Vector3d(0, (*ordinates)[0], (*ordinates)[1]);
		vertices[5] += Math::Vector3d(0, (*ordinates)[2], (*ordinates)[1]);
		vertices[6] += Math::Vector3d(0, (*ordinates)[2], (*ordinates)[3]);
		vertices[7] += Math::Vector3d(0, (*ordinates)[0], (*ordinates)[3]);
		break;

	case kUpPyramidType:
		vertices[1] += Math::Vector3d(size.x(), 0, 0);
		vertices[2] += Math::Vector3d(size.x(), 0, size.z());
		vertices[3] += Math::Vector3d(0, 0, size.z());

		vertices[4] += Math::Vector3d((*ordinates)[0], size.y(), (*ordinates)[1]);
		vertices[5] += Math::Vector3d((*ordinates)[2], size.y(), (*ordinates)[1]);
		vertices[6] += Math::Vector3d((*ordinates)[2], size.y(), (*ordinates)[3]);
		vertices[7] += Math::Vector3d((*ordinates)[0], size.y(), (*ordinates)[3]);
		break;

	case kDownPyramidType:

		vertices[0] += Math::Vector3d(size.x(), size.y(), 0);
		vertices[1] += Math::Vector3d(0, size.y(), 0);
		vertices[2] += Math::Vector3d(0, size.y(), size.z());
		vertices[3] += Math::Vector3d(size.x(), size.y(), size.z());

		vertices[4] += Math::Vector3d((*ordinates)[2], 0, (*ordinates)[1]);
		vertices[5] += Math::Vector3d((*ordinates)[0], 0, (*ordinates)[1]);
		vertices[6] += Math::Vector3d((*ordinates)[0], 0, (*ordinates)[3]);
		vertices[7] += Math::Vector3d((*ordinates)[2], 0, (*ordinates)[3]);
		break;

	case kNorthPyramidType:
		vertices[0] += Math::Vector3d(0, size.y(), 0);
		vertices[1] += Math::Vector3d(size.x(), size.y(), 0);
		vertices[2] += Math::Vector3d(size.x(), 0, 0);

		vertices[4] += Math::Vector3d((*ordinates)[0], (*ordinates)[3], size.z());
		vertices[5] += Math::Vector3d((*ordinates)[2], (*ordinates)[3], size.z());
		vertices[6] += Math::Vector3d((*ordinates)[2], (*ordinates)[1], size.z());
		vertices[7] += Math::Vector3d((*ordinates)[0], (*ordinates)[1], size.z());
		break;
	case kSouthPyramidType:
		vertices[0] += Math::Vector3d(0, 0, size.z());
		vertices[1] += Math::Vector3d(size.x(), 0, size.z());
		vertices[2] += Math::Vector3d(size.x(), size.y(), size.z());

		vertices[3] += Math::Vector3d(0, size.y(), size.z());
		vertices[4] += Math::Vector3d((*ordinates)[0], (*ordinates)[1], 0);
		vertices[5] += Math::Vector3d((*ordinates)[2], (*ordinates)[1], 0);
		vertices[6] += Math::Vector3d((*ordinates)[2], (*ordinates)[3], 0);
		vertices[7] += Math::Vector3d((*ordinates)[0], (*ordinates)[3], 0);
		break;
	}

	Common::Array<Math::Vector3d> face;
	uint8 r, g, b;
	if (getRGBAt((*colours)[0], r, g, b)) {
		useColor(r, g, b);

		face.push_back(vertices[4]);
		face.push_back(vertices[5]);
		face.push_back(vertices[1]);
		face.push_back(vertices[0]);

		renderFace(face);
		face.clear();
	}

	if (getRGBAt((*colours)[1], r, g, b)) {
		useColor(r, g, b);

		face.push_back(vertices[5]);
		face.push_back(vertices[6]);
		face.push_back(vertices[2]);
		face.push_back(vertices[1]);

		renderFace(face);
		face.clear();
	}

	if (getRGBAt((*colours)[2], r, g, b)) {
		useColor(r, g, b);

		face.push_back(vertices[6]);
		face.push_back(vertices[7]);
		face.push_back(vertices[3]);
		face.push_back(vertices[2]);
		renderFace(face);
		face.clear();
	}

	if (getRGBAt((*colours)[3], r, g, b)) {
		useColor(r, g, b);

		face.push_back(vertices[7]);
		face.push_back(vertices[4]);
		face.push_back(vertices[0]);
		face.push_back(vertices[3]);

		renderFace(face);
		face.clear();
	}

	if (getRGBAt((*colours)[4], r, g, b)) {
		useColor(r, g, b);

		face.push_back(vertices[0]);
		face.push_back(vertices[1]);
		face.push_back(vertices[2]);
		face.push_back(vertices[3]);
		renderFace(face);
		face.clear();
	}

	if (getRGBAt((*colours)[5], r, g, b)) {
		useColor(r, g, b);

		face.push_back(vertices[7]);
		face.push_back(vertices[6]);
		face.push_back(vertices[5]);
		face.push_back(vertices[4]);
		renderFace(face);
	}
}

void Renderer::renderCube(const Math::Vector3d &origin, const Math::Vector3d &size, Common::Array<uint8> *colours) {
	uint8 r, g, b;
	Common::Array<Math::Vector3d> face;

	if (getRGBAt((*colours)[0], r, g, b)) {
		useColor(r, g, b);
		face.push_back(origin);
		face.push_back(Math::Vector3d(origin.x(), origin.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x(), origin.y() + size.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x(), origin.y() + size.y(), origin.z()));
		renderFace(face);
	}

	if (getRGBAt((*colours)[1], r, g, b)) {
		useColor(r, g, b);
		face.clear();
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y() + size.y(), origin.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y() + size.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y(), origin.z()));
		renderFace(face);
	}

	if (getRGBAt((*colours)[2], r, g, b)) {
		useColor(r, g, b);
		face.clear();
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y(), origin.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x(), origin.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x(), origin.y(), origin.z()));
		renderFace(face);
	}

	if (getRGBAt((*colours)[3], r, g, b)) {
		useColor(r, g, b);
		face.clear();
		face.push_back(Math::Vector3d(origin.x(), origin.y() + size.y(), origin.z()));
		face.push_back(Math::Vector3d(origin.x(), origin.y() + size.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y() + size.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y() + size.y(), origin.z()));
		renderFace(face);
	}

	if (getRGBAt((*colours)[4], r, g, b)) {
		useColor(r, g, b);
		face.clear();
		face.push_back(Math::Vector3d(origin.x(), origin.y() + size.y(), origin.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y() + size.y(), origin.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y(), origin.z()));
		face.push_back(origin);
		renderFace(face);
	}

	if (getRGBAt((*colours)[5], r, g, b)) {
		useColor(r, g, b);
		face.clear();
		face.push_back(Math::Vector3d(origin.x(), origin.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x() + size.x(), origin.y() + size.y(), origin.z() + size.z()));
		face.push_back(Math::Vector3d(origin.x(), origin.y() + size.y(), origin.z() + size.z()));
		renderFace(face);
	}
}

void Renderer::renderRectangle(const Math::Vector3d &origin, const Math::Vector3d &size, Common::Array<uint8> *colours) {

	assert(size.x() == 0 || size.y() == 0 || size.z() == 0);
	polygonOffset(true);

	float dx, dy, dz;
	uint8 r, g, b;
	Common::Array<Math::Vector3d> vertices;
	for (int i = 0; i < 2; i++) {

		// debug("rec color: %d", (*colours)[i]);
		if (getRGBAt((*colours)[i], r, g, b)) {
			useColor(r, g, b);
			vertices.clear();
			vertices.push_back(Math::Vector3d(origin.x(), origin.y(), origin.z()));

			dx = dy = dz = 0.0;
			if (size.x() == 0) {
				dy = size.y();
			} else if (size.y() == 0) {
				dx = size.x();
			} else if (size.z() == 0) {
				dx = size.x();
			}

			vertices.push_back(Math::Vector3d(origin.x() + dx, origin.y() + dy, origin.z() + dz));
			vertices.push_back(Math::Vector3d(origin.x() + size.x(), origin.y() + size.y(), origin.z() + size.z()));
			renderFace(vertices);

			vertices.clear();
			vertices.push_back(Math::Vector3d(origin.x(), origin.y(), origin.z()));

			dx = dy = dz = 0.0;
			if (size.x() == 0) {
				dz = size.z();
			} else if (size.y() == 0) {
				dz = size.z();
			} else if (size.z() == 0) {
				dy = size.y();
			}

			vertices.push_back(Math::Vector3d(origin.x() + dx, origin.y() + dy, origin.z() + dz));
			vertices.push_back(Math::Vector3d(origin.x() + size.x(), origin.y() + size.y(), origin.z() + size.z()));
			renderFace(vertices);
		}
	}
	polygonOffset(false);
}

void Renderer::renderPolygon(const Math::Vector3d &origin, const Math::Vector3d &size, const Common::Array<uint16> *ordinates, Common::Array<uint8> *colours) {
	uint8 r, g, b;
	if (ordinates->size() % 3 > 0 && ordinates->size() > 0)
		error("Invalid polygon with size %f %f %f and ordinates %d", size.x(), size.y(), size.z(), ordinates->size());

	Common::Array<Math::Vector3d> vertices;
	polygonOffset(true);

	if (ordinates->size() == 6) {                 // Line
		assert(getRGBAt((*colours)[0], r, g, b)); // It will never return false?
		useColor(r, g, b);
		for (uint i = 0; i < ordinates->size(); i = i + 3)
			vertices.push_back(Math::Vector3d((*ordinates)[i], (*ordinates)[i + 1], (*ordinates)[i + 2]));
		renderFace(vertices);

		vertices.clear();
		assert(getRGBAt((*colours)[1], r, g, b)); // It will never return false?
		useColor(r, g, b);
		for (int i = ordinates->size(); i > 0; i = i - 3)
			vertices.push_back(Math::Vector3d((*ordinates)[i - 3], (*ordinates)[i - 2], (*ordinates)[i - 1]));
		renderFace(vertices);

	} else {
		if (getRGBAt((*colours)[0], r, g, b)) {
			useColor(r, g, b);
			for (uint i = 0; i < ordinates->size(); i = i + 3) {
				vertices.push_back(Math::Vector3d((*ordinates)[i], (*ordinates)[i + 1], (*ordinates)[i + 2]));
			}
			renderFace(vertices);
		}
		vertices.clear();
		if (getRGBAt((*colours)[1], r, g, b)) {
			useColor(r, g, b);
			for (int i = ordinates->size(); i > 0; i = i - 3) {
				vertices.push_back(Math::Vector3d((*ordinates)[i - 3], (*ordinates)[i - 2], (*ordinates)[i - 1]));
			}
			renderFace(vertices);
		}
	}

	polygonOffset(false);
}

Graphics::RendererType determinateRenderType() {
	Common::String rendererConfig = ConfMan.get("renderer");
	Graphics::RendererType desiredRendererType = Graphics::Renderer::parseTypeCode(rendererConfig);
	Graphics::RendererType matchingRendererType = Graphics::Renderer::getBestMatchingAvailableType(desiredRendererType,
#if defined(USE_OPENGL_GAME)
						Graphics::kRendererTypeOpenGL |
#endif
#if defined(USE_TINYGL)
						Graphics::kRendererTypeTinyGL |
#endif
						0);

	#if defined(USE_TINYGL) // Prefer TinyGL until OpenGL is good enough
	if (desiredRendererType == Graphics::kRendererTypeDefault)
		matchingRendererType = desiredRendererType = Graphics::kRendererTypeTinyGL;
	#endif

	if (matchingRendererType != desiredRendererType && desiredRendererType != Graphics::kRendererTypeDefault) {
		// Display a warning if unable to use the desired renderer
		warning("Unable to create a '%s' renderer", rendererConfig.c_str());
	}

	#if defined(USE_OPENGL_GAME) && !defined(USE_GLES2)
		if (matchingRendererType == Graphics::kRendererTypeOpenGL)
			return matchingRendererType;
	#endif

	#if defined(USE_TINYGL)
	if (desiredRendererType == Graphics::kRendererTypeTinyGL)
		return desiredRendererType;
	#endif

	return Graphics::kRendererTypeDefault;
}

Renderer *createRenderer(int screenW, int screenH, Common::RenderMode renderMode) {
	Graphics::PixelFormat pixelFormat = Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::RendererType rendererType = determinateRenderType();

	bool isAccelerated = rendererType != Graphics::kRendererTypeTinyGL;

	if (isAccelerated) {
		initGraphics3d(screenW, screenH);
	} else {
		initGraphics(screenW, screenH, &pixelFormat);
	}

	#if defined(USE_OPENGL_GAME) && !defined(USE_GLES2)
		if (rendererType == Graphics::kRendererTypeOpenGL) {
			return CreateGfxOpenGL(screenW, screenH, renderMode);
		}
	#endif

	#if defined(USE_TINYGL)
	if (rendererType == Graphics::kRendererTypeTinyGL) {
		return CreateGfxTinyGL(screenW, screenH, renderMode);
	}
	#endif

	GUI::MessageDialog dialog("No available renderers enabled");
	// TODO: improve message with other renders
	dialog.runModal();
	return nullptr;
}

} // End of namespace Freescape
