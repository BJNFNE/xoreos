/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010-2011 Sven Hesse (DrMcCoy), Matthew Hoops (clone2727)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

/** @file graphics/aurora/modelnode.cpp
 *  A node within a 3D model.
 */

#include "common/util.h"
#include "common/maths.h"

#include "graphics/graphics.h"
#include "graphics/camera.h"

#include "graphics/images/txi.h"

#include "graphics/aurora/modelnode.h"
#include "graphics/aurora/model.h"
#include "graphics/aurora/texture.h"

namespace Graphics {

namespace Aurora {

static bool nodeComp(ModelNode *a, ModelNode *b) {
	return a->isInFrontOf(*b);
}


ModelNode::ModelNode(Model &model) :
	_model(&model), _parent(0), _level(0),
	_faceCount(0), _coords(0), _smoothGroups(0), _material(0), _isTransparent(false),
	_constraints(0), _render(false), _hasTransparencyHint(false),
	_needRebuild(true), _list(0) {

	_position[0] = 0.0; _position[1] = 0.0; _position[2] = 0.0;
	_rotation[0] = 0.0; _rotation[1] = 0.0; _rotation[2] = 0.0;

	_orientation[0] = 0.0;
	_orientation[1] = 0.0;
	_orientation[2] = 0.0;
	_orientation[3] = 0.0;
}

ModelNode::~ModelNode() {
	if (_list != 0)
		GfxMan.abandon(_list, 1);

	delete[] _constraints;
	delete[] _material;
	delete[] _smoothGroups;
	delete[] _coords;
}

ModelNode *ModelNode::getParent() {
	return _parent;
}

const ModelNode *ModelNode::getParent() const {
	return _parent;
}

void ModelNode::setParent(ModelNode *parent) {
	_parent = parent;
}

const Common::UString &ModelNode::getName() const {
	return _name;
}

float ModelNode::getWidth() const {
	return _boundBox.getWidth() * _model->_modelScale[0];
}

float ModelNode::getHeight() const {
	return _boundBox.getHeight() * _model->_modelScale[1];
}

float ModelNode::getDepth() const {
	return _boundBox.getDepth() * _model->_modelScale[2];
}

bool ModelNode::isInFrontOf(const ModelNode &node) const {
	assert(_model == node._model);

	if (_model->getType() == kModelTypeGUIFront)
		return _position[2] > node._position[2];

	return _position[2] < node._position[2];
}

void ModelNode::getPosition(float &x, float &y, float &z) const {
	x = _position[0] * _model->_modelScale[0];
	y = _position[1] * _model->_modelScale[1];
	z = _position[2] * _model->_modelScale[2];
}

void ModelNode::getRotation(float &x, float &y, float &z) const {
	x = _rotation[0];
	y = _rotation[1];
	z = _rotation[2];
}

void ModelNode::getOrientation(float &x, float &y, float &z, float &a) const {
	x = _orientation[0];
	y = _orientation[1];
	z = _orientation[2];
	a = _orientation[3];
}

void ModelNode::getAbsolutePosition(float &x, float &y, float &z) const {
	x = _absolutePosition.getX() * _model->_modelScale[0];
	y = _absolutePosition.getY() * _model->_modelScale[1];
	z = _absolutePosition.getZ() * _model->_modelScale[2];
}

void ModelNode::setPosition(float x, float y, float z) {
	GfxMan.lockFrame();

	_position[0] = x / _model->_modelScale[0];
	_position[1] = y / _model->_modelScale[1];
	_position[2] = z / _model->_modelScale[2];

	if (_parent)
		_parent->orderChildren();

	GfxMan.unlockFrame();
}

void ModelNode::setRotation(float x, float y, float z) {
	GfxMan.lockFrame();

	_rotation[0] = x;
	_rotation[1] = y;
	_rotation[2] = z;

	GfxMan.unlockFrame();
}

void ModelNode::move(float x, float y, float z) {
	float curX, curY, curZ;
	getPosition(curX, curY, curZ);

	setPosition(curX + x, curY + y, curZ + z);
}

void ModelNode::rotate(float x, float y, float z) {
	setRotation(_rotation[0] + x, _rotation[1] + y, _rotation[2] + z);
}

void ModelNode::inheritPosition(ModelNode &node) const {
	node._position[0] = _position[0];
	node._position[1] = _position[1];
	node._position[2] = _position[2];
}

void ModelNode::inheritOrientation(ModelNode &node) const {
	node._orientation[0] = _orientation[0];
	node._orientation[1] = _orientation[1];
	node._orientation[2] = _orientation[2];
	node._orientation[3] = _orientation[3];
}

void ModelNode::inheritGeometry(ModelNode &node) const {
	node._textures = _textures;

	node._render        = _render;
	node._isTransparent = _isTransparent;

	if (!node.createFaces(_faceCount))
		return;

	memcpy(node._coords, _coords,
			(3 * 3 * _faceCount + 2 * 3 * _faceCount * _textures.size()) * sizeof(float));

	memcpy(node._smoothGroups, _smoothGroups, _faceCount * sizeof(uint32));
	memcpy(node._material    , _material    , _faceCount * sizeof(uint32));

	node.createBound();
}

void ModelNode::setInvisible(bool invisible) {
	_render = !invisible;
}

void ModelNode::loadTextures(const std::vector<Common::UString> &textures) {
	bool hasTexture = false;

	_textures.resize(textures.size());

	bool hasAlpha = true;
	bool isDecal  = true;

	for (uint t = 0; t != textures.size(); t++) {

		try {

			if (!textures[t].empty() && (textures[t] != "NULL")) {
				_textures[t] = TextureMan.get(textures[t]);
				hasTexture = true;

				if (!_textures[t].getTexture().hasAlpha())
					hasAlpha = false;
				if (_textures[t].getTexture().getTXI().getFeatures().alphaMean == 1.0)
					hasAlpha = false;

				if (!_textures[t].getTexture().getTXI().getFeatures().decal)
					isDecal = false;
			}

		} catch (...) {
			warning("Failed loading texture \"%s\"", textures[t].c_str());
		}

	}

	if (_hasTransparencyHint) {
		_isTransparent = _transparencyHint;
		if (isDecal)
			_isTransparent = true;
	} else {
		_isTransparent = hasAlpha;
	}

	// If the node has no actual texture, we just assume
	// that the geometry shouldn't be rendered.
	if (!hasTexture)
		_render = false;
}

bool ModelNode::createFaces(uint32 count) {
	assert(!_coords);

	if (count == 0)
		return false;

	const uint32 textureCount = _textures.size();

	_faceCount = count;

	_coords = new float[3 * 3 * _faceCount + 2 * 3 * _faceCount * textureCount];

	_vX = _coords + 0 * 3 * _faceCount;
	_vY = _coords + 1 * 3 * _faceCount;
	_vZ = _coords + 2 * 3 * _faceCount;

	_tX = _coords + 3 * 3 * _faceCount + 0 * 3 * _faceCount * textureCount;
	_tY = _coords + 3 * 3 * _faceCount + 1 * 3 * _faceCount * textureCount;

	_smoothGroups = new uint32[_faceCount];
	_material     = new uint32[_faceCount];

	return true;
}

void ModelNode::createBound() {
	uint32 count = 3 * _faceCount;
	for (uint32 v = 0; v < count; v++)
		_boundBox.add(_vX[v], _vY[v], _vZ[v]);

	createCenter();
}

void ModelNode::createCenter() {

	float minX, minY, minZ, maxX, maxY, maxZ;
	_boundBox.getMin(minX, minY, minZ);
	_boundBox.getMax(maxX, maxY, maxZ);

	_center[0] = minX + ((maxX - minX) / 2.0);
	_center[1] = minY + ((maxY - minY) / 2.0);
	_center[2] = minZ + ((maxZ - minZ) / 2.0);
}

const Common::BoundingBox &ModelNode::getAbsoluteBound() const {
	return _absoluteBoundBox;
}

void ModelNode::createAbsoluteBound(Common::BoundingBox parentPosition) {
	// Transform by our position/orientation/rotation
	parentPosition.translate(_position[0], _position[1], _position[2]);
	parentPosition.rotate(_orientation[3], _orientation[0], _orientation[1], _orientation[2]);

	parentPosition.rotate(_rotation[0], 1.0, 0.0, 0.0);
	parentPosition.rotate(_rotation[1], 0.0, 1.0, 0.0);
	parentPosition.rotate(_rotation[2], 0.0, 0.0, 1.0);


	// That's our absolute position
	_absolutePosition = parentPosition.getOrigin();


	// Add our bounding box, creating the absolute bounding box
	_absoluteBoundBox = parentPosition;
	_absoluteBoundBox.add(_boundBox);
	_absoluteBoundBox.absolutize();


	// Recurse into the children
	for (std::list<ModelNode *>::iterator c = _children.begin(); c != _children.end(); ++c) {
		(*c)->createAbsoluteBound(parentPosition);

		_absoluteBoundBox.add((*c)->getAbsoluteBound());
	}
}

void ModelNode::orderChildren() {
	_children.sort(nodeComp);

	// Order the children's children
	for (std::list<ModelNode *>::iterator c = _children.begin(); c != _children.end(); ++c)
		(*c)->orderChildren();
}

void ModelNode::checkRebuild() {
	if (!_needRebuild)
		return;

	_needRebuild = false;
	rebuild();
}

void ModelNode::render(RenderPass pass) {
	checkRebuild();

	// Apply the node's transformation
	glTranslatef(_position[0], _position[1], _position[2]);
	glRotatef(_orientation[3], _orientation[0], _orientation[1], _orientation[2]);

	glRotatef(_rotation[0], 1.0, 0.0, 0.0);
	glRotatef(_rotation[1], 0.0, 1.0, 0.0);
	glRotatef(_rotation[2], 0.0, 0.0, 1.0);


	// Test if we have something to render in the current pass
	bool shouldRender = _render;
	if (((pass == kRenderPassOpaque)      &&  _isTransparent) ||
			((pass == kRenderPassTransparent) && !_isTransparent))
		shouldRender = false;


	// Render the node's geometry
	if (shouldRender && (_faceCount > 0) && (_list != 0)) {

		// Enable all needed texture units
		for (uint32 t = 0; t < _textures.size(); t++) {
			TextureMan.activeTexture(t);
			glEnable(GL_TEXTURE_2D);

			TextureMan.set(_textures[t]);
		}

		// Render
		glCallList(_list);

		// Disable the texture units again
		for (uint32 i = 0; i < _textures.size(); i++) {
			TextureMan.activeTexture(i);
			glDisable(GL_TEXTURE_2D);
		}

	}


	// Render the node's children
	for (std::list<ModelNode *>::iterator c = _children.begin(); c != _children.end(); ++c) {
		glPushMatrix();
		(*c)->render(pass);
		glPopMatrix();
	}
}

void ModelNode::doRebuild() {
	if (_faceCount == 0)
		return;

	_list = glGenLists(1);

	glNewList(_list, GL_COMPILE);
	glBegin(GL_TRIANGLES);

	const uint32 textureCount = _textures.size();

	const float *vX = _vX;
	const float *vY = _vY;
	const float *vZ = _vZ;
	const float *tX = _tX;
	const float *tY = _tY;

	for (uint32 f = 0; f < _faceCount; f++, vX += 3, vY += 3, vZ += 3,
	                                   tX += 3 * textureCount, tY += 3 * textureCount) {

		// Texture vertex A
		for (uint32 t = 0; t < textureCount; t++)
			TextureMan.textureCoord2f(t, tX[3 * t + 0], tY[3 * t + 0]);

		// Geometry vertex A
		glVertex3f(vX[0], vY[0], vZ[0]);


		// Texture vertex B
		for (uint32 t = 0; t < textureCount; t++)
			TextureMan.textureCoord2f(t, tX[3 * t + 1], tY[3 * t + 1]);

		// Geometry vertex B
		glVertex3f(vX[1], vY[1], vZ[1]);


		// Texture vertex C
		for (uint32 t = 0; t < textureCount; t++)
			TextureMan.textureCoord2f(t, tX[3 * t + 2], tY[3 * t + 2]);

		// Geometry vertex C
		glVertex3f(vX[2], vY[2], vZ[2]);
	}

	glEnd();
	glEndList();
}

void ModelNode::doDestroy() {
	if (!_list == 0)
		return;

	glDeleteLists(_list, 1);
	_list = 0;
}

} // End of namespace Aurora

} // End of namespace Graphics
