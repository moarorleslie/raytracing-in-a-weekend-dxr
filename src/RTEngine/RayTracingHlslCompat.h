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

#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

//**********************************************************************************************
//
// RaytracingHLSLCompat.h
//
// A header with shared definitions for C++ and HLSL source files. 
//
//**********************************************************************************************

#ifdef HLSL
#include "util\HlslCompat.h"
#else
using namespace DirectX;

// Shader will use byte encoding to access vertex indices.
typedef UINT16 Index;
#endif

// Number of metaballs to use within an AABB.
#define N_METABALLS 3    // = {3, 5}

// Limitting calculations only to metaballs a ray intersects can speed up raytracing
// dramatically particularly when there is a higher number of metaballs used. 
// Use of dynamic loops can have detrimental effects to performance for low iteration counts
// and outweighing any potential gains from avoiding redundant calculations.
// Requires: USE_DYNAMIC_LOOPS set to 1 to take effect.
#if N_METABALLS >= 5
#define USE_DYNAMIC_LOOPS 1
#define LIMIT_TO_ACTIVE_METABALLS 1
#else 
#define USE_DYNAMIC_LOOPS 0
#define LIMIT_TO_ACTIVE_METABALLS 0
#endif

#define N_FRACTAL_ITERATIONS 4      // = <1,...>

// PERFORMANCE TIP: Set max recursion depth as low as needed
// as drivers may apply optimization strategies for low recursion depths.
#define MAX_RAY_RECURSION_DEPTH 6    // ~ primary rays + reflections + shadow rays from reflected geometry.


struct ProceduralPrimitiveAttributes
{
    XMFLOAT3 normal;
};

struct RayPayload
{
    XMFLOAT4 color;
    UINT   recursionDepth;
};

struct ShadowRayPayload
{
    bool hit;
};

struct SceneConstantBuffer
{
    XMMATRIX projectionToWorld;
    XMVECTOR cameraPosition;
    XMVECTOR lightPosition;
    XMVECTOR lightAmbientColor;
    XMVECTOR lightDiffuseColor;
    float    reflectance;
    float    elapsedTime;                 // Elapsed application time.
};

// Attributes per primitive type.
struct MaterialConstantBuffer
{
    XMFLOAT4 albedo;
    float reflectanceCoef;
    float diffuseCoef;
    float specularCoef;
    float specularPower;
    float refractionIndex;
    float radius; 
    float fuzz;
    int hasTexture;
    int hasPerlin;
    XMFLOAT3 padding;
};

// Attributes per primitive instance.
struct PrimitiveInstanceConstantBuffer
{
    UINT instanceIndex;  
    UINT primitiveType; // Procedural primitive type
};

// Dynamic attributes per primitive instance.
struct PrimitiveInstancePerFrameBuffer
{
    XMMATRIX localSpaceToBottomLevelAS;   // Matrix from local primitive space to bottom-level object space.
    XMMATRIX bottomLevelASToLocalSpace;   // Matrix from bottom-level object space to local primitive space.
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
};


// Ray types traced in this sample.
namespace RayType {
    enum Enum {
        Radiance = 0,   // ~ Primary, reflected camera/view rays calculating color for each hit.
        Shadow,         // ~ Shadow/visibility rays, only testing for occlusion
        Count
    };
}

namespace TraceRayParameters
{
    static const UINT InstanceMask = ~0;   // Everything is visible.
    namespace HitGroup {
        static const UINT Offset[RayType::Count] =
        {
            0, // Radiance ray
            1  // Shadow ray
        };
        static const UINT GeometryStride = RayType::Count;
    }
    namespace MissShader {
        static const UINT Offset[RayType::Count] =
        {
            0, // Radiance ray
            1  // Shadow ray
        };
    }
}

// From: http://blog.selfshadow.com/publications/s2015-shading-course/hoffman/s2015_pbs_physics_math_slides.pdf
static const XMFLOAT4 ChromiumReflectance = XMFLOAT4(0.549f, 0.556f, 0.554f, 1.0f);

static const XMFLOAT4 BackgroundColor = XMFLOAT4(0.5f, 0.7f, 1.0f, 1.0f);
static const float InShadowRadiance = 0.35f;

// Analytic: can be resolved using a closed-form expression, ie algebraic 
// TODO figure out how to convert enum to ids

namespace AnalyticPrimitive {
    enum Enum {
    /*    AABB0 = 0,
        AABB1,
        AABB2,
        MetalSpheres,
        GlassSpheres,
        DiffuseSpheres,*/
        SP0,
        SP1,
        SP2,
        SP3,
        SP4,
        SP5,
        SP6,
        SP7,
        SP8,
        SP9,
        SP10,
        SP11,
        SP12,
        SP13,
        SP14,
        SP15,
        SP16,
        SP17,
        SP18,
        SP19,
        SP20,
        SP21,
        SP22,
        SP23,
        SP24,
        SP25,
        SP26,
        SP27,
        SP28,
        SP29,
        SP30,
        SP31,
        SP32,
        SP33,
        SP34,
        SP35,
        SP36,
        SP37,
        SP38,
        SP39,
        SP40,
        SP41,
        SP42,
        SP43,
        SP44,
        SP45,
        SP46,
        SP47,
        SP48,
        SP49,
        SP50,
        SP51,
        SP52,
        SP53,
        SP54,
        SP55,
        SP56,
        SP57,
        SP58,
        SP59,
        SP60,
        SP61,
        SP62,
        SP63,
        SP64,
        SP65,
        SP66,
        SP67,
        SP68,
        SP69,
        SP70,
        SP71,
        SP72,
        SP73,
        SP74,
        SP75,
        SP76,
        SP77,
        SP78,
        SP79,
        SP80,
        SP81,
        SP82,
        SP83,
        SP84,
        SP85,
        SP86,
        SP87,
        SP88,
        SP89,
        SP90,
        SP91,
        SP92,
        SP93,
        SP94,
        SP95,
        SP96,
        SP97,
        SP98,
        SP99,
        SP100,
        SP101,
        SP102,
        SP103,
        SP104,
        SP105,
        SP106,
        SP107,
        SP108,
        SP109,
        SP110,
        SP111,
        SP112,
        SP113,
        SP114,
        SP115,
        SP116,
        SP117,
        SP118,
        SP119,
        SP120,
        SP121,
        SP122,
        SP123,
        SP124,
        SP125,
        SP126,
        SP127,
        SP128,
        SP129,
        SP130,
        SP131,
        SP132,
        SP133,
        SP134,
        SP135,
        SP136,
        SP137,
        SP138,
        SP139,
        SP140,
        SP141,
        SP142,
        SP143,
        SP144,
        SP145,
        SP146,
        SP147,
        SP148,
        SP149,   
        SP150,
        SP151,
        SP152,
        SP153,
        SP154,
        SP155,
        SP156,
        SP157,
        SP158,
        SP159,
        SP160,
        SP161,
        SP162,
        SP163,
        SP164,
        SP165,
        SP166,
        SP167,
        SP168,
        SP169,
        SP170,
        SP171,
        SP172,
        SP173,
        SP174,
        SP175,
        SP176,
        SP177,
        SP178,
        SP179,
        SP180,
        SP181,
        SP182,
        SP183,
        SP184,
        SP185,
        SP186,
        SP187,
        SP188,
        SP189,
        SP190,
        SP191,
        SP192,
        SP193,
        SP194,
        SP195,
        SP196,
        SP197,
        SP198,
        SP199,
        SP200,
        SP201,
        SP202,
        SP203,
        SP204,
        SP205,
        SP206,
        SP207,
        SP208,
        SP209,
        SP210,
        SP211,
        SP212,
        SP213,
        SP214,
        SP215,
        SP216,
        SP217,
        SP218,
        SP219,
        SP220,
        SP221,
        SP222,
        SP223,
        SP224,
        SP225,
        SP226,
        SP227,
        SP228,
        SP229,
        SP230,
        SP231,
        SP232,
        SP233,
        SP234,
        SP235,
        SP236,
        SP237,
        SP238,
        SP239,
        SP240,
        SP241,
        SP242,
        SP243,
        SP244,
        SP245,
        SP246,
        SP247,
        SP248,
        SP249,   
        SP250,
        SP251,
        SP252,
        SP253,
        SP254,
        SP255,
        SP256,
        SP257,
        SP258,
        SP259,
        SP260,
        SP261,
        SP262,
        SP263,
        SP264,
        SP265,
        SP266,
        SP267,
        SP268,
        SP269,
        SP270,
        SP271,
        SP272,
        SP273,
        SP274,
        SP275,
        SP276,
        SP277,
        SP278,
        SP279,
        SP280,
        SP281,
        SP282,
        SP283,
        SP284,
        SP285,
        SP286,
        SP287,
        SP288,
        SP289,
        SP290,
        SP291,
        SP292,
        SP293,
        SP294,
        SP295,
        SP296,
        SP297,
        SP298,
        SP299,
        SP300,
        SP301,
        SP302,
        SP303,
        SP304,
        SP305,
        SP306,
        SP307,
        SP308,
        SP309,
        SP310,
        SP311,
        SP312,
        SP313,
        SP314,
        SP315,
        SP316,
        SP317,
        SP318,
        SP319,
        SP320,
        SP321,
        SP322,
        SP323,
        SP324,
        SP325,
        SP326,
        SP327,
        SP328,
        SP329,
        SP330,
        SP331,
        SP332,
        SP333,
        SP334,
        SP335,
        SP336,
        SP337,
        SP338,
        SP339,
        SP340,
        SP341,
        SP342,
        SP343,
        SP344,
        SP345,
        SP346,
        SP347,
        SP348,
        SP349,   
        SP350,
        SP351,
        SP352,
        SP353,
        SP354,
        SP355,
        SP356,
        SP357,
        SP358,
        SP359,
        SP360,
        SP361,
        SP362,
        SP363,
        SP364,
        SP365,
        SP366,
        SP367,
        SP368,
        SP369,
        SP370,
        SP371,
        SP372,
        SP373,
        SP374,
        SP375,
        SP376,
        SP377,
        SP378,
        SP379,
        SP380,
        SP381,
        SP382,
        SP383,
        SP384,
        SP385,
        SP386,
        SP387,
        SP388,
        SP389,
        SP390,
        SP391,
        SP392,
        SP393,
        SP394,
        SP395,
        SP396,
        SP397,
        SP398,
        SP399,
        SP400,
        SP401,
        SP402,
        SP403,
        SP404,
        SP405,
        SP406,
        SP407,
        SP408,
        SP409,
        SP410,
        SP411,
        SP412,
        SP413,
        SP414,
        SP415,
        SP416,
        SP417,
        SP418,
        SP419,
        SP420,
        SP421,
        SP422,
        SP423,
        SP424,
        SP425,
        SP426,
        SP427,
        SP428,
        SP429,
        SP430,
        SP431,
        SP432,
        SP433,
        SP434,
        SP435,
        SP436,
        SP437,
        SP438,
        SP439,
        SP440,
        SP441,
        SP442,
        SP443,
        SP444,
        SP445,
        SP446,
        SP447,
        SP448,
        SP449,   
        SP450,
        SP451,
        SP452,
        SP453,
        SP454,
        SP455,
        SP456,
        SP457,
        SP458,
        SP459,
        SP460,
        SP461,
        SP462,
        SP463,
        SP464,
        SP465,
        SP466,
        SP467,
        SP468,
        SP469,
        SP470,
        SP471,
        SP472,
        SP473,
        FUZZY,
        METAL,
        GLASS,
        BLUEGLOSSY,
        TEXTURED,
        TEXTUREDMETAL,
        PERLIN,
        MOVING,
        Count
    };
}

namespace VolumetricPrimitive {
    enum Enum {
        Metaballs = 0,
        Count
    };
}

#endif // RAYTRACINGHLSLCOMPAT_H