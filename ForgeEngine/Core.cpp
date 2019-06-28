#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Core.h"
#include "Window.h"
#include "ControllableCamera.h"
#include "Transform.h"
#include <DirectXCommonClasses/Time.h>
#include <DirectXCommonClasses/InputClass.h>
#include <exception>
#include "Object.h"
#include <DirectXTex/DirectXTex.h>

#include "RenderingSystem.h"
#include "MeshRenderer.h"
#include "ShadersManager.h"

using namespace DirectX;

Core* Core::s_instance;

Core::Core()
{
    s_instance = this;
}

Core::~Core()
{
    for (Object* const& obj : m_objects)
        delete obj;

    for (Object* const& obj : m_objectsToAdd)
        delete obj;

    for (Object* const& obj : m_objectsToDelete)
        delete obj;

    m_swapChain->Release();
    m_d3Device->Release();
    m_d3DeviceContext->Release();
    m_renderTargetView->Release();
    m_depthStencilView->Release();

    delete m_renderingSystem;
    delete m_window;

    samplerState->Release();

    InputClass::Release();
}

void Core::Run(const HINSTANCE& hInstance, const int& ShowWnd, const int& width, const int& height)
{
    Initialize(hInstance, ShowWnd, width, height);

    while (m_window->IsAlive())
    {
        m_window->Update();
        Time::UpdateTime(false);
        InputClass::UpdateInput();

        BeforeUpdateScene();
        UpdateScene();
        AfterUpdateScene();

        DeletePendingObjects();
        AddPendingObjects();

        DrawScene();
    }
}

HRESULT Core::InitializeSwapChain()
{
    DXGI_MODE_DESC bufferDesc;
    FillSwapChainBufferDescWithDefaultValues(bufferDesc);

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    FillSwapChainDescWithDefaultValues(swapChainDesc);
    swapChainDesc.BufferDesc = bufferDesc;

    return D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL, D3D11_SDK_VERSION, &swapChainDesc, &m_swapChain, &m_d3Device, NULL, &m_d3DeviceContext);
}

HRESULT Core::InitializeDepthStencilBuffer()
{
    D3D11_TEXTURE2D_DESC depthStencilDesc;
    FillDepthStencilDescWithDefaultValues(depthStencilDesc);

    HRESULT hr = m_d3Device->CreateTexture2D(&depthStencilDesc, nullptr, &m_depthStencilBuffer);

    if (hr != S_OK)
        return hr;

    hr = m_d3Device->CreateDepthStencilView(m_depthStencilBuffer, nullptr, &m_depthStencilView);

    if (hr == S_OK)
        m_d3DeviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);

    return hr;
}

void Core::Initialize(const HINSTANCE& hInstance, const int& ShowWnd, const int& width, const int& height)
{
    m_window = new Window(hInstance, ShowWnd, width, height, true);

    if (InitializeD3D(hInstance) != S_OK)
    {
        MessageBox(0, "Direct3D Initialization - Failed",
            "Error", MB_OK);
        throw std::exception("Direct3D Initialization - Failed");
    }

    m_renderingSystem = new RenderingSystem(m_d3Device, m_d3DeviceContext);

    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (S_OK != m_d3Device->CreateSamplerState(&sampDesc, &samplerState))
        throw std::exception("error");


    Time::Initialize();
    InputClass::Initialize(*m_window->GetHInstance(), *m_window->GetHWND());

    m_d3DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (float)width;
    viewport.Height = (float)height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    m_d3DeviceContext->RSSetViewports(1, &viewport);

    InitScene();
}

HRESULT Core::InitializeD3D(const HINSTANCE& hInstance)
{
    InitializeSwapChain();
    ID3D11Texture2D* BackBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);

    if (hr != S_OK)
        return hr;

    hr = m_d3Device->CreateRenderTargetView(BackBuffer, NULL, &m_renderTargetView);
    BackBuffer->Release();

    hr = InitializeDepthStencilBuffer();

    return hr;
}

void Core::FillDepthStencilDescWithDefaultValues(D3D11_TEXTURE2D_DESC& desc)
{
    ZeroMemory(&desc, sizeof(desc));

    desc.Width = m_window->GetWidth();
    desc.Height = m_window->GetHeight();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
}

void Core::FillSwapChainBufferDescWithDefaultValues(DXGI_MODE_DESC& desc)
{
    ZeroMemory(&desc, sizeof(DXGI_MODE_DESC));

    desc.Width = m_window->GetWidth();
    desc.Height = m_window->GetHeight();
    desc.RefreshRate.Numerator = 60;
    desc.RefreshRate.Denominator = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
}

void Core::FillSwapChainDescWithDefaultValues(DXGI_SWAP_CHAIN_DESC& desc)
{
    ZeroMemory(&desc, sizeof(DXGI_SWAP_CHAIN_DESC));

    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = *(m_window->GetHWND());
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
}

void Core::AddPendingObjects()
{
    for (Object* const& object : m_objectsToAdd)
    {
        bool success = m_objects.insert(object).second;
        assert(success);
    }

    m_objectsToAdd.clear();
}

void Core::DeletePendingObjects()
{
    for (Object* const& object : m_objectsToDelete)
    {
        size_t removedElements = m_objects.erase(object);

        assert(removedElements == 1);

        delete object;
    }

    m_objectsToDelete.clear();
}

void Core::InitScene()
{
    m_camera = InstantiateObject<ControllableCamera>(0.4f * 3.14f, (float)m_window->GetWidth() / m_window->GetHeight(), 0.1f, 100.0f);
    m_camera->GetTransform()->SetPosition({ 0.0f, 0.0f, -5.0f });
    m_camera->GetTransform()->LookAt(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

void Core::BeforeUpdateScene()
{


}

void Core::UpdateScene()
{
    for (Object* obj : m_objects)
    {
        obj->Update();
    }
}

void Core::AfterUpdateScene()
{
}

void Core::DrawScene()
{
    float bgColor[4] = { (0.0f, 0.0f, 0.0f, 0.0f) };
    m_d3DeviceContext->ClearRenderTargetView(m_renderTargetView, bgColor);

    m_d3DeviceContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_d3DeviceContext->PSSetSamplers(0, 1, &samplerState);

    m_renderingSystem->RenderRegisteredMeshRenderers(m_camera);

    m_swapChain->Present(0, 0);
}

void Core::DestroyObject(Object* const& obj)
{
    s_instance->m_objectsToDelete.push_back(obj);
}
