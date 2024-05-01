#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#undef near
#undef far
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")

#include "simple_math.h"
#include "draw.h"

#include <stdio.h>
#include <stdlib.h>

struct Shader {
    char *name;
    ID3D11InputLayout *input_layout;
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
};

struct D3D11_Context {
    int32 current_width = -1;
    int32 current_height = -1;
    bool resources_initialized = false;

    IDXGISwapChain *swap_chain = nullptr;
    ID3D11DeviceContext *device_context = nullptr;
    ID3D11Device *device = nullptr;
    ID3D11RenderTargetView *render_target_view = nullptr;
    ID3D11RasterizerState *rasterizer_state = nullptr;
    ID3D11BlendState *blend_state = nullptr;
    ID3D11DepthStencilView *depth_stencil_view = nullptr;
    ID3D11DepthStencilState *depth_stencil_state = nullptr;
    ID3D11SamplerState *sampler;
};

struct Simple_Constants {
    m4 transform;
};

D3D11_Context *d3d11_ctx = new D3D11_Context();
Shader *simple_shader;
ID3D11Buffer *simple_constant_buffer = nullptr;

void d3d11_initialize_devices(uint32 width, uint32 height, HWND window_handle) {
    DXGI_MODE_DESC buffer_desc{};
    buffer_desc.Width = width;
    buffer_desc.Height = height;
    buffer_desc.RefreshRate.Numerator = 60;
    buffer_desc.RefreshRate.Denominator = 1;
    buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    buffer_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    buffer_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    DXGI_SWAP_CHAIN_DESC swapchain_desc{};
    swapchain_desc.BufferDesc = buffer_desc;
    swapchain_desc.SampleDesc.Count = 1;
    swapchain_desc.SampleDesc.Quality = 0;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = 1;
    swapchain_desc.OutputWindow = window_handle;
    swapchain_desc.Windowed = TRUE;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, NULL, D3D11_SDK_VERSION, &swapchain_desc, &d3d11_ctx->swap_chain, &d3d11_ctx->device, NULL, &d3d11_ctx->device_context);
    assert(SUCCEEDED(hr));
}

void *d3d11_create_face_texture(uint8 *bitmap, int width, int height) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    ID3D11Texture2D *font_texture = nullptr;
    D3D11_SUBRESOURCE_DATA sr_data{};
    sr_data.pSysMem = bitmap;
    sr_data.SysMemPitch = width;
    sr_data.SysMemSlicePitch = 0;
    HRESULT hr = d3d11_ctx->device->CreateTexture2D(&desc, &sr_data, &font_texture);
    assert(SUCCEEDED(hr));
    assert(font_texture != nullptr);
    ID3D11ShaderResourceView *shader_resource_view = nullptr;
    hr = d3d11_ctx->device->CreateShaderResourceView(font_texture, nullptr, &shader_resource_view);
    assert(SUCCEEDED(hr));
    return shader_resource_view;
}

void resize_render_target_view(UINT width, UINT height) {
    // NOTE: Resize render target view
    d3d11_ctx->device_context->OMSetRenderTargets(0, 0, 0);

    // Release all outstanding references to the swap chain's buffers.
    if (d3d11_ctx->render_target_view) d3d11_ctx->render_target_view->Release();

    // Preserve the existing buffer count and format.
    // Automatically choose the width and height to match the client rect for HWNDs.
    HRESULT hr = d3d11_ctx->swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    // Get buffer and create a render-target-view.
    ID3D11Texture2D *backbuffer;
    hr = d3d11_ctx->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer);

    hr = d3d11_ctx->device->CreateRenderTargetView(backbuffer, NULL, &d3d11_ctx->render_target_view);

    backbuffer->Release();

    if (d3d11_ctx->depth_stencil_view) d3d11_ctx->depth_stencil_view->Release();

    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        ID3D11Texture2D *depth_stencil_buffer = nullptr;
        hr = d3d11_ctx->device->CreateTexture2D(&desc, NULL, &depth_stencil_buffer);
        hr = d3d11_ctx->device->CreateDepthStencilView(depth_stencil_buffer, NULL, &d3d11_ctx->depth_stencil_view);
    }

    d3d11_ctx->device_context->OMSetRenderTargets(1, &d3d11_ctx->render_target_view, d3d11_ctx->depth_stencil_view);
}

void upload_constants(ID3D11Buffer *constant_buffer, void *constants, int32 constants_size) {
    D3D11_MAPPED_SUBRESOURCE res{};
    if (d3d11_ctx->device_context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) == S_OK) {
        memcpy(res.pData, constants, constants_size);
        d3d11_ctx->device_context->Unmap(constant_buffer, 0);
    } else {
        fprintf(stderr, "Failed to map constant buffer\n");
    }
}

void update_vertex_buffer(ID3D11Buffer *vertex_buffer, void *vertex_data, int32 vertices_size) {
    D3D11_MAPPED_SUBRESOURCE res{};
    if (d3d11_ctx->device_context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) == S_OK) {
        memcpy(res.pData, vertex_data, vertices_size);
        d3d11_ctx->device_context->Unmap(vertex_buffer, 0);
    } else {
        fprintf(stderr, "Failed to map vertex buffer\n");
    }
}

ID3D11Buffer *make_vertex_buffer(void *vertex_data, int32 count, int32 size) {
    ID3D11Buffer *vertex_buffer = nullptr;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = count * size;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = vertex_data;
    if (d3d11_ctx->device->CreateBuffer(&desc, &data, &vertex_buffer) != S_OK) {
        fprintf(stderr, "Failed to create vertex buffer\n");
    }
    return vertex_buffer;
}

ID3D11Buffer *make_constant_buffer(unsigned int size) {
    ID3D11Buffer *constant_buffer = nullptr;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = size;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (d3d11_ctx->device->CreateBuffer(&desc, nullptr, &constant_buffer) != S_OK) {
        fprintf(stderr, "Failed to create constant buffer\n");
    }
    return constant_buffer;
}

Shader *make_shader(const char *shader_name, const char *source, const char *vs_entry, const char *ps_entry, D3D11_INPUT_ELEMENT_DESC *items, int item_count) {
    printf("Compiling shader '%s'... ", shader_name);
    Shader *shader = new Shader();
    ID3DBlob *vertex_blob, *pixel_blob, *vertex_error_blob, *pixel_error_blob;
    UINT flags = 0;
    #if _DEBUG
    flags |= D3DCOMPILE_DEBUG;
    #endif

    bool compilation_success = true;
    if (D3DCompile(source, strlen(source), shader_name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, vs_entry, "vs_5_0", flags, 0, &vertex_blob, &vertex_error_blob) != S_OK) {
        compilation_success = false;
    }
    if (D3DCompile(source, strlen(source), shader_name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, ps_entry, "ps_5_0", flags, 0, &pixel_blob, &pixel_error_blob) != S_OK) {
        compilation_success = false;
    }

    if (compilation_success) {
        printf("SUCCESS\n");
    } else {
        printf("FAILED\n");
        if (vertex_error_blob) printf("%s", (char *)vertex_error_blob->GetBufferPointer());
        // Errors are the same since compiling from the same source file, might be useful if we used different source files for the pixel and vertex shaders
        // if (pixel_error_blob) log_info("%s", (char *)pixel_error_blob->GetBufferPointer());
    }

    if (compilation_success) {
        d3d11_ctx->device->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), NULL, &shader->vertex_shader);
        d3d11_ctx->device->CreatePixelShader(pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), NULL, &shader->pixel_shader);
        HRESULT hr = d3d11_ctx->device->CreateInputLayout(items, item_count, vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &shader->input_layout);
        // assert(hr == S_OK);
    }

    if (vertex_error_blob) vertex_error_blob->Release();
    if (pixel_error_blob)  pixel_error_blob->Release();
    if (vertex_blob) vertex_blob->Release();
    if (pixel_blob) pixel_blob->Release();

    return shader;
}

Shader *make_shader_from_file(const char *file_name, const char *vs_entry, const char *ps_entry, D3D11_INPUT_ELEMENT_DESC *items, int item_count) {
    String source = read_file_string(file_name);
    Shader *shader = make_shader(file_name, source.data, vs_entry, ps_entry, items, item_count);
    //shader->name = copy_string(path_strip_file_name(file_name));
    return shader;
}

void d3d11_render(Render_Target *target) {
    if (!d3d11_ctx->resources_initialized) {
        d3d11_ctx->resources_initialized = true;

        HRESULT hr = S_OK;
        ID3D11Texture2D *backbuffer;
        hr = d3d11_ctx->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer);

        hr = d3d11_ctx->device->CreateRenderTargetView(backbuffer, NULL, &d3d11_ctx->render_target_view);
        backbuffer->Release();

        {
            D3D11_DEPTH_STENCIL_DESC desc{};
            desc.DepthEnable = false;
            desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
            desc.StencilEnable = false;
            desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
            desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
            desc.BackFace = desc.FrontFace;
            d3d11_ctx->device->CreateDepthStencilState(&desc, &d3d11_ctx->depth_stencil_state);
        }

        {
            D3D11_BLEND_DESC desc{};
            desc.AlphaToCoverageEnable = false;
            desc.RenderTarget[0].BlendEnable = true;
            desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            d3d11_ctx->device->CreateBlendState(&desc, &d3d11_ctx->blend_state);
        }

        {
            D3D11_RASTERIZER_DESC desc{};
            desc.FillMode = D3D11_FILL_SOLID;
            desc.CullMode = D3D11_CULL_NONE;
            desc.ScissorEnable = false;
            desc.DepthClipEnable = false;
            d3d11_ctx->device->CreateRasterizerState(&desc, &d3d11_ctx->rasterizer_state);
        }

        {
            D3D11_SAMPLER_DESC desc{};
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
            hr = d3d11_ctx->device->CreateSamplerState(&desc, &d3d11_ctx->sampler);
            assert(SUCCEEDED(hr));
        }

        D3D11_INPUT_ELEMENT_DESC simple_layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        simple_shader = make_shader_from_file("simple.hlsl", "vs_main", "ps_main", simple_layout, ARRAYSIZE(simple_layout));

        simple_constant_buffer = make_constant_buffer(sizeof(Simple_Constants));
    }

    if (target->width != d3d11_ctx->current_width || target->height != d3d11_ctx->current_height) {
        resize_render_target_view(target->width, target->height);
        d3d11_ctx->current_width = target->width;
        d3d11_ctx->current_height = target->height;
    }

    v2 render_dim = V2((float)target->width, (float)target->height);
    m4 projection = ortho_rh_zo(0.0f, render_dim.x, render_dim.y, 0.0f, 0.0f, 1.0f);

    Simple_Constants simple_constants{};
    simple_constants.transform = projection;
    upload_constants(simple_constant_buffer, &simple_constants, sizeof(Simple_Constants));

    d3d11_ctx->device_context->VSSetConstantBuffers(0, 1, &simple_constant_buffer);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = viewport.TopLeftY = 0;
    viewport.Width = render_dim.x;
    viewport.Height = render_dim.y;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d11_ctx->device_context->RSSetState(d3d11_ctx->rasterizer_state);
    d3d11_ctx->device_context->RSSetViewports(1, &viewport);

    d3d11_ctx->device_context->VSSetShader(simple_shader->vertex_shader, 0, 0);
    d3d11_ctx->device_context->PSSetShader(simple_shader->pixel_shader, 0, 0);

    float bg_color[4] = {0, 0, 0, 1};
    d3d11_ctx->device_context->ClearRenderTargetView(d3d11_ctx->render_target_view, bg_color);

    float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3d11_ctx->device_context->OMSetBlendState(d3d11_ctx->blend_state, blend_factor, 0xffffffff); 
    d3d11_ctx->device_context->OMSetDepthStencilState(d3d11_ctx->depth_stencil_state, 0);

    for (int i = 0; i < target->groups.count; i++) {
        Render_Group *group = &target->groups[i];
        d3d11_ctx->device_context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView **)&group->texture);
        d3d11_ctx->device_context->PSSetSamplers(0, 1, &d3d11_ctx->sampler);

        ID3D11Buffer *vertex_buffer = make_vertex_buffer(group->vertices.data, (int)group->vertices.count, sizeof(Vertex));
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        d3d11_ctx->device_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);

        d3d11_ctx->device_context->IASetInputLayout(simple_shader->input_layout);
        d3d11_ctx->device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3d11_ctx->device_context->Draw((UINT)group->vertices.count, 0);

        if (vertex_buffer) vertex_buffer->Release();
    }

    d3d11_ctx->swap_chain->Present(1, 0);
}
