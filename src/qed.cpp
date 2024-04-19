#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <windowsx.h>
#include <fileapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#undef near
#undef far
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")
#endif // _WIN32

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244)
// #include <stb_image.h>
#pragma warning(pop)

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdint.h>
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef float float32;
typedef double float64;

#include <assert.h>
#include <stdio.h>

#include "array.h"
#include "qed.h"

#define WIDTH  1040
#define HEIGHT 860

static bool window_should_close;

IDXGISwapChain *d3d11_swap_chain = nullptr;
ID3D11Device *d3d11_device = nullptr;
ID3D11DeviceContext *d3d11_context = nullptr;
ID3D11RenderTargetView *d3d11_render_target = nullptr;

void upload_constants(ID3D11Buffer *constant_buffer, void *constants, int32 constants_size) {
    D3D11_MAPPED_SUBRESOURCE res{};
    if (d3d11_context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) == S_OK) {
        memcpy(res.pData, constants, constants_size);
        d3d11_context->Unmap(constant_buffer, 0);
    } else {
        fprintf(stderr, "Failed to map constant buffer\n");
    }
}

void update_vertex_buffer(ID3D11Buffer *vertex_buffer, void *vertex_data, int32 vertices_size) {
    D3D11_MAPPED_SUBRESOURCE res{};
    if (d3d11_context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) == S_OK) {
        memcpy(res.pData, vertex_data, vertices_size);
        d3d11_context->Unmap(vertex_buffer, 0);
    } else {
        fprintf(stderr, "Failed to map vertex buffer\n");
    }
}

ID3D11Buffer *make_vertex_buffer(void *vertex_data, int32 elem_count, int32 size, bool dynamic = false) {
    ID3D11Buffer *vertex_buffer = nullptr;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = elem_count * size;
    desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = vertex_data;
    if (d3d11_device->CreateBuffer(&desc, &data, &vertex_buffer) != S_OK) {
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
    if (d3d11_device->CreateBuffer(&desc, nullptr, &constant_buffer) != S_OK) {
        fprintf(stderr, "Failed to create constant buffer\n");
    }
    return constant_buffer;
}

char *read_entire_file(const char *file_name) {
    char *result = NULL;
    HANDLE file_handle = CreateFileA((LPCSTR)file_name, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        uint64 bytes_to_read;
        if (GetFileSizeEx(file_handle, (PLARGE_INTEGER)&bytes_to_read)) {
            assert(bytes_to_read <= UINT32_MAX);
            result = (char *)malloc(bytes_to_read + 1);
            result[bytes_to_read] = 0;
            DWORD bytes_read;
            if (ReadFile(file_handle, result, (DWORD)bytes_to_read, &bytes_read, NULL) && (DWORD)bytes_to_read ==  bytes_read) {
            } else {
                // TODO: error handling
                printf("ReadFile: error reading file, %s!\n", file_name);
            }
       } else {
            // TODO: error handling
            printf("GetFileSize: error getting size of file: %s!\n", file_name);
       }
       CloseHandle(file_handle);
    } else {
        // TODO: error handling
        printf("CreateFile: error opening file: %s!\n", file_name);
    }
    return result;
}

Shader *make_shader(const char *file_name, const char *vs_entry, const char *ps_entry, D3D11_INPUT_ELEMENT_DESC *items, int item_count) {
    Shader *shader = new Shader();
    //shader->name = copy_string(path_strip_file_name(file_name));
    printf("Compiling shader '%s'... ", file_name);

    char *source = read_entire_file(file_name);
    ID3DBlob *vertex_blob, *pixel_blob, *vertex_error_blob, *pixel_error_blob;
    UINT flags = 0;
    #if _DEBUG
    flags |= D3DCOMPILE_DEBUG;
    #endif

    bool compilation_success = true;
    if (D3DCompile(source, strlen(source), file_name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, vs_entry, "vs_5_0", flags, 0, &vertex_blob, &vertex_error_blob) != S_OK) {
        compilation_success = false;
    }
    if (D3DCompile(source, strlen(source), file_name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, ps_entry, "ps_5_0", flags, 0, &pixel_blob, &pixel_error_blob) != S_OK) {
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
        d3d11_device->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), NULL, &shader->vertex_shader);
        d3d11_device->CreatePixelShader(pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), NULL, &shader->pixel_shader);
        HRESULT hr = d3d11_device->CreateInputLayout(items, item_count, vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &shader->input_layout);
        // assert(hr == S_OK);
    }


    if (vertex_error_blob) vertex_error_blob->Release();
    if (pixel_error_blob)  pixel_error_blob->Release();
    if (vertex_blob) vertex_blob->Release();
    if (pixel_blob) pixel_blob->Release();
    return shader;
}

void win32_get_window_size(HWND hwnd, int *width, int *height) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    if (*width) *width = rect.right - rect.left;
    if (*height) *height = rect.bottom - rect.top;
    
}

LRESULT CALLBACK window_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    LRESULT result = 0;
    switch (Msg) {
    case WM_SIZE:
    {
#if 0
        IDXGISwapChain *swapchain = graphics_context->swap_chain;
        ID3D11RenderTargetView *render_target = graphics_context->render_target;
        // NOTE: Resize render target view
        if (swapchain) {
            graphics_context->d3d_context->OMSetRenderTargets(0, 0, 0);

            // Release all outstanding references to the swap chain's buffers.
            render_target->Release();

            // Preserve the existing buffer count and format.
            // Automatically choose the width and height to match the client rect for HWNDs.
            HRESULT hr = swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
                                            
            // Perform error handling here!

            // Get buffer and create a render-target-view.
            ID3D11Texture2D* buffer;
            hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**) &buffer);
            // Perform error handling here!

            hr = graphics_context->d3d_device->CreateRenderTargetView(buffer, NULL, &render_target);
            // Perform error handling here!
            buffer->Release();

            graphics_context->d3d_context->OMSetRenderTargets(1, &render_target, NULL);
        }
    #endif 
        break;
    }

    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProcA(hWnd, Msg, wParam, lParam);
    }
    return result;
}

Face *load_font_face(const char *font_name, int font_height) {
    FT_Library ft_lib;
    int err = FT_Init_FreeType(&ft_lib);
    if (err) {
        printf("Error initializing freetype library: %d\n", err);
    }

    FT_Face ft_face;
    err = FT_New_Face(ft_lib, font_name, 0, &ft_face);
    if (err == FT_Err_Unknown_File_Format) {
        printf("Format not supported\n");
    } else if (err) {
        printf("Font file could not be read\n");
    }

    err = FT_Set_Pixel_Sizes(ft_face, 0, font_height);
    if (err) {
        printf("Error setting pixel sizes of font\n");
    }

    Face *face = new Face();

    int bbox_ymax = FT_MulFix(ft_face->bbox.yMax, ft_face->size->metrics.y_scale) >> 6;
    int bbox_ymin = FT_MulFix(ft_face->bbox.yMin, ft_face->size->metrics.y_scale) >> 6;
    int height = bbox_ymax - bbox_ymin;
    float ascend = ft_face->size->metrics.ascender / 64.f;
    float descend = ft_face->size->metrics.descender / 64.f;
    float bbox_height = (float)(bbox_ymax - bbox_ymin);
    float glyph_height = (float)ft_face->size->metrics.height / 64.f;
    float glyph_width = (float)(ft_face->bbox.xMax - ft_face->bbox.xMin) / 64.f;

    int atlas_width = 1; // white pixel
    int atlas_height = 0;
    int max_bmp_height = 0;
    for (uint32 c = 32; c < 256; c++) {
        if (FT_Load_Char(ft_face, c, FT_LOAD_RENDER)) {
            printf("Error loading char %c\n", c);
            continue;
        }

        atlas_width += ft_face->glyph->bitmap.width;
        if (atlas_height < (int)ft_face->glyph->bitmap.rows) {
            atlas_height = ft_face->glyph->bitmap.rows;
        }

        int bmp_height = ft_face->glyph->bitmap.rows + ft_face->glyph->bitmap_top;
        if (max_bmp_height < bmp_height) {
            max_bmp_height = bmp_height;
        }
    }

    int atlas_x = 1; // white pixel

    // Pack glyph bitmaps
    unsigned char *bitmap = (unsigned char *)calloc(atlas_width * atlas_height + 1, 1);
    bitmap[0] = 255;
    for (uint32 c = 32; c < 256; c++) {
        if (FT_Load_Char(ft_face, c, FT_LOAD_RENDER)) {
            printf("Error loading char '%c'\n", c);
        }

        Glyph *glyph = &face->glyphs[c];
        glyph->ax = (float)(ft_face->glyph->advance.x >> 6);
        glyph->ay = (float)(ft_face->glyph->advance.y >> 6);
        glyph->bx = (float)ft_face->glyph->bitmap.width;
        glyph->by = (float)ft_face->glyph->bitmap.rows;
        glyph->bt = (float)ft_face->glyph->bitmap_top;
        glyph->bl = (float)ft_face->glyph->bitmap_left;
        glyph->to = (float)atlas_x / atlas_width;
            
        // Write glyph bitmap to atlas
        for (int y = 0; y < glyph->by; y++) {
            unsigned char *dest = bitmap + y * atlas_width + atlas_x;
            unsigned char *source = ft_face->glyph->bitmap.buffer + y * ft_face->glyph->bitmap.width;
            memcpy(dest, source, ft_face->glyph->bitmap.width);
        }

        atlas_x += ft_face->glyph->bitmap.width;
    }

    face->width = atlas_width;
    face->height = atlas_height;
    face->max_bmp_height = max_bmp_height;
    face->ascend = ascend;
    face->descend = descend;
    face->bbox_height = height;
    face->glyph_width = glyph_width;
    face->glyph_height = glyph_height;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = atlas_width;
    desc.Height = atlas_height;
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
    sr_data.SysMemPitch = atlas_width;
    sr_data.SysMemSlicePitch = 0;
    HRESULT hr = d3d11_device->CreateTexture2D(&desc, &sr_data, &font_texture);
    assert(SUCCEEDED(hr));
    assert(font_texture != nullptr);
    hr = d3d11_device->CreateShaderResourceView(font_texture, nullptr, &face->texture);
    assert(SUCCEEDED(hr));

    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_lib);
    return face;
}

int main(int argc, char **argv) {
    UINT desired_scheduler_ms = 1;
    timeBeginPeriod(desired_scheduler_ms);

#define CLASSNAME "qed_hwnd_class"
    HINSTANCE hinstance = GetModuleHandle(NULL);
    WNDCLASSA window_class{};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = CLASSNAME;
    window_class.hInstance = hinstance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    if (!RegisterClassA(&window_class)) {
        fprintf(stderr, "RegisterClassA failed, err:%d\n", GetLastError());
    }

    HWND window = 0;
    {
        RECT rc = {0, 0, WIDTH, HEIGHT};
        if (AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE)) {
            window = CreateWindowA(CLASSNAME, "Qed", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hinstance, NULL);
        } else {
            fprintf(stderr, "AdjustWindowRect failed, err:%d\n", GetLastError());
        }
        if (!window) {
            fprintf(stderr, "CreateWindowA failed, err:%d\n", GetLastError());
        }
    }

    HRESULT hr = S_OK;

    // INITIALIZE SWAP CHAIN
    DXGI_SWAP_CHAIN_DESC swapchain_desc{};
    {
        DXGI_MODE_DESC buffer_desc{};
        buffer_desc.Width = WIDTH;
        buffer_desc.Height = HEIGHT;
        buffer_desc.RefreshRate.Numerator = 60;
        buffer_desc.RefreshRate.Denominator = 1;
        buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        buffer_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        buffer_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        swapchain_desc.BufferDesc = buffer_desc;
        swapchain_desc.SampleDesc.Count = 1;
        swapchain_desc.SampleDesc.Quality = 0;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.BufferCount = 1;
        swapchain_desc.OutputWindow = window;
        swapchain_desc.Windowed = TRUE;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    }

    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, NULL, D3D11_SDK_VERSION, &swapchain_desc, &d3d11_swap_chain, &d3d11_device, NULL, &d3d11_context);

    ID3D11Texture2D *backbuffer;
    hr = d3d11_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer);

    hr = d3d11_device->CreateRenderTargetView(backbuffer, NULL, &d3d11_render_target);
    backbuffer->Release();

    ID3D11DepthStencilView *depth_stencil_view = nullptr;
    // DEPTH STENCIL BUFFER
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = WIDTH;
        desc.Height = HEIGHT;
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
        hr = d3d11_device->CreateTexture2D(&desc, NULL, &depth_stencil_buffer);
        hr = d3d11_device->CreateDepthStencilView(depth_stencil_buffer, NULL, &depth_stencil_view);
    }

    // DEPTH-STENCIL STATE
    ID3D11DepthStencilState *depth_stencil_state = nullptr;
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        d3d11_device->CreateDepthStencilState(&desc, &depth_stencil_state);
    }

    // BLEND STATE
    ID3D11BlendState *blend_state = nullptr;
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
        d3d11_device->CreateBlendState(&desc, &blend_state);
    }

    // RASTERIZER STATE
    ID3D11RasterizerState *rasterizer_state = nullptr;
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = false;
        desc.DepthClipEnable = false;
        d3d11_device->CreateRasterizerState(&desc, &rasterizer_state);
    }

    // FONT SAMPLER
    ID3D11SamplerState *font_sampler = nullptr;
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        HRESULT hr = d3d11_device->CreateSamplerState(&desc, &font_sampler);
        assert(SUCCEEDED(hr));
    }

    D3D11_INPUT_ELEMENT_DESC simple_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    Shader *simple_shader = make_shader("simple.hlsl", "vs_main", "ps_main", simple_layout, ARRAYSIZE(simple_layout));

    Face *face = load_font_face("fonts/Inconsolata.ttf", 40);
    
    const char *text = "This is some sample text.";
    Array<Vertex> vertices;

    v2 cursor = V2(0.0f, 0.0f);
    char c;
    while (c = *text++) {
        Glyph *glyph = face->glyphs + c;
        float x0 = cursor.x + glyph->bl;
        float x1 = x0 + glyph->bx;
        float y0 = cursor.y - glyph->bt + face->ascend;
        float y1 = y0 + glyph->by; 

        float tw = glyph->bx / (float)face->width;
        float th = glyph->by / (float)face->height;
        float tx = glyph->to;
        float ty = 0.0f;

        Vertex v[6];
        v[0] = { x0, y1, tx, ty + th,      V4(1, 1, 1, 1) };
        v[1] = { x0, y0, tx, ty,           V4(1, 1, 1, 1) };
        v[2] = { x1, y0, tx + tw, ty,      V4(1, 1, 1, 1) };
        v[3] = v[0];
        v[4] = v[2];
        v[5] = { x1, y1, tx + tw, ty + th, V4(1, 1, 1, 1) };

        vertices.push(v[0]);
        vertices.push(v[1]);
        vertices.push(v[2]);
        vertices.push(v[3]);
        vertices.push(v[4]);
        vertices.push(v[5]);

        cursor.x += glyph->ax;
    }

    ID3D11Buffer *vertex_buffer = make_vertex_buffer(vertices.data, (int32)vertices.count, sizeof(Vertex));
    ID3D11Buffer *simple_constant_buffer = make_constant_buffer(sizeof(Simple_Constants));

    Simple_Constants simple_constants{};

    while (!window_should_close) {
        MSG message;
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                window_should_close = true;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        int width, height;
        win32_get_window_size(window, &width, &height);
        v2 render_dim = V2((float)width, (float)height);

        m4 projection = ortho_rh_zo(0.0f, render_dim.x, render_dim.y, 0.0f, 0.0f, 1.0f);

        float bg_color[4] = {0, 0, 0, 1};
        d3d11_context->ClearRenderTargetView(d3d11_render_target, bg_color);
        d3d11_context->OMSetRenderTargets(1, &d3d11_render_target, depth_stencil_view);

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = viewport.TopLeftY = 0;
        viewport.Width = WIDTH;
        viewport.Height = HEIGHT;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        d3d11_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d11_context->IASetInputLayout(simple_shader->input_layout);

        UINT stride = sizeof(Vertex), offset = 0;
        d3d11_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);

        simple_constants.transform = projection;
        upload_constants(simple_constant_buffer, &simple_constants, sizeof(Simple_Constants));

        d3d11_context->VSSetConstantBuffers(0, 1, &simple_constant_buffer);

        d3d11_context->VSSetShader(simple_shader->vertex_shader, 0, 0);
        d3d11_context->PSSetShader(simple_shader->pixel_shader, 0, 0);

        d3d11_context->PSSetSamplers(0, 1, &font_sampler);

        d3d11_context->PSSetShaderResources(0, 1, &face->texture);
        
        d3d11_context->RSSetState(rasterizer_state);
        d3d11_context->RSSetViewports(1, &viewport);

        float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        d3d11_context->OMSetBlendState(blend_state, blend_factor, 0xffffffff); 
        d3d11_context->OMSetDepthStencilState(depth_stencil_state, 0);

        d3d11_context->Draw((UINT)vertices.count, 0);

        d3d11_swap_chain->Present(1, 0);
    }

    return 0;
}
