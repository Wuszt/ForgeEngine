#include "Core.h"
#include "Window.h"
#include "ControllableCamera.h"
#include "Transform.h"
#include "Time.h"
#include "InputClass.h"
#include <exception>
#include "SceneObject.h"
#include "External/DirectXTex/DirectXTex.h"

using namespace DirectX;

Core::Core(const HINSTANCE& hInstance, const int& ShowWnd, const int& width, const int& height)
{
    m_window = new Window(hInstance, ShowWnd, width, height, true);
    m_width = width;
    m_height = height;

    if (InitializeD3D(hInstance) != S_OK)
    {
        MessageBox(0, "Direct3D Initialization - Failed",
            "Error", MB_OK);
        throw std::exception("Direct3D Initialization - Failed");
    }

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


    if (!InitScene())
    {
        MessageBox(0, "Scene Initialization - Failed",
            "Error", MB_OK);
        throw std::exception("Scene Initialization - Failed");
    }
}

Core::~Core()
{
    delete m_window;
}

void Core::Run()
{

    while (m_window->IsAlive())
    {
        m_window->Update();
        Time::UpdateTime(false);
        InputClass::UpdateInput();
        UpdateScene();
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

    desc.Width = m_width;
    desc.Height = m_height;
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

    desc.Width = m_width;
    desc.Height = m_height;
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

bool Core::InitScene()
{
    Time::Initialize();
    InputClass::Initialize(*m_window->GetHInstance(), *m_window->GetHWND());

    m_camera = new ControllableCamera(XMFLOAT3(0.0f, 0.0f, -5.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), 0.4f * 3.14f, (float)m_width / m_height, 0.1f, 100.0f);

    HRESULT hr = D3DCompileFromFile(L"Effects.fx", 0, 0, "VS", "vs_4_0", D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_DEBUG, 0, &VS_Buffer, 0);
    hr = D3DCompileFromFile(L"Effects.fx", 0, 0, "PS", "ps_4_0", D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_DEBUG, 0, &PS_Buffer, 0);

    hr = m_d3Device->CreateVertexShader(VS_Buffer->GetBufferPointer(), VS_Buffer->GetBufferSize(), NULL, &VS);
    hr = m_d3Device->CreatePixelShader(PS_Buffer->GetBufferPointer(), PS_Buffer->GetBufferSize(), NULL, &PS);

    m_d3DeviceContext->VSSetShader(VS, 0, 0);
    m_d3DeviceContext->PSSetShader(PS, 0, 0);

    m_obj = new SceneObject(XMFLOAT3(0.0f, 0.0f, 0.0f));
    //m_obj->GetTransform()->SetRotationFromEuler(XMFLOAT3(0.0f, 45.0f, 0.0f));

    Vertex v[] =
    {
        //Front
        Vertex(-0.5f, -0.5f, -0.5f, 0.0f, 1.0f),
        Vertex(-0.5f,  0.5f, -0.5f, 0.0f, 0.0f),
        Vertex(0.5f,  0.5f, -0.5f, 1.0f, 0.0f),
        Vertex(0.5f, -0.5f, -0.5f, 1.0f, 1.0f),

        //Back
        Vertex(0.5f, -0.5f, 0.5f, 0.0f, 1.0f),
        Vertex(0.5f,  0.5f, 0.5f, 0.0f, 0.0f),
        Vertex(-0.5f,  0.5f, 0.5f, 1.0f, 0.0f),
        Vertex(-0.5f, -0.5f, 0.5f, 1.0f, 1.0f),

        //Top
        Vertex(-0.5f, 0.5f, -0.5f, 0.0f, 1.0f),
        Vertex(-0.5f,  0.5f, 0.5f, 0.0f, 0.0f),
        Vertex(0.5f,  0.5f, 0.5f, 1.0f, 0.0f),
        Vertex(0.5f, 0.5f, -0.5f, 1.0f, 1.0f),

        //Bottom
        Vertex(0.5f, -0.5f, 0.5f, 0.0f, 1.0f),
        Vertex(0.5f,  -0.5f, -0.5f, 0.0f, 0.0f),
        Vertex(-0.5f,  -0.5f, -0.5f, 1.0f, 0.0f),
        Vertex(-0.5f, -0.5f, 0.5f, 1.0f, 1.0f),

        //Left
        Vertex(-0.5f, -0.5f, 0.5f, 0.0f, 1.0f),
        Vertex(-0.5f,  0.5f, 0.5f, 0.0f, 0.0f),
        Vertex(-0.5f,  0.5f, -0.5f, 1.0f, 0.0f),
        Vertex(-0.5f, -0.5f, -0.5f, 1.0f, 1.0f),

        //Right
        Vertex(0.5f, -0.5f, -0.5f, 0.0f, 1.0f),
        Vertex(0.5f,  0.5f, -0.5f, 0.0f, 0.0f),
        Vertex(0.5f,  0.5f, 0.5f, 1.0f, 0.0f),
        Vertex(0.5f, -0.5f, 0.5f, 1.0f, 1.0f),
    };

    DWORD indices[] =
    {
        //Front
        0,1,2,
        0,2,3,

        //Back
        4,5,6,
        4,6,7,

        //Top
        8,9,10,
        8,10,11,


        //Bottom
        14,13,12,
        14,12,15,

        //Left
        16,17,18,
        16,18,19,

        //Right
        20,21,22,
        20,22,23
    };

    D3D11_BUFFER_DESC indexBufferDesc;
    ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));

    D3D11_SUBRESOURCE_DATA iinitData;
    iinitData.pSysMem = indices;

    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(DWORD) * 12 * 3; //indices amount
    m_d3Device->CreateBuffer(&indexBufferDesc, &iinitData, &IndexBuffer);
    m_d3DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    D3D11_BUFFER_DESC vertexBufferDesc;
    ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(Vertex) * 24;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vertexBufferData;

    ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
    vertexBufferData.pSysMem = v;
    hr = m_d3Device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &VertBuffer);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_d3DeviceContext->IASetVertexBuffers(0, 1, &VertBuffer, &stride, &offset);

    hr = m_d3Device->CreateInputLayout(layout, numElements, VS_Buffer->GetBufferPointer(),
        VS_Buffer->GetBufferSize(), &vertLayout);

    m_d3DeviceContext->IASetInputLayout(vertLayout);

    m_d3DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (float)m_width;
    viewport.Height = (float)m_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    m_d3DeviceContext->RSSetViewports(1, &viewport);

    D3D11_BUFFER_DESC cbbd;
    ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

    cbbd.Usage = D3D11_USAGE_DEFAULT;
    cbbd.ByteWidth = sizeof(cbPerObject);
    cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = m_d3Device->CreateBuffer(&cbbd, nullptr, &cbPerObjectBuffer);

    return true;
}

void Core::UpdateScene()
{
    m_camera->Update();
}

void Core::DrawScene()
{
    DirectX::ScratchImage si;
    DirectX::TexMetadata meta;
    LoadFromWICFile(L"tmp.jpg", WIC_FLAGS_NONE, &meta, si);
    const Image* images = si.GetImages();

    ID3D11ShaderResourceView* srv;
    CreateShaderResourceView(m_d3Device, images, 1, meta, &srv);

    float bgColor[4] = { (0.0f, 0.0f, 0.0f, 0.0f) };
    m_d3DeviceContext->ClearRenderTargetView(m_renderTargetView, bgColor);

    m_d3DeviceContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    cbPerObj.WVP = XMMatrixTranspose(m_obj->GetTransform()->GetWorldMatrix() * m_camera->GetViewMatrix() * m_camera->GetProjectionMatrix());

    m_d3DeviceContext->UpdateSubresource(cbPerObjectBuffer, 0, nullptr, &cbPerObj, 0, 0);

    m_d3DeviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

    m_d3DeviceContext->PSSetShaderResources(0, 1, &srv);

    m_d3DeviceContext->PSSetSamplers(0, 1, &samplerState);

    m_d3DeviceContext->DrawIndexed(36, 0, 0);

    m_swapChain->Present(0, 0);
}
