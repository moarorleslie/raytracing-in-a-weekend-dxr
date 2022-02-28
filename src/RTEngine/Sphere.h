#ifndef SPHERE_H
#define SPHERE_H

#include "stdafx.h"
#include "Material.h"
using namespace DirectX;

// Material is material from engine perspective, maps to constant buffer (hlsl perspective)
class Sphere
{
public:
	Sphere(UINT id, Material* mat, XMFLOAT4 alb, XMFLOAT3 cen, float rad);
	~Sphere() = default;

	Material* material = NULL;
	XMFLOAT4 albedo = XMFLOAT4(0,0,0,1);
	XMFLOAT3 center = XMFLOAT3(0,0,0);
	float radius = 1;
	float fuzz = 1.0f;
	UINT ID = 0;

};

#endif // !SPHERE_H