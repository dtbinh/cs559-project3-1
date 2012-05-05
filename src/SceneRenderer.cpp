#include "StdAfx.hpp"
#include "SceneRenderer.hpp"

#include <deque>

#include "SceneNode.hpp"
#include "Material.hpp"
#include "Camera.hpp"
#include "Texture.hpp"
#include "Vector3.hpp"
#include "CgSingleton.hpp"
#include "CgProgram.hpp"
#include "DirectionalLight.hpp"

using namespace std;

SceneRenderer::SceneRenderer(size_t screenWidth, size_t screenHeight)
	: mrtFB(screenWidth, screenHeight), compFB(screenWidth, screenHeight)
{
	unlit = make_shared<Texture>(nullptr, 4, screenWidth, screenHeight,
	                             GL_RGBA, GL_UNSIGNED_BYTE, false);

	normAndDepth = make_shared<Texture>(nullptr, 4, screenWidth, screenHeight,
	                                    GL_RGBA, GL_UNSIGNED_BYTE, false);

	lit = make_shared<Texture>(nullptr, 4, screenWidth, screenHeight,
	                           GL_RGBA, GL_UNSIGNED_BYTE, false);

	mrtFB.attachTexture(unlit, 0);
	mrtFB.attachTexture(normAndDepth, 1);
	mrtFB.attachTexture(lit, 2);
	mrtFB.setNumRenderTargets(3);

	comp0 = make_shared<Texture>(nullptr, 3, screenWidth, screenHeight,
	                             GL_RGBA, GL_UNSIGNED_BYTE, false);

	comp1 = make_shared<Texture>(nullptr, 3, screenWidth, screenHeight,
	                             GL_RGBA, GL_UNSIGNED_BYTE, false);

	auto& cgContext = CgSingleton::getSingleton().getContext();
	auto& fragProfile = CgSingleton::getSingleton().getFragmentProfile();
	stripAlphaShader = make_shared<CgProgram>(cgContext, false,
	                   "./resources/shaders/StripAlpha.cg",
	                   fragProfile, "main");
	alphaOnlyShader = make_shared<CgProgram>(cgContext, false,
	                  "./resources/shaders/AlphaOnly.cg",
	                  fragProfile, "main");
	directionalLightShader = make_shared<CgProgram>(cgContext, false,
			"./resources/shaders/DirectionalLight.cg",
			fragProfile, "main");

	singleTexMat = make_shared<Material>();
	singleTexMat->textures.push_back(normAndDepth);
	singleTexMat->fragmentShader = stripAlphaShader;
	singleTexMat->depthTest = false;

	lightingMat = make_shared<Material>();
	lightingMat->textures.resize(3);
	lightingMat->textures[1] = normAndDepth;
	lightingMat->textures[2] = lit;
	lightingMat->depthTest = false;

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

//! Utility function to save from some verbosity below
static inline void drawQuad()
{
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(-1.0f, 1.0f, 0.99f);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-1.0f, -1.0f, 0.99f);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(1.0f, -1.0f, 0.99f);
	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(1.0f, 1.0f, 0.99f);
	glEnd();
}

void SceneRenderer::renderScene()
{
	// There's no point in drawing a scene if we don't have an active camera
	if (!activeCamera)
		return;

	// List of renderables to render
	list<Renderable*> lights;
	list<Renderable*> bg;
	list<Renderable*> normals;

	// Update the transforms of all scene nodes via bfs and add their
	// renderables to the various queues
	deque<SceneNode*> q;
	for (auto it = sceneNodes.begin(); it != sceneNodes.end(); ++it)
		q.push_back(it->get());

	while (!q.empty()) {
		auto curr = q.front();
		q.pop_front();

		curr->updateAbsoluteTransform();
		const auto& renderables = curr->getRenderables();
		for (auto it = renderables.begin(); it != renderables.end(); ++it) {
			switch ((*it)->getRenderableType()) {
			case Renderable::RT_LIGHT:
				lights.push_back(it->get());
				break;

			case Renderable::RT_BACKGROUND:
				bg.push_back(it->get());
				break;

			case Renderable::RT_NORMAL:
				normals.push_back(it->get());
				break;
			}
		}

		// Enqueue all the node's children
		const auto& currChildren = curr->getChildren();
		for (auto it = currChildren.begin(); it != currChildren.end(); ++it)
			q.push_back(it->get());
	}

	// Render to the screen texture
	mrtFB.setupRender();

	// Draw our camera first
	activeCamera->render();

	// Draw all background objects
	for (auto it = bg.begin(); it != bg.end(); ++it) {
		if ((*it)->isVisible()) {
			glPushMatrix();
			glMultMatrixf((*it)->getOwner().lock()->
			              getAbsoluteTransform().getArray());
			(*it)->render();
			glPopMatrix();
		}
	}

	// Clear the depth buffer, allowing normal objects to always be drawn
	// over the background ones
	glClear(GL_DEPTH_BUFFER_BIT);

	// Draw all normal objects
	for (auto it = normals.begin(); it != normals.end(); ++it) {
		if ((*it)->isVisible()) {
			glPushMatrix();
			glMultMatrixf((*it)->getOwner().lock()->
			              getAbsoluteTransform().getArray());
			(*it)->render();
			glPopMatrix();
		}
	}

	mrtFB.cleanupRender();

	// Draw our rendered texture on a quad
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Transform modelViewIT;
	glGetFloatv(GL_MODELVIEW_MATRIX, modelViewIT.getArray());
	modelViewIT.setToInverse();
	modelViewIT.setToTranspose();

	// Set matrices to identities to simplify drawing screen-wide quads
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	switch (dm) {
	case DM_UNLIT:
		singleTexMat->textures[0] = unlit;
		singleTexMat->fragmentShader = stripAlphaShader;
		setActiveMaterial(singleTexMat);
		drawQuad();
		break;

	case DM_NORMALS:
		singleTexMat->textures[0] = normAndDepth;
		singleTexMat->fragmentShader = stripAlphaShader;
		setActiveMaterial(singleTexMat);
		drawQuad();
		break;

	case DM_DEPTH:
		singleTexMat->textures[0] = normAndDepth;
		singleTexMat->fragmentShader = alphaOnlyShader;
		setActiveMaterial(singleTexMat);
		drawQuad();
		break;

	case DM_LIT:
		singleTexMat->textures[0] = lit;
		singleTexMat->fragmentShader = stripAlphaShader;
		setActiveMaterial(singleTexMat);
		drawQuad();
		break;

	case DM_FINAL:
		// Step one of the deferred process: copy the unlit colors to comp0
		singleTexMat->textures[0] = unlit;
		singleTexMat->fragmentShader = stripAlphaShader;
		setActiveMaterial(singleTexMat);
		compFB.attachTexture(comp0);
		compFB.setupRender();
		drawQuad();
		compFB.cleanupRender();

		// Sort the lights
		list<DirectionalLight*> dirLights;
		//! \todo Put lists of other light types here

		for (auto it = lights.begin(); it != lights.end(); ++it) {
			switch (static_cast<Light*>(*it)->getLightType()) {
			case Light::LT_DIRECTIONAL:
				dirLights.push_back(static_cast<DirectionalLight*>(*it));
				break;
			}
		}
		// Done sorting lights

		auto src = comp0;
		auto dest = comp1;
		// Apply directional lights
		lightingMat->fragmentShader = directionalLightShader;
		for (auto it = dirLights.begin(); it != dirLights.end(); ++it) {
			// Set up the lighting material
			lightingMat->textures[0] = src;
			setActiveMaterial(lightingMat);
			// Set shader parameters
			directionalLightShader->getNamedParameter("color").
				set3fv((*it)->color);
			// Convert the light direction to view space
			Vector3 lightDirection = (*it)->direction; 
			lightDirection.normalize(); //!< \todo: unneeded?
			modelViewIT.transformPoint(lightDirection);
			directionalLightShader->getNamedParameter("lightDirection").
				setVector3(lightDirection);
			// Render to dest
			compFB.attachTexture(dest);
			compFB.setupRender();
			drawQuad();
			compFB.cleanupRender();
			// Flip src and dest
			swap(src, dest);
		}

		singleTexMat->textures[0] = src;
		singleTexMat->fragmentShader = stripAlphaShader;
		setActiveMaterial(singleTexMat);
		drawQuad();
		break;
	}
}
