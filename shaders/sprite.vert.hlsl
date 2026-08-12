cbuffer Camera : register(b0, space1) {
	float2 viewport_size;
	float2 view_position;
	float zoom;
	float3 _camera_padding;
};

struct VertexInput {
	float2 corner : TEXCOORD0;
	float2 uv : TEXCOORD1;
	float2 origin : TEXCOORD2;
	float2 size : TEXCOORD3;
	float4 uv_rect : TEXCOORD4;
	float4 color : TEXCOORD5;
	float4 parameters : TEXCOORD6;
	float4 corner_radii : TEXCOORD7;
};

struct VertexOutput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float2 local_position : TEXCOORD1;
	float2 half_size : TEXCOORD2;
	float4 color : TEXCOORD3;
	nointerpolation float4 corner_radii : TEXCOORD4;
	nointerpolation uint kind : TEXCOORD5;
	nointerpolation float msdf_range : TEXCOORD6;
	nointerpolation uint antialiased_edges : TEXCOORD7;
	nointerpolation float4 auxiliary : TEXCOORD8;
};

VertexOutput main(VertexInput input) {
	const uint kind = (uint)(input.parameters.z + 0.5);
	// SDF geometry extends beyond its mathematical boundary so the fragment
	// shader has enough coverage to antialias the outside edge.
	const bool procedural = kind == 1 || kind == 2;
	const float2 raster_size = input.size + (procedural ? 3.0 / zoom : 0.0);
	const float sine = sin(input.parameters.x);
	const float cosine = cos(input.parameters.x);
	const float2 local = input.corner * raster_size;
	const float2 rotated = float2(
		local.x * cosine - local.y * sine,
		local.x * sine + local.y * cosine
	);
	const float2 center = input.origin + input.size * 0.5;
	float2 pixel_position = (center + rotated - view_position) * zoom;
	float2 local_position = local;
	if (kind == 4) {
		pixel_position = (input.corner + 0.5) * viewport_size;
		local_position = view_position + pixel_position / zoom;
	}

	VertexOutput output;
	output.position = float4(
		pixel_position.x * (2.0 / viewport_size.x) - 1.0,
		1.0 - pixel_position.y * (2.0 / viewport_size.y),
		0.0,
		1.0
	);
	output.uv = lerp(input.uv_rect.xy, input.uv_rect.zw, input.uv);
	output.local_position = local_position;
	output.half_size = input.size * 0.5;
	output.color = input.color;
	output.corner_radii = input.corner_radii;
	output.kind = kind;
	output.msdf_range = input.parameters.w;
	output.antialiased_edges = (uint)(input.parameters.y + 0.5);
	output.auxiliary = input.uv_rect;
	return output;
}
