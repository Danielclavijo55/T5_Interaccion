/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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

#include <random>

#include "Tutorial04_Instancing.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "../../Common/src/TexturedCube.hpp"
#include "imgui.h"

#ifdef PLATFORM_WIN32
#   include <Windows.h>
#endif

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial04_Instancing();
}

void Tutorial04_Instancing::CreatePipelineState()
{
    // clang-format off
    // Define vertex shader input layout
    // This tutorial uses two types of input: per-vertex data and per-instance data.
    LayoutElement LayoutElems[] =
    {
        // Per-vertex data - first buffer slot
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False},
            
        // Per-instance data - second buffer slot
        // We will use four attributes to encode instance-specific 4x4 transformation matrix
        // Attribute 2 - first row
        LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 3 - second row
        LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 4 - third row
        LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 5 - fourth row
        LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
    };
    // clang-format on

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    TexturedCube::CreatePSOInfo CubePsoCI;
    CubePsoCI.pDevice                = m_pDevice;
    CubePsoCI.RTVFormat              = m_pSwapChain->GetDesc().ColorBufferFormat;
    CubePsoCI.DSVFormat              = m_pSwapChain->GetDesc().DepthBufferFormat;
    CubePsoCI.pShaderSourceFactory   = pShaderSourceFactory;
    CubePsoCI.VSFilePath             = "cube_inst.vsh";
    CubePsoCI.PSFilePath             = "cube_inst.psh";
    CubePsoCI.ExtraLayoutElements    = LayoutElems;
    CubePsoCI.NumExtraLayoutElements = _countof(LayoutElems);

    m_pPSO = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);

    // Create dynamic uniform buffer that will store our transformation matrix
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly to the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

void Tutorial04_Instancing::CreateInstanceBuffer()
{
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * MaxInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
}

// Manejo de eventos nativos (mouse, teclado, etc.)
bool Tutorial04_Instancing::HandleNativeMessage(const void* pNativeMsgData)
{
#ifdef PLATFORM_WIN32
    const MSG* pMsg = reinterpret_cast<const MSG*>(pNativeMsgData);
    
    if (pMsg->message == WM_MOUSEMOVE ||
        pMsg->message == WM_LBUTTONDOWN ||
        pMsg->message == WM_LBUTTONUP ||
        pMsg->message == WM_MOUSEWHEEL)
    {
        int x = LOWORD(pMsg->lParam);
        int y = HIWORD(pMsg->lParam);
        bool buttonDown = (pMsg->message == WM_LBUTTONDOWN);
        bool buttonUp = (pMsg->message == WM_LBUTTONUP);
        int wheel = 0;
        
        if (pMsg->message == WM_MOUSEWHEEL)
        {
            wheel = GET_WHEEL_DELTA_WPARAM(pMsg->wParam) / WHEEL_DELTA;
        }
        
        HandleMouseEvent(x, y, buttonDown, buttonUp, wheel);
        return true; // Mensaje procesado
    }
    
#endif
    return false; // Mensaje no procesado
}

// Actualización de matrices de cámara
void Tutorial04_Instancing::UpdateCameraMatrices()
{
    // Ventana 1: Paneo y Zoom
    ViewWindow1 = float4x4::Translation(CameraWindow1.PanOffset.x, CameraWindow1.PanOffset.y, 0.0f) *
                  float4x4::Scale(CameraWindow1.Zoom, CameraWindow1.Zoom, CameraWindow1.Zoom) *
                  float4x4::RotationX(-0.8f) *
                  float4x4::Translation(0.f, 0.f, 20.0f);
    
    // Ventana 2: Control Orbital
    // Añadimos una matriz de escala con Y negativo para voltear el móvil verticalmente
    ViewWindow2 = float4x4::Translation(0.0f, 0.0f, -CameraWindow2.OrbitDistance) *
                  float4x4::RotationX(CameraWindow2.OrbitAngleX) *
                  float4x4::RotationY(CameraWindow2.OrbitAngleY) *
                  float4x4::Scale(1.0f, -1.0f, 1.0f); // Invierte el eje Y para corregir la orientación
    
    // Ventana 3: Cámara Libre con distancia aumentada
    float4x4 rotation = float4x4::RotationZ(CameraWindow3.RotZ) *
                        float4x4::RotationY(CameraWindow3.RotY) *
                        float4x4::RotationX(CameraWindow3.RotX);
    
    // Aplicamos un factor de escala extremadamente pequeño para alejar radicalmente la vista
    ViewWindow3 = rotation *
                  float4x4::Scale(CameraWindow3.ViewZoom, CameraWindow3.ViewZoom, CameraWindow3.ViewZoom) *
                  float4x4::Translation(-CameraWindow3.Position.x,
                                       -CameraWindow3.Position.y,
                                       -CameraWindow3.Position.z);
}

// Manejo de eventos de ratón para controles de cámara
void Tutorial04_Instancing::HandleMouseEvent(int x, int y, bool buttonDown, bool buttonUp, int wheel)
{
    const auto& SCDesc = m_pSwapChain->GetDesc();
    float screenPosX = static_cast<float>(x) / SCDesc.Width;
    
    // Determinar en qué ventana está el ratón
    int windowIdx = -1;
    if (screenPosX < 1.0f/3.0f)
        windowIdx = 0; // Ventana 1 (Paneo y Zoom)
    else if (screenPosX < 2.0f/3.0f)
        windowIdx = 1; // Ventana 2 (Control Orbital)
    else
        windowIdx = 2; // Ventana 3 (Cámara Libre)
    
    // Capturar/soltar ratón
    if (buttonDown)
    {
        m_MouseCaptured = true;
        m_ActiveWindow = windowIdx;
        m_LastMousePos = float2(static_cast<float>(x), static_cast<float>(y));
    }
    else if (buttonUp)
    {
        m_MouseCaptured = false;
        m_ActiveWindow = -1;
    }
    
    // Si tenemos el ratón capturado, procesar según la ventana activa
    if (m_MouseCaptured)
    {
        float2 currentPos = float2(static_cast<float>(x), static_cast<float>(y));
        float2 delta = currentPos - m_LastMousePos;
        
        switch(m_ActiveWindow)
        {
            case 0: // Ventana 1: Paneo
                // Convertir el movimiento del ratón a desplazamiento de paneo
                CameraWindow1.PanOffset.x += delta.x * 0.01f;
                CameraWindow1.PanOffset.y -= delta.y * 0.01f; // Invertir Y porque en pantalla Y crece hacia abajo
                break;
                
            case 1: // Ventana 2: Control Orbital
                // Convertir el movimiento del ratón a rotación orbital
                CameraWindow2.OrbitAngleY += delta.x * 0.01f;
                CameraWindow2.OrbitAngleX += delta.y * 0.01f;
                break;
                
            case 2: // Ventana 3: Cámara Libre
                // Rotación con mouse para cámara libre
                if (delta.x != 0.0f)
                    CameraWindow3.RotY += delta.x * 0.01f;
                if (delta.y != 0.0f)
                    CameraWindow3.RotX += delta.y * 0.01f;
                break;
        }
        
        m_LastMousePos = currentPos;
    }
    
    // Procesar la rueda del ratón para zoom/distancia
    if (wheel != 0)
    {
        if (windowIdx == 0) // Zoom en ventana 1
        {
            // Zoom in/out según la dirección de la rueda
            CameraWindow1.Zoom += static_cast<float>(wheel) * 0.1f;
            // Limitar el zoom a valores razonables
            CameraWindow1.Zoom = std::max(0.1f, std::min(CameraWindow1.Zoom, 5.0f));
        }
        else if (windowIdx == 1) // Ajustar distancia orbital en ventana 2
        {
            CameraWindow2.OrbitDistance -= static_cast<float>(wheel) * 1.0f;
            CameraWindow2.OrbitDistance = std::max(5.0f, std::min(CameraWindow2.OrbitDistance, 40.0f));
        }
        else if (windowIdx == 2) // Ajustar factor de zoom para ventana 3
        {
            if (wheel > 0)
                CameraWindow3.ViewZoom *= 1.2f; // Acercar
            else
                CameraWindow3.ViewZoom *= 0.8f; // Alejar
                
            // Limitar el zoom a valores razonables
            CameraWindow3.ViewZoom = std::max(0.001f, std::min(CameraWindow3.ViewZoom, 0.1f));
        }
    }
}

void Tutorial04_Instancing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatePipelineState();

    // Load textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    m_TextureSRV       = TexturedCube::LoadTexture(m_pDevice, "DGLogo.png")->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    // Set cube texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);

    CreateInstanceBuffer();
    
    // Inicializar las vistas de cámara
    ViewWindow1 = float4x4::RotationX(-0.8f) * float4x4::Translation(0.f, 0.f, 20.0f);
    ViewWindow2 = float4x4::RotationX(-0.8f) * float4x4::Translation(0.f, 0.f, 20.0f);
    ViewWindow3 = float4x4::RotationX(-0.8f) * float4x4::Translation(0.f, 0.f, 20.0f);
    
    // Inicializar parámetros de cámara
    CameraWindow1.Zoom = 1.0f;
    
    // Inicializar parámetros para ventana orbital con valores que muestren el móvil correctamente
    CameraWindow2.OrbitAngleX = 3.0f;  // Valor ajustado según la imagen donde se ve correctamente
    CameraWindow2.OrbitAngleY = 0.0f;
    CameraWindow2.OrbitDistance = 20.0f;
    
    // Inicializar parámetros para cámara libre (Ventana 3) - valores exactos de la imagen final
    CameraWindow3.Position = float3(-0.77f, 0.83f, -4.57f);
    CameraWindow3.RotX = -1.43f;
    CameraWindow3.RotY = 0.05f;
    CameraWindow3.RotZ = 0.05f;
    CameraWindow3.ViewZoom = 0.226f; // Valor exacto de la imagen
}

void Tutorial04_Instancing::UpdateUI()
{
    // Actualizar matrices de cámara basadas en parámetros actuales
    UpdateCameraMatrices();
    
    // Ventana 1: Paneo y Zoom
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Ventana 1: Paneo y Zoom", nullptr))
    {
        ImGui::Text("Arrastre con el ratón para paneo");
        ImGui::Text("Use la rueda del ratón para zoom");
        
        // Controladores de paneo
        ImGui::SliderFloat("Pan X", &CameraWindow1.PanOffset.x, -10.0f, 10.0f);
        ImGui::SliderFloat("Pan Y", &CameraWindow1.PanOffset.y, -10.0f, 10.0f);
        
        // Controlador de zoom
        ImGui::SliderFloat("Zoom", &CameraWindow1.Zoom, 0.1f, 5.0f);
        
        if (ImGui::Button("Reset Camera"))
        {
            CameraWindow1.PanOffset = float2(0.0f, 0.0f);
            CameraWindow1.Zoom = 1.0f;
        }
    }
    ImGui::End();
    
    // Ventana 2: Control Orbital
    ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Ventana 2: Control Orbital", nullptr))
    {
        ImGui::Text("Arrastre con el ratón para orbitar");
        
        // Controladores de ángulos de órbita
        ImGui::SliderFloat("Orbit X", &CameraWindow2.OrbitAngleX, -PI_F, PI_F);
        ImGui::SliderFloat("Orbit Y", &CameraWindow2.OrbitAngleY, -PI_F, PI_F);
        ImGui::SliderFloat("Distance", &CameraWindow2.OrbitDistance, 5.0f, 40.0f);
        
        if (ImGui::Button("Reset Orbit"))
        {
            CameraWindow2.OrbitAngleX = 3.0f; // Valor ajustado según la imagen donde funciona
            CameraWindow2.OrbitAngleY = 0.0f;
            CameraWindow2.OrbitDistance = 20.0f;
        }
    }
    ImGui::End();
    
    // Ventana 3: Cámara Libre
    ImGui::SetNextWindowPos(ImVec2(630, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Ventana 3: Cámara Libre", nullptr))
    {
        ImGui::Text("Control de posición de cámara:");
        
        // Controles de posición
        ImGui::Text("Posición:");
        ImGui::SliderFloat("X", &CameraWindow3.Position.x, -15.0f, 15.0f);
        ImGui::SliderFloat("Y", &CameraWindow3.Position.y, -15.0f, 15.0f);
        ImGui::SliderFloat("Z", &CameraWindow3.Position.z, -40.0f, 40.0f);
        
        ImGui::Separator();
        
        // Controles de rotación
        ImGui::Text("Rotación:");
        ImGui::SliderFloat("Rot X", &CameraWindow3.RotX, -PI_F, PI_F);
        ImGui::SliderFloat("Rot Y", &CameraWindow3.RotY, -PI_F, PI_F);
        ImGui::SliderFloat("Rot Z", &CameraWindow3.RotZ, -PI_F, PI_F);
        
        // Control de escala para el zoom
        ImGui::SliderFloat("Zoom", &CameraWindow3.ViewZoom, 0.01f, 0.5f, "%.3f");
    }
    ImGui::End();
}

void Tutorial04_Instancing::PopulateInstanceBuffer()
{
    std::vector<float4x4> InstanceData(MaxInstances);
    int instId = 0;

    // Ángulos de rotación para diferentes partes
    static float mainRotation = 0.0f;          // Rotación principal del móvil
    static float firstTierRotation = 0.0f;     // Rotación del primer nivel
    static float secondTierRotation = 0.0f;    // Rotación del segundo nivel

    // Actualizar ángulos con velocidades diferenciadas
    mainRotation += 0.003f;                    // Rotación base más lenta
    firstTierRotation += 0.005f;               // Primer nivel gira un poco más rápido
    secondTierRotation += 0.007f;              // Segundo nivel gira más rápido aún

    // Base principal (placa superior)
    float4x4 baseMatrix = float4x4::Scale(1.6f, 0.1f, 1.6f) * float4x4::Translation(0.0f, 4.8f, 0.0f);
    InstanceData[instId++] = baseMatrix;

    // Matrices de rotación para los diferentes niveles
    float4x4 mainRotMatrix = float4x4::RotationY(mainRotation);
    float4x4 firstLevelMatrix = mainRotMatrix * float4x4::RotationY(firstTierRotation);
    float4x4 secondLevelMatrix = firstLevelMatrix * float4x4::RotationY(secondTierRotation);

    // === PRIMER NIVEL ===
    // Palo central vertical
    float4x4 centerPoleMatrix = float4x4::Scale(0.1f, 1.0f, 0.1f) * float4x4::Translation(0.0f, 3.65f, 0.0f);
    InstanceData[instId++] = centerPoleMatrix;

    // Brazos horizontales del primer nivel
    float4x4 horizontalArm1 = float4x4::Scale(3.6f, 0.1f, 0.1f) *
                             float4x4::Translation(0.0f, 2.6f, 0.0f) *
                             firstLevelMatrix;
    float4x4 horizontalArm2 = float4x4::Scale(0.1f, 0.1f, 3.6f) *
                             float4x4::Translation(0.0f, 2.6f, 0.0f) *
                             firstLevelMatrix;
    InstanceData[instId++] = horizontalArm1;
    InstanceData[instId++] = horizontalArm2;

    // Cubos del primer nivel
    float4x4 cubePositions[] = {
        float4x4::Translation(3.0f, 2.0f, 0.0f),
        float4x4::Translation(-3.0f, 2.0f, 0.0f),
        float4x4::Translation(0.0f, 2.0f, 3.0f),
        float4x4::Translation(0.0f, 2.0f, -3.0f)
    };

    for (const auto& pos : cubePositions) {
        float4x4 cubeMatrix = float4x4::Scale(0.6f, 0.6f, 0.6f) * pos * firstLevelMatrix;
        InstanceData[instId++] = cubeMatrix;
    }

    // === SEGUNDO NIVEL ===
    // Palos verticales conectores (ahora giran con el segundo nivel)
    float4x4 verticalConnectors[] = {
        // Conectores centrados en los puntos de intersección de los brazos del segundo nivel
        float4x4::Scale(0.1f, 0.85f, 0.1f) * float4x4::Translation(0.0f, 0.85f, 3.0f),
        float4x4::Scale(0.1f, 0.85f, 0.1f) * float4x4::Translation(0.0f, 0.85f, -3.0f),
        float4x4::Scale(0.1f, 0.85f, 0.1f) * float4x4::Translation(3.0f, 0.85f, 0.0f),
        float4x4::Scale(0.1f, 0.85f, 0.1f) * float4x4::Translation(-3.0f, 0.85f, 0.0f)
    };

    for (const auto& connector : verticalConnectors) {
        InstanceData[instId++] = connector * secondLevelMatrix;  // Ahora usan secondLevelMatrix
    }

    // Brazos horizontales del segundo nivel
    float4x4 secondLevelArms[] = {
        float4x4::Scale(2.0f, 0.1f, 0.1f) * float4x4::Translation(0.0f, 0.2f, 3.0f),
        float4x4::Scale(2.0f, 0.1f, 0.1f) * float4x4::Translation(0.0f, 0.2f, -3.0f),
        float4x4::Scale(0.1f, 0.1f, 2.0f) * float4x4::Translation(3.0f, 0.2f, 0.0f),
        float4x4::Scale(0.1f, 0.1f, 2.0f) * float4x4::Translation(-3.0f, 0.2f, 0.0f)
    };

    for (const auto& arm : secondLevelArms) {
        InstanceData[instId++] = arm * secondLevelMatrix;
    }

    // Cubos del segundo nivel (ajustados para mantener simetría)
    float4x4 secondTierPositions[] = {
        float4x4::Translation(1.0f, -0.4f, 3.0f),
        float4x4::Translation(-1.0f, -0.4f, 3.0f),
        float4x4::Translation(1.0f, -0.4f, -3.0f),
        float4x4::Translation(-1.0f, -0.4f, -3.0f),
        float4x4::Translation(3.0f, -0.4f, 1.0f),
        float4x4::Translation(3.0f, -0.4f, -1.0f),
        float4x4::Translation(-3.0f, -0.4f, 1.0f),
        float4x4::Translation(-3.0f, -0.4f, -1.0f)
    };

    for (const auto& pos : secondTierPositions) {
        float4x4 cubeMatrix = float4x4::Scale(0.6f, 0.6f, 0.6f) * pos * secondLevelMatrix;
        InstanceData[instId++] = cubeMatrix;
    }

    // Actualizar el buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * instId);
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, InstanceData.data(),
                                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

// Actualizar parámetros del engine
void Tutorial04_Instancing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    
    UpdateUI();

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Se usa ViewWindow1 como matriz de vista predeterminada
    m_ViewProjMatrix = ViewWindow1 * SrfPreTransform * Proj;

    // Global rotation matrix
    m_RotationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 0.f) * float4x4::RotationX(-static_cast<float>(CurrTime) * 0.f);
}

// Render a frame
void Tutorial04_Instancing::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();

    PopulateInstanceBuffer();

    // Clear the back buffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Configuramos los viewports para las tres ventanas
    // Dividimos la pantalla en 3 partes horizontales
    const auto& SCDesc = m_pSwapChain->GetDesc();
    
    Viewport Viewports[3];
    // Ventana 1: Parte izquierda
    Viewports[0].TopLeftX = 0;
    Viewports[0].TopLeftY = 0;
    Viewports[0].Width = SCDesc.Width / 3;
    Viewports[0].Height = SCDesc.Height;
    Viewports[0].MinDepth = 0;
    Viewports[0].MaxDepth = 1;
    
    // Ventana 2: Parte central
    Viewports[1].TopLeftX = SCDesc.Width / 3;
    Viewports[1].TopLeftY = 0;
    Viewports[1].Width = SCDesc.Width / 3;
    Viewports[1].Height = SCDesc.Height;
    Viewports[1].MinDepth = 0;
    Viewports[1].MaxDepth = 1;
    
    // Ventana 3: Parte derecha
    Viewports[2].TopLeftX = 2 * SCDesc.Width / 3;
    Viewports[2].TopLeftY = 0;
    Viewports[2].Width = SCDesc.Width / 3;
    Viewports[2].Height = SCDesc.Height;
    Viewports[2].MinDepth = 0;
    Viewports[2].MaxDepth = 1;
    
    // Renderizamos el móvil tres veces, una vez para cada viewport con su propia cámara
    for (int viewIdx = 0; viewIdx < 3; viewIdx++)
    {
        // Establecer el viewport actual
        m_pImmediateContext->SetViewports(1, &Viewports[viewIdx], SCDesc.Width, SCDesc.Height);
        
        // Seleccionar la matriz de vista correspondiente a este viewport
        float4x4 CurrentView;
        switch(viewIdx)
        {
            case 0: CurrentView = ViewWindow1; break; // Paneo y zoom
            case 1: CurrentView = ViewWindow2; break; // Control orbital
            case 2: CurrentView = ViewWindow3; break; // Selección de objetos
        }
        
        // Calcular la matriz view-projection para este viewport
        float4x4 ViewProj = CurrentView * SrfPreTransform * Proj;
        
        // Actualizar los constantes del shader
        {
            MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants[0] = ViewProj;
            CBConstants[1] = m_RotationMatrix;
        }

        // Bind vertex, instance and index buffers
        const Uint64 offsets[] = {0, 0};
        IBuffer*     pBuffs[]  = {m_CubeVertexBuffer, m_InstanceBuffer};
        m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Set the pipeline state
        m_pImmediateContext->SetPipelineState(m_pPSO);
        // Commit shader resources
        m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType    = VT_UINT32;
        DrawAttrs.NumIndices   = 36;
        DrawAttrs.NumInstances = 24; // Número de instancias del móvil
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }
}

} // namespace Diligent
