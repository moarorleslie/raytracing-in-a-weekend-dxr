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

#ifndef RTENGINE_H
#define RTENGINE_H



#include "DXSample.h"
#include "StepTimer.h"
#include "RaytracingSceneDefines.h"
#include "DirectXRaytracingHelper.h"
#include "PerformanceTimers.h"
#include "Material.h"
#include "Sphere.h"



class RTEngine : public DXSample
{
public:

    RTEngine(UINT width, UINT height, std::wstring name);

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    virtual void OnInit();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized);
    virtual void OnDestroy();
    virtual IDXGISwapChain* GetSwapchain() { return m_deviceResources->GetSwapChain(); }

private:
    enum class DemoType {
        RTIAW = 0,
        RTTNW,
        METABALLS
    };

    static const UINT FrameCount = 3;

    // Constants.
    const UINT NUM_BLAS = 2;          // Triangle + AABB bottom-level AS.
    const float c_aabbWidth = 2;      // AABB width.
    const float c_aabbDistance = 2;   // Distance between AABBs.
    
    // DirectX Raytracing (DXR) attributes
    ComPtr<ID3D12Device5> m_dxrDevice;
    ComPtr<ID3D12GraphicsCommandList5> m_dxrCommandList;
    ComPtr<ID3D12StateObject> m_dxrStateObject;

    // Root signatures
    ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
    ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature[LocalRootSignature::Type::Count];

    // Descriptors
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    UINT m_descriptorsAllocated;
    UINT m_descriptorSize;

    // Raytracing scene
    ConstantBuffer<SceneConstantBuffer> m_sceneCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> m_aabbPrimitiveAttributeBuffer;
    std::vector<D3D12_RAYTRACING_AABB> m_aabbs;

    // Root constants
    Material myMaterials[IntersectionShaderType::TotalPrimitiveCount];
    MaterialConstantBuffer m_planeMaterialCB;
    MaterialConstantBuffer m_aabbMaterialCB[IntersectionShaderType::TotalPrimitiveCount];

    // Geometry
    D3DBuffer m_PlaneIndexBuffer;
    D3DBuffer m_PlaneVertexBuffer;
    D3DBuffer m_TetraIndexBuffer;
    D3DBuffer m_TetraVertexBuffer;
    D3DBuffer m_aabbBuffer;

    // Acceleration structure
    ComPtr<ID3D12Resource> m_bottomLevelAS[BottomLevelASType::Count];
    ComPtr<ID3D12Resource> m_topLevelAS;

    // Raytracing output
    ComPtr<ID3D12Resource> m_raytracingOutput;
    D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
    UINT m_raytracingOutputResourceUAVDescriptorHeapIndex;

    // Shader tables
    static const wchar_t* c_hitGroupNames_TriangleGeometry[RayType::Count];
    static const wchar_t* c_hitGroupNames_AABBGeometry[IntersectionShaderType::Count][RayType::Count];
    static const wchar_t* c_raygenShaderName;
    static const wchar_t* c_intersectionShaderNames[IntersectionShaderType::Count];
    static const wchar_t* c_closestHitShaderNames[GeometryType::Count];
    static const wchar_t* c_missShaderNames[RayType::Count];

    ComPtr<ID3D12Resource> m_missShaderTable;
    UINT m_missShaderTableStrideInBytes;
    ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    UINT m_hitGroupShaderTableStrideInBytes;
    ComPtr<ID3D12Resource> m_rayGenShaderTable;

    // Application state
    DX::GPUTimer m_gpuTimers[GpuTimers::Count];
    StepTimer m_timer;
    float m_animateRotationTime;
    float m_animateMovingSphereTime;
    bool m_animateGeometry;
    bool m_animateCamera;
    bool m_animateLight;
    XMVECTOR m_eye;
    XMVECTOR m_at;
    XMVECTOR m_up;
    int numSpheres = 0;

    void UpdateCameraMatrices();
    void UpdateMovingSphere(float animationTime);
    void UpdateAABBPrimitiveTransform(float animationTime);
    void InitializeScene();
    void SetupMaterials();
    void SetupLights();
    void SetupCamera();
    void RTTNWDemo();
    void RTIAWRandomScene();
    void RecreateD3D();
    void DoRaytracing();
    void CreateConstantBuffers();
    void CreateAABBPrimitiveAttributesBuffers();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void ReleaseDeviceDependentResources();
    void ReleaseWindowSizeDependentResources();
    void CreateRaytracingInterfaces();
    void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);
    void CreateRootSignatures();
    void CreateDxilLibrarySubobject(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateHitGroupSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateRaytracingPipelineStateObject();
    void CreateAuxilaryDeviceResources();
    void CreateDescriptorHeap();
    void CreateRaytracingOutputResource();
    void MetaballDemo();
    void BuildGeometry();
    void BuildPlaneGeometry();
    void BuildTetrahedronGeometry();
    void BuildGeometryDescsForBottomLevelAS(std::array<std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs);
    template <class InstanceDescType, class BLASPtrType>
    void BuildBotomLevelASInstanceDescs(BLASPtrType *bottomLevelASaddresses, ComPtr<ID3D12Resource>* instanceDescsResource);
    AccelerationStructureBuffers BuildBottomLevelAS(const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
    AccelerationStructureBuffers BuildTopLevelAS(AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count], D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
    void BuildAccelerationStructures();
    void BuildShaderTables();
    void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
    void CopyRaytracingOutputToBackbuffer();
    void CalculateFrameStats();
    UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);
    UINT CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize);
    void SetSphereGPU(Sphere* pSphere);


    // Defined Albedos for testing
    XMFLOAT4 green = XMFLOAT4(0.1f, 1.0f, 0.5f, 1.0f);
    XMFLOAT4 red = XMFLOAT4(1.0f, 0.5f, 0.5f, 1.0f);
    XMFLOAT4 blue = XMFLOAT4(0.0f, 0.0f, 0.3f, 1.0f);
    XMFLOAT4 white = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    XMFLOAT4 grey = XMFLOAT4(0.549f, 0.556f, 0.554f, 1.0f);
    XMFLOAT4 glass = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
    XMFLOAT4 brown = XMFLOAT4(0.5f, 0.4f, 0.3f, 1.0f);
};
#endif // !RTENGINE_H