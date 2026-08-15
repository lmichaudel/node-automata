cbuffer Camera : register(b0, space1) {
	float2 viewport_size;
	float2 view_position;
	float2 cursor_world_position;
	float zoom;
	float transition_radius;
	float height_darkening;
	float tile_size;
	float grid_world_width;
	float grid_min_pixel_width;
	float grid_strength;
	float grid_enabled;
	float grid_fade_radius;
};

struct VertexInput {
	float2 corner : TEXCOORD0;
	float2 uv : TEXCOORD1;
	float2 origin : TEXCOORD2;
	uint neighborhood : TEXCOORD3;
};

struct VertexOutput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	nointerpolation uint neighborhood : TEXCOORD1;
	nointerpolation float radius : TEXCOORD2;
	nointerpolation float height_darkening : TEXCOORD3;
	nointerpolation float tile_size : TEXCOORD4;
	float2 world_position : TEXCOORD5;
	nointerpolation float4 grid_parameters : TEXCOORD6;
	nointerpolation float3 grid_fade : TEXCOORD7;
};

VertexOutput main(VertexInput input) {
	const float2 world = input.origin + input.corner * tile_size;
	const float2 pixel = (world - view_position) * zoom;
	VertexOutput output;
	output.position = float4(
		pixel.x * (2.0 / viewport_size.x) - 1.0,
		1.0 - pixel.y * (2.0 / viewport_size.y),
		0.0,
		1.0
	);
	output.uv = input.uv;
	output.neighborhood = input.neighborhood;
	output.radius = transition_radius;
	output.height_darkening = height_darkening;
	output.tile_size = tile_size;
	output.world_position = world;
	output.grid_parameters = float4(
		grid_world_width, grid_min_pixel_width, grid_strength, grid_enabled);
	output.grid_fade = float3(cursor_world_position, grid_fade_radius);
	return output;
}
