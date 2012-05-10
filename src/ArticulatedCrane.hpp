#pragma once

#include "Model.hpp"
#include "SceneRenderer.hpp"
#include "SceneNode.hpp"
#include "Vector3.hpp"
#include "Renderable.hpp"
#include "Animator.hpp"
#include "OBJFile.hpp"

using namespace std;


//! An articulated object that allows rotating the arm about the base,
//! moving the load in and out along the arm, and raising and lowering
//! the load.
class ArticulatedCrane: public Animator
{
private:
	Vector3 basevector;
	Vector3 armvector;
	Vector3 loadvector;
	float loaddistance;
	float loadheight;
	float dtheta;
	float dx;
	float dy;
	float outmax;
	float downmax;
	bool loaddirout;
	bool loaddirdown;
	shared_ptr<Model> basemodel;
	shared_ptr<Model> armmodel;
	shared_ptr<Model> loadmodel;
	shared_ptr<SceneNode> basesn;
	shared_ptr<SceneNode> armsn;
	shared_ptr<SceneNode> loadsn;

public:
	ArticulatedCrane(SceneRenderer* sr, Vector3 location);
	void animate(double dt);
	void rotateArm();
	void extendLoad();
	void retractLoad();
	void raiseLoad();
	void lowerLoad();
};
