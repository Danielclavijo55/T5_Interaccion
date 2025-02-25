/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include <string>
#include <vector>
#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{

class Tutorial04_Instancing final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;
    virtual bool HandleNativeMessage(const void* pNativeMsgData) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial04: Instancing"; }

private:
    void CreatePipelineState();
    void CreateInstanceBuffer();
    void UpdateUI();
    void PopulateInstanceBuffer();
    
    // Métodos para control de cámara
    void UpdateCameraMatrices();
    void HandleMouseEvent(int x, int y, bool buttonDown, bool buttonUp, int wheel);

    // Estructuras para control de cámara
    struct CameraParams
    {
        float2 PanOffset = {0.0f, 0.0f};
        float Zoom = 1.0f;
        float OrbitAngleX = -0.8f;
        float OrbitAngleY = 0.0f;
        float OrbitDistance = 20.0f;
        
        // Parámetros para cámara libre (Ventana 3)
        float3 Position = {0.0f, 0.0f, 20.0f};
        float RotX = 0.0f;
        float RotY = 0.0f;
        float RotZ = 0.0f;
        float ViewZoom = 0.01f; // Factor de zoom para la ventana 3
    };
    
    RefCntAutoPtr<IPipelineState>         m_pPSO;
    RefCntAutoPtr<IBuffer>                m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_InstanceBuffer;
    RefCntAutoPtr<IBuffer>                m_VSConstants;
    RefCntAutoPtr<ITextureView>           m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;

    float4x4             m_ViewProjMatrix;
    float4x4             m_RotationMatrix;
    int                  m_GridSize   = 5;
    static constexpr int MaxGridSize  = 32;
    static constexpr int MaxInstances = MaxGridSize * MaxGridSize * MaxGridSize;
    
    // Cámaras para las tres ventanas
    CameraParams CameraWindow1; // Paneo y zoom
    CameraParams CameraWindow2; // Control orbital
    CameraParams CameraWindow3; // Cámara libre

    // Matrices de vista para cada ventana
    float4x4 ViewWindow1;
    float4x4 ViewWindow2;
    float4x4 ViewWindow3;
    
    // Variables para rastreo de mouse
    bool m_MouseCaptured = false;
    int m_ActiveWindow = -1; // -1: ninguna, 0: ventana 1, 1: ventana 2, 2: ventana 3
    float2 m_LastMousePos = {0.0f, 0.0f};
};

} // namespace Diligent
