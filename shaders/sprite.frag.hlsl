Texture2D<float4> sprite_texture : register(t0, space2);
SamplerState sprite_sampler : register(s0, space2);

struct FragmentInput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float2 local_position : TEXCOORD1;
	float2 half_size : TEXCOORD2;
	float4 color : TEXCOORD3;
	float corner_radius : TEXCOORD4;
	nointerpolation uint kind : TEXCOORD5;
};

float coverage(float signed_distance) {
	const float antialias_width = max(fwidth(signed_distance), 0.75);
	return saturate(0.5 - signed_distance / antialias_width);
}

float rounded_box_distance(float2 position, float2 half_size, float radius) {
	radius = clamp(radius, 0.0, min(half_size.x, half_size.y));
	const float2 q = abs(position) - half_size + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float4 main(FragmentInput input) : SV_Target0 {
	// 0: textured sprite, 1: circle, 2: rounded rectangle.
	if (input.kind == 0) {
		return sprite_texture.Sample(sprite_sampler, input.uv) * input.color;
	}

	float signed_distance;
	if (input.kind == 1) {
		signed_distance = length(input.local_position) - min(input.half_size.x, input.half_size.y);
	} else {
		signed_distance = rounded_box_distance(
			input.local_position,
			input.half_size,
			input.corner_radius
		);
	}

	return float4(input.color.rgb, input.color.a * coverage(signed_distance));
}
