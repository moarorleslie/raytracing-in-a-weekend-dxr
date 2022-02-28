#include "stdafx.h"
#include "Sphere.h"

#include "RaytracingSceneDefines.h"


using namespace DirectX;
Sphere::Sphere(UINT id, Material* mat, XMFLOAT4 alb, XMFLOAT3 cen, float rad)
{
	this->material = mat;
	this->albedo = alb;
	this->center = cen;
	this->radius = rad;
	this->ID = id;
}


