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

#ifndef RAYTRACINGSHADERHELPER_H
#define RAYTRACINGSHADERHELPER_H

#include "RayTracingHlslCompat.h"

#define INFINITY (1.0/0.0)

struct Ray
{
    float3 origin;
    float3 direction;
};

float length_toPow2(float2 p)
{
    return dot(p, p);
}

float length_toPow2(float3 p)
{
    return dot(p, p);
}

float rnd(float2 x)
{
    int n = int(x.x * 40.0 + x.y * 6400.0);
    n = (n << 13) ^ n;
    return 1.0 - float( (n * (n * n * 15731 + 789221) + \
             1376312589) & 0x7fffffff) / 1073741824.0;
}

float rnd2(float2 uv)
{
    return frac(sin(dot(uv.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float3 randomFloat3(float seed) 
{
  float2 seed0 = float2(0,seed);
  float2 seed1 = float2(0,seed+1);
  float2 seed2 = float2(0,seed+2);
  return float3(rnd2(seed0), rnd2(seed1), rnd2(seed2));
}

float trilinearInterp(double c[2][2][2], double u, double v, double w) {
    float accum = 0.0;
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++)
            {
                accum += (i * u + (1 - i) * (1 - u)) *
                    (j * v + (1 - j) * (1 - v)) *
                    (k * w + (1 - k) * (1 - w)) * c[i][j][k];
            }
        }
    }

    return accum;
}

float noise(float3 p, float scale)  
{
   p *= scale;
   float u = p.x - floor(p.x);
   float v = p.y - floor(p.y);
   float w = p.z - floor(p.z);

   //Hermitian smoothing
   u = u * u * (3 - 2 * u);
   v = v * v * (3 - 2 * v);
   w = w * w * (3 - 2 * w);

   int i = floor(p.x);
   int j = floor(p.y);
   int k = floor(p.z);

   double c[2][2][2];

   for (int di = 0; di < 2; di++) {
       for (int dj = 0; dj < 2; dj++) {
           for (int dk = 0; dk < 2; dk++) {
               float perm_x = rnd2(float2(0, i+di));
               float perm_y = rnd2(float2(0, j+dj));
               float perm_z = rnd2(float2(0, k+dk));
               float3 rand = randomFloat3(perm_x * perm_y * perm_z);
               c[di][dj][dk] = rand.x;
           }
       }
   }
   return trilinearInterp(c,u,v,w);
}

float lengthSquared(float3 myVec){
    return myVec.x*myVec.x + myVec.y*myVec.y + myVec.z*myVec.z;                             
}

float3 randomInUnitSphere(float seed)
{
  while(true)
  {
    float3 ranFloat3 = randomFloat3(seed);
    if(lengthSquared(ranFloat3) >= 1.0f){
        return ranFloat3;
    }
    seed = seed +1;
  }
}

float2 get_sphere_uv(float3 p) 
{
    float pi = 3.1415;
    float theta = acos(-p.y);
    float phi = atan2(-p.z, p.x) + pi;

    float u = phi / (2*pi);
    float v = theta / pi;
    
    return float2(u,v);
}

// Returns a cycling <0 -> 1 -> 0> animation interpolant 
float CalculateAnimationInterpolant(in float elapsedTime, in float cycleDuration)
{
    float curLinearCycleTime = fmod(elapsedTime, cycleDuration) / cycleDuration;
    curLinearCycleTime = (curLinearCycleTime <= 0.5f) ? 2 * curLinearCycleTime : 1 - 2 * (curLinearCycleTime - 0.5f);
    return smoothstep(0, 1, curLinearCycleTime);
}

void swap(inout float a, inout float b)
{
    float temp = a;
    a = b;
    b = temp;
}

bool IsInRange(in float val, in float min, in float max)
{
    return (val >= min && val <= max);
}

// Load three 16 bit indices.
static
uint3 Load3x16BitIndices(uint offsetBytes, ByteAddressBuffer Indices)
{
    uint3 indices;

    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~3;
    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);

    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], float2 barycentrics)
{
    return vertexAttribute[0] +
        barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline Ray GenerateCameraRay(uint2 index, in float3 cameraPosition, in float4x4 projectionToWorld)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a world positon.
    float4 world = mul(float4(screenPos, 0, 1), projectionToWorld);
    world.xyz /= world.w;

    Ray ray;
    ray.origin = cameraPosition;
    ray.direction = normalize(world.xyz - ray.origin);

    return ray;
}

// Test if a hit is culled based on specified RayFlags.
bool IsCulled(in Ray ray, in float3 hitSurfaceNormal)
{
    float rayDirectionNormalDot = dot(ray.direction, hitSurfaceNormal);

    bool isCulled = 
        ((RayFlags() & RAY_FLAG_CULL_BACK_FACING_TRIANGLES) && (rayDirectionNormalDot > 0))
        ||
        ((RayFlags() & RAY_FLAG_CULL_FRONT_FACING_TRIANGLES) && (rayDirectionNormalDot < 0));

    return isCulled; 
}

// Test if a hit is valid based on specified RayFlags and <RayTMin, RayTCurrent> range.
bool IsAValidHit(in Ray ray, in float thit, in float3 hitSurfaceNormal)
{
    return IsInRange(thit, RayTMin(), RayTCurrent()) && !IsCulled(ray, hitSurfaceNormal);
}

// Texture coordinates on a horizontal plane.
float2 TexCoords(in float3 position)
{
    return position.xz;
}

//procedurally generated checker texture
float4 getCheckerColor(float u, float v, float3 p)
{
    float4 color1 = float4(0,0,0,1);
    float4 color2 = float4(1,1,1,1);

    float sines = sin(10*p.x)*sin(10*p.y)*sin(10*p.z);
    if (sines < 0)
    {
      return color1;
    } 
   else
   { 
      return color2;  
   }               
}

// Fresnel reflectance - schlick approximation.
float3 FresnelReflectanceSchlick(in float3 I, in float3 N, in float3 f0)
{
    float cosi = saturate(dot(-I, N));
    return f0 + (1 - f0)*pow(1 - cosi, 5);
}

// Schlick's approximation for reflectance
float reflectance(in float cosine, in float reflectIndex)
{
    // Use Schlick's approximation for reflectance.
    float r0 = (1 - reflectIndex) / (1 + reflectIndex);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}
 
float3 reflectSH(in float3 incidentVec, in float3 normal)
{
    return incidentVec - 2*normal*dot(incidentVec, normal);                                          
}
// eta is refraction index
float3 refractSH(in float3 incidentVec, in float3 normal, float ior)
{
//Refraction eq from RTIAW
  /*  float cos_theta = min(dot(-incidentVec, normal), 1.0);
    float3 r_out_perp =  ior * (incidentVec + cos_theta*normal);
    float3 r_out_parallel = -sqrt(abs(1.0 - length_toPow2(r_out_perp))) * normal;
    return r_out_perp + r_out_parallel;*/

// refraction eq from scratchapixel
    float cosi = clamp(-1, 1, dot(incidentVec, normal)); 
    float etai = 1;
    float etat = ior; 
    float3 n = normal; 
    if (cosi < 0) { cosi = -cosi; } else { swap(etai, etat); n= -normal; } 
    float eta = etai / etat; 
    float k = 1 - eta * eta * (1 - cosi * cosi); 
    if(k < 0){ return 0;}
    return eta * incidentVec + (eta * cosi - sqrt(k)) * n; 
}
                    
#endif // RAYTRACINGSHADERHELPER_H