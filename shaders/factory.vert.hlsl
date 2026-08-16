cbuffer Camera : register(b0, space1) {
	float2 viewport_size;
	float2 view_position;
	float zoom;
	float3 padding;
};

struct VertexInput {
	float2 corner : TEXCOORD0;
	float2 uv : TEXCOORD1;
	float2 center : TEXCOORD2;
	float2 size : TEXCOORD3;
	float4 color : TEXCOORD4;
	float4 parameters : TEXCOORD5;
	float4 auxiliary : TEXCOORD6;
};

struct VertexOutput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float2 local_position : TEXCOORD1;
	nointerpolation float2 half_size : TEXCOORD2;
	nointerpolation float4 color : TEXCOORD3;
	nointerpolation float4 parameters : TEXCOORD4;
	nointerpolation float4 auxiliary : TEXCOORD5;
};

VertexOutput main(VertexInput input) {
	const uint kind = (uint)(input.parameters.x + 0.5);
	float2 raster_size = input.size;
	if (kind != 0) {
		raster_size += 5.0 / zoom;
		if (kind == 3)
			raster_size.x += input.size.y;
	}
	const float2 local = input.corner * raster_size;
	const float sine = sin(input.parameters.y);
	const float cosine = cos(input.parameters.y);
	const float2 rotated = float2(local.x * cosine - local.y * sine,
		local.x * sine + local.y * cosine);
	const float2 pixel = (input.center + rotated - view_position) * zoom;
	VertexOutput output;
	output.position = float4(pixel.x * 2.0 / viewport_size.x - 1.0,
		1.0 - pixel.y * 2.0 / viewport_size.y, 0.0, 1.0);
	output.uv = input.uv;
	output.local_position = local;
	output.half_size = input.size * 0.5;
	output.color = input.color;
	output.parameters = input.parameters;
	output.auxiliary = input.auxiliary;
	return output;
}
