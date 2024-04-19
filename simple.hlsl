
cbuffer Constant_Buffer : register(b0) {
    matrix transform;
};

struct VS_IN {
    float2 pos_l : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct VS_OUT {
    float4 pos_h : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

Texture2D diffuse_tex : register(t0);
sampler diffuse_sampler : register(s0);

VS_OUT vs_main(VS_IN vin) {
    VS_OUT vout;
    vout.pos_h = mul(transform, float4(vin.pos_l, 0.0, 1));
    vout.color = vin.color;
    vout.uv = vin.uv;
    return vout;
}

float4 ps_main(VS_OUT pin) : SV_TARGET {
    float4 diffuse = diffuse_tex.Sample(diffuse_sampler, pin.uv);
    return diffuse.r * pin.color;
}
