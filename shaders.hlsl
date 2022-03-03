struct VS_INPUT {
	float2 pos : POSITION;
	float2 uv  : TEXCOORD;
	float4 col : COLOR;
};

struct PS_INPUT {
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
	float4 col : COLOR;
};


static const float PI = 3.14159265359f;


// visible to all shaders
cbuffer cbuffer0 : register(b0) {
	float width;
	float height;
	float aspect;
	float uptime;
};


PS_INPUT vs(VS_INPUT input) {
	float2 pos = input.pos;
	float2 uv = input.uv * 10.0f;

	float2 rot_dir = float2(cos(uptime), sin(uptime));
	float2x2 rot = float2x2(rot_dir, float2(rot_dir.y, -rot_dir.x));

	pos = mul(rot, pos);
	pos.x /= aspect;

	PS_INPUT output;
	output.pos = float4(pos, 0.0, 1.0f);
	output.uv = uv;
	output.col = input.col;
	return output;
}


sampler sampler0 : register(s0);
Texture2D<float4> texture0 : register(t0);

float4 ps(PS_INPUT input) : SV_TARGET {
    float4 texel = texture0.Sample(sampler0, input.uv);
    float4 col = input.col;

    col *= texel;

    return col;
}
