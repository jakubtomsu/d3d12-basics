
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


static const float PI      = 3.14159265359f;


// visible to all shaders
cbuffer cbuffer0 : register(b0) {
	float width;
	float height;
	float aspect;
	float uptime;
};


PS_INPUT vs(VS_INPUT input) {
	float2 pos = input.pos;
	pos.x *= aspect;
	float2 uv = input.uv;

	float angle = fmod(uptime / 21.0f, 1.0f) * PI*2.0f;
	float2x2 rot = {
		aspect*cos(angle), aspect*sin(angle),
		      -sin(angle),        cos(angle),
	};

	//pos = mul(rot, pos);

	PS_INPUT output;
	output.pos = float4(pos, 0.0, 1.0f);
	output.uv = uv;
	output.col = input.col;
	return output;
}


sampler sampler0 : register(s0);
Texture2D<float4> texture0 : register(t0);

float4 ps(PS_INPUT input) : SV_TARGET {
	float4 texel = texture0.Sample(sampler0, input.uv * 5.0f);
	float4 col = input.col;

	col *= texel;
	//col = float4(floor(input.uv*10.0f)/10.0f, 0.0f, 1.0f);
	//col = input.pos;

	return col;
}