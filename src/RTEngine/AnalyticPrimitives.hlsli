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
// AnalyticPrimitives.hlsli
//
// Set of ray vs analytic primitive intersection tests.
// Metal
//**********************************************************************************************

#ifndef ANALYTICPRIMITIVES_H
#define ANALYTICPRIMITIVES_H


#include "RaytracingShaderHelper.hlsli"

// Solve a quadratic equation.
// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool SolveQuadraticEqn(float a, float b, float c, out float x0, out float x1)
{
    float discr = b * b - 4 * a * c;
    if (discr < 0) return false;
    else if (discr == 0) x0 = x1 = -0.5 * b / a;
    else {
        float q = (b > 0) ?
            -0.5 * (b + sqrt(discr)) :
            -0.5 * (b - sqrt(discr));
        x0 = q / a;
        x1 = c / q;
    }
    if (x0 > x1) swap(x0, x1);

    return true;
}

// Calculate a normal for a hit point on a sphere.
float3 CalculateNormalForARaySphereHit(in Ray ray, in float thit, float3 center)
{
    float3 hitPosition = ray.origin + thit * ray.direction;
    return normalize(hitPosition - center);
}

// Analytic solution of an unbounded ray sphere intersection points.
// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool SolveRaySphereIntersectionEquation(in Ray ray, out float tmin, out float tmax, in float3 center, in float radius)
{
    float3 L = ray.origin - center;
    float a = dot(ray.direction, ray.direction);
    float b = 2 * dot(ray.direction, L);
    float c = dot(L, L) - radius * radius;
    return SolveQuadraticEqn(a, b, c, tmin, tmax);
}

// Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects a hollow sphere.
// This algo is similar to ray tracing in a weekend ray spehere intersection
bool RaySphereIntersectionTest(in Ray ray, out float thit, out float tmax, out ProceduralPrimitiveAttributes attr, in float3 center = float3(0, 0, 0), in float radius = 1)
{
    float t0, t1; // solutions for t if the ray intersects 

    if (!SolveRaySphereIntersectionEquation(ray, t0, t1, center, radius)) return false;
    tmax = t1;

    if (t0 < RayTMin())
    {
        // t0 is before RayTMin, let's use t1 instead .
        if (t1 < RayTMin()) return false; // both t0 and t1 are before RayTMin

        attr.normal = CalculateNormalForARaySphereHit(ray, t1, center);
        if (IsAValidHit(ray, t1, attr.normal))
        {
            thit = t1;
            return true;
        }
    }
    else
    {
        attr.normal = CalculateNormalForARaySphereHit(ray, t0, center);
        if (IsAValidHit(ray, t0, attr.normal))
        {
            thit = t0;
            return true;
        }

        attr.normal = CalculateNormalForARaySphereHit(ray, t1, center);
        if (IsAValidHit(ray, t1, attr.normal))
        {
            thit = t1;
            return true;
        }
    }
    return false;
}

// Test if a ray segment <RayTMin(), RayTCurrent()> intersects a solid sphere.
// Limitation: this test does not take RayFlags into consideration and does not calculate a surface normal.
bool RaySolidSphereIntersectionTest(in Ray ray, out float thit, out float tmax, in float3 center = float3(0, 0, 0), in float radius = 1)
{
    float t0, t1; // solutions for t if the ray intersects 

    if (!SolveRaySphereIntersectionEquation(ray, t0, t1, center, radius)) 
        return false;

    // Since it's a solid sphere, clip intersection points to ray extents.
    thit = max(t0, RayTMin());
    tmax = min(t1, RayTCurrent());

    return true;
}

bool RaySphereTest(in Ray ray, out float thit, out ProceduralPrimitiveAttributes attr, in float radius)
{
    float3 center = float3(0, 0, 0);
    float  radii =  radius;
    bool hitFound = false;

    thit = RayTCurrent();

    float _thit;
    float _tmax;
    ProceduralPrimitiveAttributes _attr;
    if (RaySphereIntersectionTest(ray, _thit, _tmax, _attr, center, radii))
    {
        if (_thit < thit)
        {
            thit = _thit;
            attr = _attr;
            hitFound = true;
            return hitFound;
        }
    }
    
    return hitFound;
}

// Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects multiple hollow spheres.
bool RaySpheresIntersectionTest(in Ray ray, out float thit, out ProceduralPrimitiveAttributes attr)
{
    const int N = 2;
    float3 centers[N] =
    {
        float3(0, -1.25, .5),
        float3(-2,-2.0, 0.0),
    };
    float  radii[N] = { 1, .25};//{ 1, 1, 0.15 };
    bool hitFound = false;

    //
    // Test for intersection against all spheres and take the closest hit.
    //
    thit = RayTCurrent();

    // test against all spheres
    for (int i = 0; i < N; i++)
    {
        float _thit;
        float _tmax;
        ProceduralPrimitiveAttributes _attr;
        if (RaySphereIntersectionTest(ray, _thit, _tmax, _attr, centers[i], radii[i]))
        {
            if (_thit < thit)
            {
                thit = _thit;
                attr = _attr;
                hitFound = true;
                return hitFound;
            }
        }
    }
    return hitFound;
}

#endif // ANALYTICPRIMITIVES_H