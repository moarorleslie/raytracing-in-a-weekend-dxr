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

#include "stdafx.h"
#include "RTEngine.h"
#include "UtilityFunctions.h"
#include "CompiledShaders\Raytracing.hlsl.h"
#include <iostream>

#include<Windows.h>


using namespace std;
using namespace DX;

// Shader entry points.
const wchar_t* RTEngine::c_raygenShaderName = L"MyRaygenShader";
const wchar_t* RTEngine::c_intersectionShaderNames[] =
{
	L"MyIntersectionShader_AnalyticPrimitiveSphere",
	L"MyIntersectionShader_VolumetricPrimitive",
};
const wchar_t* RTEngine::c_closestHitShaderNames[] =
{
	L"MyClosestHitShader_Triangle",
	L"MyClosestHitShader_AABB",
};
const wchar_t* RTEngine::c_missShaderNames[] =
{
	L"MyMissShader", L"MyMissShader_ShadowRay"
};
// Hit groups.
const wchar_t* RTEngine::c_hitGroupNames_TriangleGeometry[] =
{
	L"MyHitGroup_Triangle", L"MyHitGroup_Triangle_ShadowRay"
};
const wchar_t* RTEngine::c_hitGroupNames_AABBGeometry[][RayType::Count] =
{
	{ L"MyHitGroup_AABB_AnalyticPrimitive", L"MyHitGroup_AABB_AnalyticPrimitive_ShadowRay" },
	{ L"MyHitGroup_AABB_VolumetricPrimitive", L"MyHitGroup_AABB_VolumetricPrimitive_ShadowRay" },
};


RTEngine::RTEngine(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_raytracingOutputResourceUAVDescriptorHeapIndex(UINT_MAX),
	m_animateRotationTime(0.0f),
	m_animateCamera(false),
	m_animateGeometry(true),
	m_animateLight(false),
	m_descriptorsAllocated(0),
	m_descriptorSize(0),
	m_missShaderTableStrideInBytes(UINT_MAX),
	m_hitGroupShaderTableStrideInBytes(UINT_MAX)
{
	UpdateForSizeChange(width, height);
}


void RTEngine::OnInit()
{
	m_deviceResources = std::make_unique<DeviceResources>(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_UNKNOWN,
		FrameCount,
		D3D_FEATURE_LEVEL_11_0,
		// Sample shows handling of use cases with tearing support, which is OS dependent and has been supported since TH2.
		// Since the sample requires build 1809 (RS5) or higher, we don't need to handle non-tearing cases.
		DeviceResources::c_RequireTearingSupport,
		m_adapterIDoverride
		);
	m_deviceResources->RegisterDeviceNotify(this);
	m_deviceResources->SetWindow(Win32Application::GetHwnd(), m_width, m_height);
	m_deviceResources->InitializeDXGIAdapter();

	// If broken here, and existing graphics card is compatible, check window's graphics display setting
	// and ensure that the compiled app is using the correct GPU
	ThrowIfFalse(IsDirectXRaytracingSupported(m_deviceResources->GetAdapter()),
		L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

	m_deviceResources->CreateDeviceResources();
	m_deviceResources->CreateWindowSizeDependentResources();

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initialize scene rendering parameters.
void RTEngine::InitializeScene()
{
	SetupCamera();
	SetupLights();

	BuildPlaneGeometry();

	// Draw scene
	DemoType demo1 = DemoType::RTIAW;
	DemoType demo2 = DemoType::RTTNW;
	DemoType demo3 = DemoType::METABALLS;

	switch (demo2) {
	case DemoType::RTIAW:
		RTIAWRandomScene();
		break;
	case DemoType::RTTNW:
		RTTNWDemo();
		break;
	case DemoType::METABALLS:
		MetaballDemo();
		break;
	default:
		assert(false);
	}

}

// Update camera matrices passed into the shader.
void RTEngine::UpdateCameraMatrices()
{
	auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

	m_sceneCB->cameraPosition = m_eye;
	float fovAngleY = 20.0f;
	XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 0.01f, 125.0f);
	XMMATRIX viewProj = view * proj;
	m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, viewProj);
}

// Update AABB primite attributes buffers passed into the shader.

void RTEngine::UpdateAABBPrimitiveTransform(float animationTime)
{
	auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
	int flip = (int)animationTime;

	XMMATRIX mIdentity = XMMatrixIdentity();

	XMMATRIX mScale15y = XMMatrixScaling(1, 1.5, 1);
	XMMATRIX mScale15 = XMMatrixScaling(1.5, 1.5, 1.5);
	XMMATRIX mScale2 = XMMatrixScaling(2, 2, 2);
	XMMATRIX mScale3 = XMMatrixScaling(3, 3, 3);

	float testValue = 1.0f;
	XMMATRIX testScale = XMMatrixScaling(testValue, testValue, testValue);

	XMMATRIX mRotation = XMMatrixRotationY(-2 * animationTime);

	// Apply scale, rotation and translation transforms.
	// The intersection shader tests in this sample work with local space, so here
	// we apply the BLAS object space translation that was passed to geometry descs.
	auto SetTransformForAABBRotate = [&](UINT primitiveIndex, XMMATRIX& mScale, XMMATRIX& mRotation)
	{
		XMVECTOR vTranslation =
			(0.5f) * (XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_aabbs[primitiveIndex].MinX))
				+ XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_aabbs[primitiveIndex].MaxX)));
		XMMATRIX mTranslation = XMMatrixTranslationFromVector(vTranslation);
		//mTranslation = mTranslation*XMMatrixTranslation(0, 0, 1 * animationTime);
		XMMATRIX mTransform = mScale * mRotation * mTranslation;
		m_aabbPrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = mTransform;
		m_aabbPrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = XMMatrixInverse(nullptr, mTransform);
	};

	UINT offset = 0;

	// scale and rotation here
	for (int i = 0; i < numSpheres; i++) {
		SetTransformForAABBRotate(i, testScale, mRotation);
	}

	offset += AnalyticPrimitive::Count;

	// Volumetric primitives.
	{
		using namespace VolumetricPrimitive;
		SetTransformForAABBRotate(offset + Metaballs, mScale15, mRotation);
	}
}

void RTEngine::UpdateMovingSphere(float animationTime)
{
	auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

	auto SetTransformForAABBTranslate = [&](UINT primitiveIndex)
	{
		XMVECTOR vTranslation =
			(0.5f) * (XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_aabbs[primitiveIndex].MinX))
				+ XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_aabbs[primitiveIndex].MaxX)));

		XMMATRIX mTranslation = XMMatrixTranslationFromVector(vTranslation);

		mTranslation = mTranslation * XMMatrixTranslation(0, 0, animationTime);

		m_aabbPrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = mTranslation;
		m_aabbPrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = XMMatrixInverse(nullptr, mTranslation);
	};

	SetTransformForAABBTranslate(AnalyticPrimitive::MOVING);
}


void RTEngine::SetupLights()
{
	// Initialize the lighting parameters.
	XMFLOAT4 lightPosition;
	XMFLOAT4 lightAmbientColor;
	XMFLOAT4 lightDiffuseColor;

	lightPosition = XMFLOAT4(0.0f, 18.0f, -20.0f, 0.0f);
	m_sceneCB->lightPosition = XMLoadFloat4(&lightPosition);

	lightAmbientColor = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
	m_sceneCB->lightAmbientColor = XMLoadFloat4(&lightAmbientColor);

	float d = 0.6f;
	lightDiffuseColor = XMFLOAT4(d, d, d, 1.0f);
	m_sceneCB->lightDiffuseColor = XMLoadFloat4(&lightDiffuseColor);
}

void RTEngine::SetupCamera()
{
	// Initialize the view and projection inverse matrices.
	m_eye = { 22, 2, 3, 1 };
	m_at = { 0, 0, -4, 1 };
	m_up = { 0, 1, 0 };

	// Rotate camera around Y axis.
	XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(24.0f));
	m_eye = XMVector3Transform(m_eye, rotate);
	m_up = XMVector3Transform(m_up, rotate);

	UpdateCameraMatrices();
}

void RTEngine::MetaballDemo()
{
	using namespace AnalyticPrimitive;
	using namespace VolumetricPrimitive;
	int offset = 0;
	offset += AnalyticPrimitive::Count;

	auto SetAttributes = [&](
		UINT primitiveIndex,
		const XMFLOAT4& albedo,
		float reflectanceCoef = 0.0f,
		float fuzz = 0.1f,
		float diffuseCoef = 0.9f,
		float specularCoef = 0.7f,
		float specularPower = 50.0f,
		float stepScale = 1.0f
		)
	{
		auto& attributes = m_aabbMaterialCB[primitiveIndex];
		attributes.albedo = albedo;
		attributes.reflectanceCoef = reflectanceCoef;
		attributes.diffuseCoef = diffuseCoef;
		attributes.specularCoef = specularCoef;
		attributes.specularPower = specularPower;
		attributes.fuzz = fuzz;
		attributes.hasTexture = true;
	};

	// Volumetric primitives.
	{
		using namespace VolumetricPrimitive;
		SetAttributes(offset + Metaballs, ChromiumReflectance, .7);
	}


	XMINT3 aabbGrid = XMINT3(4, 1, 4);
	const XMFLOAT3 basePosition =
	{
		-(aabbGrid.x * c_aabbWidth + (aabbGrid.x - 1) * c_aabbDistance) / 2.0f,
		-(aabbGrid.y * c_aabbWidth + (aabbGrid.y - 1) * c_aabbDistance) / 2.0f,
		-(aabbGrid.z * c_aabbWidth + (aabbGrid.z - 1) * c_aabbDistance) / 2.0f,
	};

	XMFLOAT3 stride = XMFLOAT3(c_aabbWidth + c_aabbDistance, c_aabbWidth + c_aabbDistance, c_aabbWidth + c_aabbDistance);
	auto InitializeAABB = [&](auto& offsetIndex, auto& size)
	{
		return D3D12_RAYTRACING_AABB{
			basePosition.x + offsetIndex.x * stride.x,
			basePosition.y + offsetIndex.y * stride.y,
			basePosition.z + offsetIndex.z * stride.z,
			basePosition.x + offsetIndex.x * stride.x + size.x,
			basePosition.y + offsetIndex.y * stride.y + size.y,
			basePosition.z + offsetIndex.z * stride.z + size.z,
		};
	};

	m_aabbs.resize(IntersectionShaderType::TotalPrimitiveCount);


	offset = AnalyticPrimitive::Count;
	m_aabbs[offset + Metaballs] = InitializeAABB(XMINT3(0, 0, 0), XMFLOAT3(3, 3, 3));
	auto device = m_deviceResources->GetD3DDevice();
	AllocateUploadBuffer(device, m_aabbs.data(), m_aabbs.size() * sizeof(m_aabbs[0]), &m_aabbBuffer.resource);
}

void RTEngine::RTIAWRandomScene()
{
	// Goal: world.add(sphere(location, size, sphereMaterial))

	using namespace AnalyticPrimitive;
	this->numSpheres = AnalyticPrimitive::Count;

	m_aabbs.resize(IntersectionShaderType::TotalPrimitiveCount);

	// new version of materials WIP
	Material glassMat;
	glassMat.reflectanceCoef = 1; glassMat.diffuseCoef = 0;  glassMat.specularCoef = .7; glassMat.specularPower = 150; glassMat.refractionIndex = 1.7; 

	Material diffuseMat;
	diffuseMat.reflectanceCoef = 0; diffuseMat.diffuseCoef = 2;  diffuseMat.specularCoef = .1;

	Material metalMat;
	metalMat.reflectanceCoef = .9; metalMat.diffuseCoef = 0;

	int sphereIndex = 0;

	// create large spheres in the middle

	float largeRadius = 1;
	Sphere* largeGlass = new Sphere(sphereIndex, &glassMat, glass, XMFLOAT3(0, 1, 0), largeRadius);
	sphereIndex++;

	Sphere* largeDiffuse = new Sphere(sphereIndex, &diffuseMat, brown, XMFLOAT3(-3, 1, 0), largeRadius);
	sphereIndex++;

	Sphere* largeMetal = new Sphere(sphereIndex, &metalMat, grey, XMFLOAT3(3, 1, 0), largeRadius);
	sphereIndex++;

	SetSphereGPU(largeGlass);
	SetSphereGPU(largeDiffuse);
	SetSphereGPU(largeMetal);

	delete largeGlass;
	delete largeDiffuse;
	delete largeMetal;

	int numLoops = 10;
	float offsetX = .5;
	float offsetY = .5;

	// create random mini spheres, logic comes from RTIAW

	for (int a = -numLoops; a < numLoops; a++)
	{
		for (int b = -numLoops; b < numLoops; b++)
		{
			XMFLOAT3 center = XMFLOAT3(a*offsetX  + 0.9 * random_double(), .8, b*offsetY  + 0.9 * random_double());
			XMFLOAT3 cutoff = XMFLOAT3(3, .8, 0);
			float distance = getDistance(center, cutoff);
			sphereIndex++;
			if (distance > 0.9) {
				double chooseMat = random_double();
			
				XMFLOAT3 box = XMFLOAT3(3, 3, 3);
				Sphere* miniSphere = nullptr;
				float radius = .2f;
				float fuzz = random_double(0.01, 0.1);
				if (chooseMat < 0.7) {
					XMFLOAT4 randA = random();
					XMFLOAT4 randB = random();
					XMFLOAT4 randomAlbedo = XMFLOAT4(randA.x * randB.x, randA.y * randB.y, randA.z * randB.z, 1);
					miniSphere = new Sphere(sphereIndex, &diffuseMat, randomAlbedo, center, radius);
				}
				else if (chooseMat < 0.95) {
					XMFLOAT4 randomAlbedo = random(.4, 1);
					metalMat.fuzz = fuzz;
					miniSphere = new Sphere(sphereIndex, &metalMat, randomAlbedo, center, radius);
				}
				else {
					miniSphere = new Sphere(sphereIndex, &glassMat, glass, center, radius);
				}
				SetSphereGPU(miniSphere);
				delete miniSphere;
			}
		}
	}
}

void RTEngine::RTTNWDemo()
{
	// Goal: world.add(sphere(location, size, sphereMaterial))

	using namespace AnalyticPrimitive;
	this->numSpheres = AnalyticPrimitive::Count;

	m_aabbs.resize(IntersectionShaderType::TotalPrimitiveCount);

	Material glassMat;
	glassMat.reflectanceCoef = 1; glassMat.diffuseCoef = 0;  glassMat.specularCoef = .7; glassMat.specularPower = 100; glassMat.refractionIndex = 1.7;

	Material diffuseMat;
	diffuseMat.reflectanceCoef = 0; diffuseMat.diffuseCoef = 1;  diffuseMat.specularCoef = 0; 

	Material diffuseTexMat;
	diffuseTexMat.reflectanceCoef = 0; diffuseTexMat.diffuseCoef = 1;  diffuseTexMat.specularCoef = 0; diffuseTexMat.hasTexture = true;

	Material glossyMat;
	glossyMat.reflectanceCoef = 1; glossyMat.diffuseCoef = .5;  glossyMat.specularCoef = .7; glossyMat.specularPower = 100;

	Material metalMat;
	metalMat.reflectanceCoef = .7; metalMat.specularCoef = .7; metalMat.specularPower = 25;

	Material metalMatFuzzy;
	metalMatFuzzy.reflectanceCoef = .7; metalMatFuzzy.specularCoef = .7; metalMatFuzzy.specularPower = 25; metalMatFuzzy.fuzz = .02f;

	Material metalTexMat;
	metalTexMat.reflectanceCoef = .7; metalTexMat.specularCoef = .7; metalTexMat.specularPower = 25; metalTexMat.hasTexture = true;

	Material metalPerlinMat;
	metalPerlinMat.reflectanceCoef = .1; metalPerlinMat.hasPerlin = true;

	int ns = 350;
	int sphereIndex = 0;

	//Cube of mini spheres
	for (int j = 0; j < ns; j++) {
		XMFLOAT3 center = XMFLOAT3(random_double(.3, .6), random_double(1.2,1.5), random_double(1.5,1.2));
		sphereIndex++;
		float radius = .08f;
		Sphere* miniSphere = new Sphere(sphereIndex, &diffuseMat, white, center, radius);
		
		SetSphereGPU(miniSphere); 
		delete miniSphere;
	}

	float largeRadius = .5;
	float fuzz = .02;

	Sphere* glassSphere = new Sphere(AnalyticPrimitive::GLASS, &glassMat, glass, XMFLOAT3(4, .875, -.7), largeRadius - .1);

	Sphere* blueGlossySphere = new Sphere(AnalyticPrimitive::BLUEGLOSSY, &glossyMat, blue, XMFLOAT3(3.3, .875, -.9), largeRadius);

	Sphere* metalSphere = new Sphere(AnalyticPrimitive::METAL, &metalMat, grey, XMFLOAT3(3.5, .875, -.1), largeRadius);

	Sphere* fuzzySphere = new Sphere(AnalyticPrimitive::FUZZY, &metalMatFuzzy, grey, XMFLOAT3(2.5, .875, .5), largeRadius);

	Sphere* movingSphere = new Sphere(AnalyticPrimitive::MOVING, &diffuseMat, brown, XMFLOAT3(.1, 1.4, -.1f), largeRadius);

	Sphere* texturedSphere = new Sphere(AnalyticPrimitive::TEXTURED, &diffuseTexMat, green, XMFLOAT3(2.4, .95, -1.2), largeRadius + .2);

	Sphere* texturedMetalSphere = new Sphere(AnalyticPrimitive::TEXTUREDMETAL, &metalTexMat, grey, XMFLOAT3(1.5, .95, -.9), largeRadius + .2);

	Sphere* perlinSphere = new Sphere(AnalyticPrimitive::PERLIN, &metalPerlinMat, grey, XMFLOAT3(1.5, .95, -.1), largeRadius + .3);

	SetSphereGPU(glassSphere);
	SetSphereGPU(blueGlossySphere);
	SetSphereGPU(metalSphere);
	SetSphereGPU(fuzzySphere);
	SetSphereGPU(movingSphere);
	SetSphereGPU(texturedSphere);
	SetSphereGPU(texturedMetalSphere);
	SetSphereGPU(perlinSphere);

	delete glassSphere;
	delete blueGlossySphere;
	delete metalSphere;
	delete fuzzySphere;
	delete movingSphere;
	delete texturedSphere;
	delete texturedMetalSphere;
	delete perlinSphere;
}

void RTEngine::SetSphereGPU(Sphere* pSphere)
{
	auto SetAttributes = [&](
		UINT primitiveIndex,
		Sphere* sphere,
		Material& mat
		)
	{
		MaterialConstantBuffer& attributes = m_aabbMaterialCB[primitiveIndex];
		attributes.albedo = sphere->albedo;
		attributes.reflectanceCoef = mat.reflectanceCoef;
		attributes.diffuseCoef = mat.diffuseCoef;
		attributes.specularCoef = mat.specularCoef;
		attributes.specularPower = mat.specularPower;
		attributes.refractionIndex = mat.refractionIndex;
		attributes.radius = sphere->radius;
		attributes.fuzz = mat.fuzz;
		attributes.hasTexture = mat.hasTexture;
		attributes.hasPerlin = mat.hasPerlin;
	};

	// grid for the spheres
	XMINT3 aabbGrid = XMINT3(3, 3, 3);
	const XMFLOAT3 basePosition =
	{
		-(aabbGrid.x * c_aabbWidth + (aabbGrid.x - 1) * c_aabbDistance) / 2.0f,
		-(aabbGrid.y * c_aabbWidth + (aabbGrid.y - 1) * c_aabbDistance) / 2.0f,
		-(aabbGrid.z * c_aabbWidth + (aabbGrid.z - 1) * c_aabbDistance) / 2.0f,
	};

	XMFLOAT3 stride = XMFLOAT3(c_aabbWidth + c_aabbDistance, c_aabbWidth + c_aabbDistance, c_aabbWidth + c_aabbDistance);
	auto InitializeAABB = [&](auto& offsetIndex, auto& size)
	{
		return D3D12_RAYTRACING_AABB{
			basePosition.x + offsetIndex.x * stride.x,
			basePosition.y + offsetIndex.y * stride.y,
			basePosition.z + offsetIndex.z * stride.z,
			basePosition.x + offsetIndex.x * stride.x + size.x,
			basePosition.y + offsetIndex.y * stride.y + size.y,
			basePosition.z + offsetIndex.z * stride.z + size.z,
		};
	};

	XMFLOAT3 boxSize = XMFLOAT3(3, 3, 3);
	SetAttributes(pSphere->ID, pSphere, *pSphere->material);
	m_aabbs[pSphere->ID] = InitializeAABB(pSphere->center, boxSize);

	auto device = m_deviceResources->GetD3DDevice();
	AllocateUploadBuffer(device, m_aabbs.data(), m_aabbs.size() * sizeof(m_aabbs[0]), &m_aabbBuffer.resource);
}


// Create constant buffers.
void RTEngine::CreateConstantBuffers()
{
	auto device = m_deviceResources->GetD3DDevice();
	auto frameCount = m_deviceResources->GetBackBufferCount();

	m_sceneCB.Create(device, frameCount, L"Scene Constant Buffer");
}

// Create AABB primitive attributes buffers.
void RTEngine::CreateAABBPrimitiveAttributesBuffers()
{
	auto device = m_deviceResources->GetD3DDevice();
	auto frameCount = m_deviceResources->GetBackBufferCount();
	m_aabbPrimitiveAttributeBuffer.Create(device, IntersectionShaderType::TotalPrimitiveCount, frameCount, L"AABB primitive attributes");
}

// Create resources that depend on the device.
void RTEngine::CreateDeviceDependentResources()
{
	CreateAuxilaryDeviceResources();

	// Initialize raytracing pipeline.

	// Create raytracing interfaces: raytracing device and commandlist.
	CreateRaytracingInterfaces();

	// Create root signatures for the shaders.
	CreateRootSignatures();

	// Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
	CreateRaytracingPipelineStateObject();

	// Create a heap for descriptors.
	CreateDescriptorHeap();

	InitializeScene();

	// Build raytracing acceleration structures from the generated geometry.
	BuildAccelerationStructures();

	// Create constant buffers for the geometry and the scene.
	CreateConstantBuffers();

	// Create AABB primitive attribute buffers.
	CreateAABBPrimitiveAttributesBuffers();

	// Build shader tables, which define shaders and their local root arguments.
	BuildShaderTables();

	// Create an output 2D texture to store the raytracing result to.
	CreateRaytracingOutputResource();
}

void RTEngine::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
	auto device = m_deviceResources->GetD3DDevice();
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;

	ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
	ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

void RTEngine::CreateRootSignatures()
{
	auto device = m_deviceResources->GetD3DDevice();

	// Global Root Signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[2]; // Perfomance TIP: Order from most frequent to least frequent.
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.

		CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignature::Slot::Count];
		rootParameters[GlobalRootSignature::Slot::OutputView].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[GlobalRootSignature::Slot::AccelerationStructure].InitAsShaderResourceView(0);
		rootParameters[GlobalRootSignature::Slot::SceneConstant].InitAsConstantBufferView(0);
		rootParameters[GlobalRootSignature::Slot::AABBattributeBuffer].InitAsShaderResourceView(3);
		rootParameters[GlobalRootSignature::Slot::VertexBuffers].InitAsDescriptorTable(1, &ranges[1]);
		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_raytracingGlobalRootSignature);
	}

	// Local Root Signature
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	{
		// Triangle geometry
		{
			namespace RootSignatureSlots = LocalRootSignature::Triangle::Slot;
			CD3DX12_ROOT_PARAMETER rootParameters[RootSignatureSlots::Count];
			rootParameters[RootSignatureSlots::MaterialConstant].InitAsConstants(SizeOfInUint32(MaterialConstantBuffer), 1);

			CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
			localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle]);
		}

		// AABB geometry
		{
			namespace RootSignatureSlots = LocalRootSignature::AABB::Slot;
			CD3DX12_ROOT_PARAMETER rootParameters[RootSignatureSlots::Count];
			rootParameters[RootSignatureSlots::MaterialConstant].InitAsConstants(SizeOfInUint32(MaterialConstantBuffer), 1);
			rootParameters[RootSignatureSlots::GeometryIndex].InitAsConstants(SizeOfInUint32(PrimitiveInstanceConstantBuffer), 2);

			CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
			localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_raytracingLocalRootSignature[LocalRootSignature::Type::AABB]);
		}
	}
}

// Create raytracing device and command list.
void RTEngine::CreateRaytracingInterfaces()
{
	auto device = m_deviceResources->GetD3DDevice();
	auto commandList = m_deviceResources->GetCommandList();

	ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
	ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
}

// DXIL library
// This contains the shaders and their entrypoints for the state object.
// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
void RTEngine::CreateDxilLibrarySubobject(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
	auto lib = raytracingPipeline->CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
	lib->SetDXILLibrary(&libdxil);
	// Use default shader exports for a DXIL library/collection subobject ~ surface all shaders.
}

// Hit groups
// A hit group specifies closest hit, any hit and intersection shaders 
// to be executed when a ray intersects the geometry.
void RTEngine::CreateHitGroupSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
	// Triangle geometry hit groups
	{
		for (UINT rayType = 0; rayType < RayType::Count; rayType++)
		{
			auto hitGroup = raytracingPipeline->CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
			if (rayType == RayType::Radiance)
			{
				hitGroup->SetClosestHitShaderImport(c_closestHitShaderNames[GeometryType::Triangle]);
			}
			hitGroup->SetHitGroupExport(c_hitGroupNames_TriangleGeometry[rayType]);
			hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
		}
	}

	// AABB geometry hit groups
	{
		// Create hit groups for each intersection shader.
		for (UINT t = 0; t < IntersectionShaderType::Count; t++)
			for (UINT rayType = 0; rayType < RayType::Count; rayType++)
			{
				auto hitGroup = raytracingPipeline->CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
				hitGroup->SetIntersectionShaderImport(c_intersectionShaderNames[t]);
				if (rayType == RayType::Radiance)
				{
					hitGroup->SetClosestHitShaderImport(c_closestHitShaderNames[GeometryType::AABB]);
				}
				hitGroup->SetHitGroupExport(c_hitGroupNames_AABBGeometry[t][rayType]);
				hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
			}
	}

}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void RTEngine::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
	// Ray gen and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

	// Hit groups
	// Triangle geometry
	{
		auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
		localRootSignature->SetRootSignature(m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle].Get());
		// Shader association
		auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
		rootSignatureAssociation->AddExports(c_hitGroupNames_TriangleGeometry);
	}

	// AABB geometry
	{
		auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
		localRootSignature->SetRootSignature(m_raytracingLocalRootSignature[LocalRootSignature::Type::AABB].Get());
		// Shader association
		auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
		for (auto& hitGroupsForIntersectionShaderType : c_hitGroupNames_AABBGeometry)
		{
			rootSignatureAssociation->AddExports(hitGroupsForIntersectionShaderType);
		}
	}
}

// Create a raytracing pipeline state object (RTPSO).
// An RTPSO represents a full set of shaders reachable by a DispatchRays() call,
// with all configuration options resolved, such as local signatures and other state.
void RTEngine::CreateRaytracingPipelineStateObject()
{
	// Create 18 subobjects that combine into a RTPSO:
	// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
	// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
	// This simple sample utilizes default shader association except for local root signature subobject
	// which has an explicit association specified purely for demonstration purposes.
	// 1 - DXIL library
	// 8 - Hit group types - 4 geometries (1 triangle, 3 aabb) x 2 ray types (ray, shadowRay)
	// 1 - Shader config
	// 6 - 3 x Local root signature and association
	// 1 - Global root signature
	// 1 - Pipeline config
	CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// DXIL library
	CreateDxilLibrarySubobject(&raytracingPipeline);

	// Hit groups
	CreateHitGroupSubobjects(&raytracingPipeline);

	// Shader config
	// Defines the maximum sizes in bytes for the ray rayPayload and attribute structure.
	auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = max(sizeof(RayPayload), sizeof(ShadowRayPayload));
	UINT attributeSize = sizeof(struct ProceduralPrimitiveAttributes);
	shaderConfig->Config(payloadSize, attributeSize);

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	CreateLocalRootSignatureSubobjects(&raytracingPipeline);

	// Global root signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(m_raytracingGlobalRootSignature.Get());

	// Pipeline config
	// Defines the maximum TraceRay() recursion depth.
	auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	// PERFOMANCE TIP: Set max recursion depth as low as needed
	// as drivers may apply optimization strategies for low recursion depths.
	UINT maxRecursionDepth = MAX_RAY_RECURSION_DEPTH;
	pipelineConfig->Config(maxRecursionDepth);

	PrintStateObjectDesc(raytracingPipeline);

	// Create the state object.
	ThrowIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
}

// Create a 2D output texture for raytracing.
void RTEngine::CreateRaytracingOutputResource()
{
	auto device = m_deviceResources->GetD3DDevice();
	auto backbufferFormat = m_deviceResources->GetBackBufferFormat();

	// Create the output resource. The dimensions and format should match the swap-chain.
	auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_raytracingOutput)));
	NAME_D3D12_OBJECT(m_raytracingOutput);

	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
	m_raytracingOutputResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle, m_raytracingOutputResourceUAVDescriptorHeapIndex);
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
	m_raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, m_descriptorSize);
}

void RTEngine::CreateAuxilaryDeviceResources()
{
	auto device = m_deviceResources->GetD3DDevice();
	auto commandQueue = m_deviceResources->GetCommandQueue();

	for (auto& gpuTimer : m_gpuTimers)
	{
		gpuTimer.RestoreDevice(device, commandQueue, FrameCount);
	}
}

void RTEngine::CreateDescriptorHeap()
{
	auto device = m_deviceResources->GetD3DDevice();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	// Allocate a heap for 6 descriptors:
	// 2 - vertex and index  buffer SRVs
	// 1 - raytracing output texture SRV
	descriptorHeapDesc.NumDescriptors = 6;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 0;
	device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
	NAME_D3D12_OBJECT(m_descriptorHeap);

	m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void RTEngine::BuildPlaneGeometry()
{
	m_planeMaterialCB = { XMFLOAT4(0.5f, 0.5f, 0.6f, 1.0f), 0.1f, 2, 0.1f, 50, 1 };

	auto device = m_deviceResources->GetD3DDevice();
	// Plane indices.
	Index indices[] =
	{
		3,1,0,
		2,1,3,

	};

	Vertex vertices[] =
	{
		{ XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
	};

	AllocateUploadBuffer(device, indices, sizeof(indices), &m_PlaneIndexBuffer.resource);
	AllocateUploadBuffer(device, vertices, sizeof(vertices), &m_PlaneVertexBuffer.resource);

	// Vertex buffer is passed to the shader along with index buffer as a descriptor range.
	UINT descriptorIndexIB = CreateBufferSRV(&m_PlaneIndexBuffer, sizeof(indices) / 4, 0);
	UINT descriptorIndexVB = CreateBufferSRV(&m_PlaneVertexBuffer, ARRAYSIZE(vertices), sizeof(vertices[0]));
	ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index");
}

void RTEngine::BuildTetrahedronGeometry()
{
	auto device = m_deviceResources->GetD3DDevice();
	// Plane indices.
	Index indices[] =
	{
		0, 1, 2,
		0, 3, 1,
		0, 2, 3,
		1, 3, 2

	};

	Vertex vertices[] =
	{
		{ XMFLOAT3(std::sqrtf(8.f / 9.f), 0.f, -1.f / 3.f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-std::sqrtf(2.f / 9.f), std::sqrtf(2.f / 3.f), -1.f / 3.f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-std::sqrtf(2.f / 9.f), -std::sqrtf(2.f / 3.f), -1.f / 3.f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(0.f, 0.f, 1.f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
	};

	AllocateUploadBuffer(device, indices, sizeof(indices), &m_TetraIndexBuffer.resource);
	AllocateUploadBuffer(device, vertices, sizeof(vertices), &m_TetraVertexBuffer.resource);

	// Vertex buffer is passed to the shader along with index buffer as a descriptor range.
	UINT descriptorIndexIB = CreateBufferSRV(&m_TetraIndexBuffer, sizeof(indices) / 4, 0);
	UINT descriptorIndexVB = CreateBufferSRV(&m_TetraVertexBuffer, ARRAYSIZE(vertices), sizeof(vertices[0]));
	ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index");
}


// Build geometry used in the sample.
void RTEngine::BuildGeometry()
{
	BuildPlaneGeometry();
   // BuildTetrahedronGeometry();
}

// Build geometry descs for bottom-level AS.
void RTEngine::BuildGeometryDescsForBottomLevelAS(array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs)
{
	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Triangle geometry desc
	{
		// Triangle bottom-level AS contains a single plane geometry.
		geometryDescs[BottomLevelASType::Triangle].resize(1);

		// Plane geometry
		auto& geometryDesc = geometryDescs[BottomLevelASType::Triangle][0];
		geometryDesc = {};
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.IndexBuffer = m_PlaneIndexBuffer.resource->GetGPUVirtualAddress();
		geometryDesc.Triangles.IndexCount = static_cast<UINT>(m_PlaneIndexBuffer.resource->GetDesc().Width) / sizeof(Index);
		geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geometryDesc.Triangles.VertexCount = static_cast<UINT>(m_PlaneVertexBuffer.resource->GetDesc().Width) / sizeof(Vertex);
		geometryDesc.Triangles.VertexBuffer.StartAddress = m_PlaneVertexBuffer.resource->GetGPUVirtualAddress();
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
		geometryDesc.Flags = geometryFlags;
	}

	// AABB geometry desc
	{
		D3D12_RAYTRACING_GEOMETRY_DESC aabbDescTemplate = {};
		aabbDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
		aabbDescTemplate.AABBs.AABBCount = 1;
		aabbDescTemplate.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
		aabbDescTemplate.Flags = geometryFlags;

		// One AABB primitive per geometry.
		geometryDescs[BottomLevelASType::AABB].resize(IntersectionShaderType::TotalPrimitiveCount, aabbDescTemplate);

		// Create AABB geometries. 
		// Having separate geometries allows of separate shader record binding per geometry.
		// In this sample, this lets us specify custom hit groups per AABB geometry.
		for (UINT i = 0; i < IntersectionShaderType::TotalPrimitiveCount; i++)
		{
			auto& geometryDesc = geometryDescs[BottomLevelASType::AABB][i];
			geometryDesc.AABBs.AABBs.StartAddress = m_aabbBuffer.resource->GetGPUVirtualAddress() + i * sizeof(D3D12_RAYTRACING_AABB);
		}
	}
}

AccelerationStructureBuffers RTEngine::BuildBottomLevelAS(const vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
	auto device = m_deviceResources->GetD3DDevice();
	auto commandList = m_deviceResources->GetCommandList();
	ComPtr<ID3D12Resource> scratch;
	ComPtr<ID3D12Resource> bottomLevelAS;

	// Get the size requirements for the scratch and AS buffers.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = buildFlags;
	bottomLevelInputs.NumDescs = static_cast<UINT>(geometryDescs.size());
	bottomLevelInputs.pGeometryDescs = geometryDescs.data();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	// Create a scratch buffer.
	AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	{
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAS, initialResourceState, L"BottomLevelAccelerationStructure");
	}

	// bottom-level AS desc.
	{
		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAS->GetGPUVirtualAddress();
	}

	// Build the acceleration structure.
	m_dxrCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

	AccelerationStructureBuffers bottomLevelASBuffers;
	bottomLevelASBuffers.accelerationStructure = bottomLevelAS;
	bottomLevelASBuffers.scratch = scratch;
	bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
	return bottomLevelASBuffers;
}

template <class InstanceDescType, class BLASPtrType>
void RTEngine::BuildBotomLevelASInstanceDescs(BLASPtrType* bottomLevelASaddresses, ComPtr<ID3D12Resource>* instanceDescsResource)
{
	auto device = m_deviceResources->GetD3DDevice();

	vector<InstanceDescType> instanceDescs;
	instanceDescs.resize(NUM_BLAS);

	// Width of a bottom-level AS geometry.
	// Make the plane a little larger than the actual number of primitives in each dimension.
	const XMUINT3 NUM_AABB = XMUINT3(70, 1, 70);
	const XMFLOAT3 fWidth = XMFLOAT3(
		NUM_AABB.x * c_aabbWidth + (NUM_AABB.x - 1) * c_aabbDistance,
		NUM_AABB.y * c_aabbWidth + (NUM_AABB.y - 1) * c_aabbDistance,
		NUM_AABB.z * c_aabbWidth + (NUM_AABB.z - 1) * c_aabbDistance);
	const XMVECTOR vWidth = XMLoadFloat3(&fWidth);


	// Bottom-level AS with a single plane.
	{
		auto& instanceDesc = instanceDescs[BottomLevelASType::Triangle];
		instanceDesc = {};
		instanceDesc.InstanceMask = 1;
		instanceDesc.InstanceContributionToHitGroupIndex = 0;
		instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::Triangle];

		// Calculate transformation matrix.
		const XMVECTOR vBasePosition = vWidth * XMLoadFloat3(&XMFLOAT3(-0.35f, 0.25f, -0.35f));

		// Scale in XZ dimensions.
		XMMATRIX mScale = XMMatrixScaling(fWidth.x, fWidth.y, fWidth.z);
		XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
		XMMATRIX mTransform = mScale * mTranslation;
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTransform);
	}

	// Create instanced bottom-level AS with procedural geometry AABBs.
	// Instances share all the data, except for a transform.
	{
		auto& instanceDesc = instanceDescs[BottomLevelASType::AABB];
		instanceDesc = {};
		instanceDesc.InstanceMask = 1;

		// Set hit group offset to beyond the shader records for the triangle AABB.
		instanceDesc.InstanceContributionToHitGroupIndex = BottomLevelASType::AABB * RayType::Count;
		instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::AABB];

		// Move all AABBS above the ground plane.
		XMMATRIX mTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&XMFLOAT3(0, c_aabbWidth / 2, 0)));
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTranslation);
	}
	UINT64 bufferSize = static_cast<UINT64>(instanceDescs.size() * sizeof(instanceDescs[0]));
	AllocateUploadBuffer(device, instanceDescs.data(), bufferSize, &(*instanceDescsResource), L"InstanceDescs");
};

AccelerationStructureBuffers RTEngine::BuildTopLevelAS(AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count], D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
	auto device = m_deviceResources->GetD3DDevice();
	auto commandList = m_deviceResources->GetCommandList();
	ComPtr<ID3D12Resource> scratch;
	ComPtr<ID3D12Resource> topLevelAS;

	// Get required sizes for an acceleration structure.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = buildFlags;
	topLevelInputs.NumDescs = NUM_BLAS;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	AllocateUAVBuffer(device, topLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	{
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &topLevelAS, initialResourceState, L"TopLevelAccelerationStructure");
	}

	// Create instance descs for the bottom-level acceleration structures.
	ComPtr<ID3D12Resource> instanceDescsResource;
	{
		D3D12_RAYTRACING_INSTANCE_DESC instanceDescs[BottomLevelASType::Count] = {};
		D3D12_GPU_VIRTUAL_ADDRESS bottomLevelASaddresses[BottomLevelASType::Count] =
		{
			bottomLevelAS[0].accelerationStructure->GetGPUVirtualAddress(),
			bottomLevelAS[1].accelerationStructure->GetGPUVirtualAddress()
		};
		BuildBotomLevelASInstanceDescs<D3D12_RAYTRACING_INSTANCE_DESC>(bottomLevelASaddresses, &instanceDescsResource);
	}

	// Top-level AS desc
	{
		topLevelBuildDesc.DestAccelerationStructureData = topLevelAS->GetGPUVirtualAddress();
		topLevelInputs.InstanceDescs = instanceDescsResource->GetGPUVirtualAddress();
		topLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
	}

	// Build acceleration structure.
	m_dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

	AccelerationStructureBuffers topLevelASBuffers;
	topLevelASBuffers.accelerationStructure = topLevelAS;
	topLevelASBuffers.instanceDesc = instanceDescsResource;
	topLevelASBuffers.scratch = scratch;
	topLevelASBuffers.ResultDataMaxSizeInBytes = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;
	return topLevelASBuffers;
}

// Build acceleration structure needed for raytracing.
void RTEngine::BuildAccelerationStructures()
{
	auto device = m_deviceResources->GetD3DDevice();
	auto commandList = m_deviceResources->GetCommandList();
	auto commandQueue = m_deviceResources->GetCommandQueue();
	auto commandAllocator = m_deviceResources->GetCommandAllocator();

	// Reset the command list for the acceleration structure construction.
	commandList->Reset(commandAllocator, nullptr);

	// Build bottom-level AS.
	AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count];
	array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count> geometryDescs;
	{
		BuildGeometryDescsForBottomLevelAS(geometryDescs);

		// Build all bottom-level AS.
		for (UINT i = 0; i < BottomLevelASType::Count; i++)
		{
			bottomLevelAS[i] = BuildBottomLevelAS(geometryDescs[i]);
		}
	}

	// Batch all resource barriers for bottom-level AS builds.
	D3D12_RESOURCE_BARRIER resourceBarriers[BottomLevelASType::Count];
	for (UINT i = 0; i < BottomLevelASType::Count; i++)
	{
		resourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS[i].accelerationStructure.Get());
	}
	commandList->ResourceBarrier(BottomLevelASType::Count, resourceBarriers);

	// Build top-level AS.
	AccelerationStructureBuffers topLevelAS = BuildTopLevelAS(bottomLevelAS);

	// Kick off acceleration structure construction.
	m_deviceResources->ExecuteCommandList();

	// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
	m_deviceResources->WaitForGpu();

	// Store the AS buffers. The rest of the buffers will be released once we exit the function.
	for (UINT i = 0; i < BottomLevelASType::Count; i++)
	{
		m_bottomLevelAS[i] = bottomLevelAS[i].accelerationStructure;
	}
	m_topLevelAS = topLevelAS.accelerationStructure;
}

// Build shader tables.
// This encapsulates all shader records - shaders and the arguments for their local root signatures.
void RTEngine::BuildShaderTables()
{
	auto device = m_deviceResources->GetD3DDevice();

	void* rayGenShaderID;
	void* missShaderIDs[RayType::Count];
	void* hitGroupShaderIDs_TriangleGeometry[RayType::Count];
	void* hitGroupShaderIDs_AABBGeometry[IntersectionShaderType::Count][RayType::Count];

	// A shader name look-up table for shader table debug print out.
	unordered_map<void*, wstring> shaderIdToStringMap;

	auto GetShaderIDs = [&](auto* stateObjectProperties)
	{
		rayGenShaderID = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
		shaderIdToStringMap[rayGenShaderID] = c_raygenShaderName;

		for (UINT i = 0; i < RayType::Count; i++)
		{
			missShaderIDs[i] = stateObjectProperties->GetShaderIdentifier(c_missShaderNames[i]);
			shaderIdToStringMap[missShaderIDs[i]] = c_missShaderNames[i];
		}
		for (UINT i = 0; i < RayType::Count; i++)
		{
			hitGroupShaderIDs_TriangleGeometry[i] = stateObjectProperties->GetShaderIdentifier(c_hitGroupNames_TriangleGeometry[i]);
			shaderIdToStringMap[hitGroupShaderIDs_TriangleGeometry[i]] = c_hitGroupNames_TriangleGeometry[i];
		}
		for (UINT r = 0; r < IntersectionShaderType::Count; r++)
			for (UINT c = 0; c < RayType::Count; c++)
			{
				hitGroupShaderIDs_AABBGeometry[r][c] = stateObjectProperties->GetShaderIdentifier(c_hitGroupNames_AABBGeometry[r][c]);
				shaderIdToStringMap[hitGroupShaderIDs_AABBGeometry[r][c]] = c_hitGroupNames_AABBGeometry[r][c];
			}
	};

	// Get shader identifiers.
	UINT shaderIDSize;
	{
		ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
		ThrowIfFailed(m_dxrStateObject.As(&stateObjectProperties));
		GetShaderIDs(stateObjectProperties.Get());
		shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	}

	/*************--------- Shader table layout -------*******************
	| --------------------------------------------------------------------
	| Shader table - HitGroupShaderTable:
	| [0] : MyHitGroup_Triangle
	| [1] : MyHitGroup_Triangle_ShadowRay
	| [2] : MyHitGroup_AABB_AnalyticPrimitive
	| [3] : MyHitGroup_AABB_AnalyticPrimitive_ShadowRay
	| ...
	| [6] : MyHitGroup_AABB_VolumetricPrimitive
	| [7] : MyHitGroup_AABB_VolumetricPrimitive_ShadowRay
	| [8] : MyHitGroup_AABB_SignedDistancePrimitive
	| [9] : MyHitGroup_AABB_SignedDistancePrimitive_ShadowRay,
	| ...
	| [20] : MyHitGroup_AABB_SignedDistancePrimitive
	| [21] : MyHitGroup_AABB_SignedDistancePrimitive_ShadowRay
	| --------------------------------------------------------------------
	**********************************************************************/

	// RayGen shader table.
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIDSize; // No root arguments

		ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.push_back(ShaderRecord(rayGenShaderID, shaderRecordSize, nullptr, 0));
		rayGenShaderTable.DebugPrint(shaderIdToStringMap);
		m_rayGenShaderTable = rayGenShaderTable.GetResource();
	}

	// Miss shader table.
	{
		UINT numShaderRecords = RayType::Count;
		UINT shaderRecordSize = shaderIDSize; // No root arguments

		ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
		for (UINT i = 0; i < RayType::Count; i++)
		{
			missShaderTable.push_back(ShaderRecord(missShaderIDs[i], shaderIDSize, nullptr, 0));
		}
		missShaderTable.DebugPrint(shaderIdToStringMap);
		m_missShaderTableStrideInBytes = missShaderTable.GetShaderRecordSize();
		m_missShaderTable = missShaderTable.GetResource();
	}

	// Hit group shader table.
	{
		UINT numShaderRecords = RayType::Count + IntersectionShaderType::TotalPrimitiveCount * RayType::Count;
		UINT shaderRecordSize = shaderIDSize + LocalRootSignature::MaxRootArgumentsSize();
		ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

		// Triangle geometry hit groups.
		{
			LocalRootSignature::Triangle::RootArguments rootArgs;
			rootArgs.materialCb = m_planeMaterialCB;

			for (auto& hitGroupShaderID : hitGroupShaderIDs_TriangleGeometry)
			{
				hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
			}
		}

		// AABB geometry hit groups.
		{
			LocalRootSignature::AABB::RootArguments rootArgs;
			UINT instanceIndex = 0;

			// Create a shader record for each primitive.
			for (UINT iShader = 0, instanceIndex = 0; iShader < IntersectionShaderType::Count; iShader++)
			{
				UINT numPrimitiveTypes = IntersectionShaderType::PerPrimitiveTypeCount(static_cast<IntersectionShaderType::Enum>(iShader));

				// Primitives for each intersection shader.
				for (UINT primitiveIndex = 0; primitiveIndex < numPrimitiveTypes; primitiveIndex++, instanceIndex++)
				{
					rootArgs.materialCb = m_aabbMaterialCB[instanceIndex];
					rootArgs.aabbCB.instanceIndex = instanceIndex;
					rootArgs.aabbCB.primitiveType = primitiveIndex;

					// Ray types.
					for (UINT r = 0; r < RayType::Count; r++)
					{
						auto& hitGroupShaderID = hitGroupShaderIDs_AABBGeometry[iShader][r];
						hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
					}
				}
			}
		}
		hitGroupShaderTable.DebugPrint(shaderIdToStringMap);
		m_hitGroupShaderTableStrideInBytes = hitGroupShaderTable.GetShaderRecordSize();
		m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
	}
}

void RTEngine::OnKeyDown(UINT8 key)
{
	float angleToRotateBy = .1f;
	XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
	switch (key)
	{
	case 'C':
		m_animateCamera = !m_animateCamera;
		break;
	case 'G':
		m_animateGeometry = !m_animateGeometry;
		break;
	case 'L':
		m_animateLight = !m_animateLight;
		break;
	case 'D':
		angleToRotateBy *= -1.0f;
		rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
		m_eye = XMVector3Transform(m_eye, rotate);
		m_up = XMVector3Transform(m_up, rotate);
		m_at = XMVector3Transform(m_at, rotate);
		UpdateCameraMatrices();
		break;
	case 'A':
		m_eye = XMVector3Transform(m_eye, rotate);
		m_up = XMVector3Transform(m_up, rotate);
		m_at = XMVector3Transform(m_at, rotate);
		UpdateCameraMatrices();
		break;
	}
}

// Update frame-based values.
bool flip = true;
void RTEngine::OnUpdate()
{
	m_timer.Tick();
	CalculateFrameStats();
	float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
	auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
	auto prevFrameIndex = m_deviceResources->GetPreviousFrameIndex();

	// Rotate the camera around Y axis.
	if (m_animateCamera)
	{
		float secondsToRotateAround = 48.0f;
		float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
		XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
		m_eye = XMVector3Transform(m_eye, rotate);
		m_up = XMVector3Transform(m_up, rotate);
		m_at = XMVector3Transform(m_at, rotate);
		UpdateCameraMatrices();
	}

	// Rotate the second light around Y axis.
	if (m_animateLight)
	{
		float secondsToRotateAround = 8.0f;
		float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
		XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
		const XMVECTOR& prevLightPosition = m_sceneCB->lightPosition;
		m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate);
	}

	// Transform the procedural geometry.
	if (m_animateGeometry)
	{
		m_animateRotationTime += elapsedTime;
		if (flip) {
			m_animateMovingSphereTime += elapsedTime;
		}
		else {
			m_animateMovingSphereTime -= elapsedTime;
		}
		if (m_animateMovingSphereTime >1 || m_animateMovingSphereTime < -1) {
			flip = !flip;
		}
	}
	UpdateAABBPrimitiveTransform(m_animateRotationTime);
	UpdateMovingSphere(m_animateMovingSphereTime);
	m_sceneCB->elapsedTime = m_animateRotationTime;
}

// SetPipelineState1: sets pipeline state containing raytracing shaders on command list 
// DispatchRays: invokes raytracing
void RTEngine::DoRaytracing()
{
	auto commandList = m_deviceResources->GetCommandList();
	auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

	auto DispatchRays = [&](auto* raytracingCommandList, auto* stateObject, auto* dispatchDesc)
	{
		dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
		dispatchDesc->HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
		dispatchDesc->HitGroupTable.StrideInBytes = m_hitGroupShaderTableStrideInBytes;
		dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
		dispatchDesc->MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
		dispatchDesc->MissShaderTable.StrideInBytes = m_missShaderTableStrideInBytes;
		dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
		dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
		dispatchDesc->Width = m_width;
		dispatchDesc->Height = m_height;
		dispatchDesc->Depth = 1;
		raytracingCommandList->SetPipelineState1(stateObject);

		m_gpuTimers[GpuTimers::Raytracing].Start(commandList);
		raytracingCommandList->DispatchRays(dispatchDesc);
		m_gpuTimers[GpuTimers::Raytracing].Stop(commandList);
	};

	auto SetCommonPipelineState = [&](auto* descriptorSetCommandList)
	{
		descriptorSetCommandList->SetDescriptorHeaps(1, m_descriptorHeap.GetAddressOf());
		// Set index and successive vertex buffer decriptor tables.
		commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::VertexBuffers, m_PlaneIndexBuffer.gpuDescriptorHandle);
		//   commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::VertexBuffers, m_TetraIndexBuffer.gpuDescriptorHandle);
		commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::OutputView, m_raytracingOutputResourceUAVGpuDescriptor);
	};

	commandList->SetComputeRootSignature(m_raytracingGlobalRootSignature.Get());

	// Copy dynamic buffers to GPU.
	{
		m_sceneCB.CopyStagingToGpu(frameIndex);
		commandList->SetComputeRootConstantBufferView(GlobalRootSignature::Slot::SceneConstant, m_sceneCB.GpuVirtualAddress(frameIndex));

		m_aabbPrimitiveAttributeBuffer.CopyStagingToGpu(frameIndex);
		commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::AABBattributeBuffer, m_aabbPrimitiveAttributeBuffer.GpuVirtualAddress(frameIndex));
	}

	// Bind the heaps, acceleration structure and dispatch rays.  
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	SetCommonPipelineState(commandList);
	commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::AccelerationStructure, m_topLevelAS->GetGPUVirtualAddress());
	DispatchRays(m_dxrCommandList.Get(), m_dxrStateObject.Get(), &dispatchDesc);
}

// Update the application state with the new resolution.
void RTEngine::UpdateForSizeChange(UINT width, UINT height)
{
	DXSample::UpdateForSizeChange(width, height);
}

// Copy the raytracing output to the backbuffer.
void RTEngine::CopyRaytracingOutputToBackbuffer()
{
	auto commandList = m_deviceResources->GetCommandList();
	auto renderTarget = m_deviceResources->GetRenderTarget();

	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	commandList->CopyResource(renderTarget, m_raytracingOutput.Get());

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}

// Create resources that are dependent on the size of the main window.
void RTEngine::CreateWindowSizeDependentResources()
{
	CreateRaytracingOutputResource();
	UpdateCameraMatrices();
}

// Release resources that are dependent on the size of the main window.
void RTEngine::ReleaseWindowSizeDependentResources()
{
	m_raytracingOutput.Reset();
}

// Release all resources that depend on the device.
void RTEngine::ReleaseDeviceDependentResources()
{
	for (auto& gpuTimer : m_gpuTimers)
	{
		gpuTimer.ReleaseDevice();
	}

	m_raytracingGlobalRootSignature.Reset();
	ResetComPtrArray(&m_raytracingLocalRootSignature);

	m_dxrDevice.Reset();
	m_dxrCommandList.Reset();
	m_dxrStateObject.Reset();

	m_raytracingGlobalRootSignature.Reset();
	ResetComPtrArray(&m_raytracingLocalRootSignature);

	m_descriptorHeap.Reset();
	m_descriptorsAllocated = 0;
	m_sceneCB.Release();
	m_aabbPrimitiveAttributeBuffer.Release();
	m_PlaneIndexBuffer.resource.Reset();
	m_PlaneVertexBuffer.resource.Reset();
	/*  m_TetraIndexBuffer.resource.Reset();
	  m_TetraVertexBuffer.resource.Reset();*/
	m_aabbBuffer.resource.Reset();

	ResetComPtrArray(&m_bottomLevelAS);
	m_topLevelAS.Reset();

	m_raytracingOutput.Reset();
	m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
	m_rayGenShaderTable.Reset();
	m_missShaderTable.Reset();
	m_hitGroupShaderTable.Reset();
}

void RTEngine::RecreateD3D()
{
	// Give GPU a chance to finish its execution in progress.
	try
	{
		m_deviceResources->WaitForGpu();
	}
	catch (HrException&)
	{
		// Do nothing, currently attached adapter is unresponsive.
	}
	m_deviceResources->HandleDeviceLost();
}

// Render the scene.
void RTEngine::OnRender()
{
	if (!m_deviceResources->IsWindowVisible())
	{
		return;
	}

	auto device = m_deviceResources->GetD3DDevice();
	auto commandList = m_deviceResources->GetCommandList();

	// Begin frame.
	m_deviceResources->Prepare();
	for (auto& gpuTimer : m_gpuTimers)
	{
		gpuTimer.BeginFrame(commandList);
	}

	DoRaytracing();
	CopyRaytracingOutputToBackbuffer();

	// End frame.
	for (auto& gpuTimer : m_gpuTimers)
	{
		gpuTimer.EndFrame(commandList);
	}

	m_deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void RTEngine::OnDestroy()
{
	// Let GPU finish before releasing D3D resources.
	m_deviceResources->WaitForGpu();
	OnDeviceLost();
}

// Release all device dependent resouces when a device is lost.
void RTEngine::OnDeviceLost()
{
	ReleaseWindowSizeDependentResources();
	ReleaseDeviceDependentResources();
}

// Create all device dependent resources when a device is restored.
void RTEngine::OnDeviceRestored()
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Compute the average frames per second and million rays per second.
void RTEngine::CalculateFrameStats()
{
	static int frameCnt = 0;
	static double prevTime = 0.0f;
	double totalTime = m_timer.GetTotalSeconds();

	frameCnt++;

	// Compute averages over one second period.
	if ((totalTime - prevTime) >= 1.0f)
	{
		float diff = static_cast<float>(totalTime - prevTime);
		float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

		frameCnt = 0;
		prevTime = totalTime;
		float raytracingTime = static_cast<float>(m_gpuTimers[GpuTimers::Raytracing].GetElapsedMS());
		float MRaysPerSecond = NumMRaysPerSecond(m_width, m_height, raytracingTime);

		wstringstream windowText;
		windowText << setprecision(2) << fixed
			<< L"    fps: " << fps
			<< L"    DispatchRays(): " << raytracingTime << "ms"
			<< L"     ~Million Primary Rays/s: " << MRaysPerSecond
			<< L"    GPU[" << m_deviceResources->GetAdapterID() << L"]: " << m_deviceResources->GetAdapterDescription();
		SetCustomWindowText(windowText.str().c_str());
	}
}

// Handle OnSizeChanged message event.
void RTEngine::OnSizeChanged(UINT width, UINT height, bool minimized)
{
	if (!m_deviceResources->WindowSizeChanged(width, height, minimized))
	{
		return;
	}

	UpdateForSizeChange(width, height);

	ReleaseWindowSizeDependentResources();
	CreateWindowSizeDependentResources();
}

// Allocate a descriptor and return its index. 
// If the passed descriptorIndexToUse is valid, it will be used instead of allocating a new one.
UINT RTEngine::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
	auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
	{
		ThrowIfFalse(m_descriptorsAllocated < m_descriptorHeap->GetDesc().NumDescriptors, L"Ran out of descriptors on the heap!");
		descriptorIndexToUse = m_descriptorsAllocated++;
	}
	*cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
	return descriptorIndexToUse;
}

// Create a SRV for a buffer.
UINT RTEngine::CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)
{
	auto device = m_deviceResources->GetD3DDevice();

	// SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = numElements;
	if (elementSize == 0)
	{
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		srvDesc.Buffer.StructureByteStride = 0;
	}
	else
	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		srvDesc.Buffer.StructureByteStride = elementSize;
	}
	UINT descriptorIndex = AllocateDescriptor(&buffer->cpuDescriptorHandle);
	device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
	buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
	return descriptorIndex;
}

