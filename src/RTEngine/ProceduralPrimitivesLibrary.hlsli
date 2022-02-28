//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//**********************************************************************************************
//
// ProceduralPrimitivesLibrary.hlsli
//
// An interface to call per geometry intersection tests based on as primitive type.
//
//**********************************************************************************************

#ifndef PROCEDURALPRIMITIVESLIBRARY_H
#define PROCEDURALPRIMITIVESLIBRARY_H

#include "RaytracingShaderHelper.hlsli"

#include "AnalyticPrimitives.hlsli"
#include "VolumetricPrimitives.hlsli"

// Analytic geometry intersection test.
// AABB local space dimensions: <-1,1>.

// Use this one for cubes
/*bool RayAnalyticGeometryIntersectionTest(in Ray ray, in AnalyticPrimitive::Enum analyticPrimitive, out float thit, out ProceduralPrimitiveAttributes attr)
{
    float3 aabb[2] = {
        float3(-1,-1,-1),
        float3(1,1,1)
    };
    float tmax;

    switch (analyticPrimitive)
    {
      case AnalyticPrimitive::AABB0: return RayAABBIntersectionTest(ray, aabb, thit, attr);
      case AnalyticPrimitive::AABB1: return RayAABBIntersectionTest(ray, aabb, thit, attr);
      case AnalyticPrimitive::AABB2: return RayAABBIntersectionTest(ray, aabb, thit, attr);
     // case AnalyticPrimitive::MetalSpheres: return RaySphereGeometryIntersectionTest(ray, thit, attr);
    //  case AnalyticPrimitive::GlassSpheres: return RaySphereGeometryIntersectionTest(ray, thit, attr);
     // case AnalyticPrimitive::DiffuseSpheres: return RaySphereGeometryIntersectionTest(ray, thit, attr);
    default: return false;
    }
}
*/
bool RaySphereGeometryIntersectionTest(in Ray ray, out float thit, out ProceduralPrimitiveAttributes attr, in float radius)
{

    return RaySphereTest(ray, thit, attr, radius);

}

// Analytic geometry intersection test.
// AABB local space dimensions: <-1,1>.
bool RayVolumetricGeometryIntersectionTest(in Ray ray, in VolumetricPrimitive::Enum volumetricPrimitive, out float thit, out ProceduralPrimitiveAttributes attr, in float elapsedTime)
{
    switch (volumetricPrimitive)
    {
    case VolumetricPrimitive::Metaballs: return RayMetaballsIntersectionTest(ray, thit, attr, elapsedTime);
    default: return false;
    }
}

#endif // PROCEDURALPRIMITIVESLIBRARY_H