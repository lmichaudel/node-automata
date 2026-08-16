Texture2D<float4> shape_texture : register(t0, space2);
SamplerState shape_sampler : register(s0, space2);

struct FragmentInput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float2 local_position : TEXCOORD1;
	nointerpolation float2 half_size : TEXCOORD2;
	nointerpolation float4 color : TEXCOORD3;
	nointerpolation float4 parameters : TEXCOORD4;
	nointerpolation float4 auxiliary : TEXCOORD5;
};

float hex_distance(float2 position, float radius) {
	// Pointy-top regular hex expressed as the intersection of its six unit-normal
	// half planes. `radius` is the center-to-tip circumradius used by the axial
	// world conversion, so geometry and picking share exactly the same convention.
	position = abs(position);
	const float apothem = 0.8660254 * radius;
	const float vertical_edge = position.x - apothem;
	const float diagonal_edge = dot(position, float2(0.5, 0.8660254)) - apothem;
	return max(vertical_edge, diagonal_edge);
}

float rounded_box_distance(float2 position, float2 half_size, float radius) {
	radius = min(radius, min(half_size.x, half_size.y));
	const float2 q = abs(position) - half_size + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float capsule_distance(float2 position, float half_length, float radius) {
	const float2 closest = float2(clamp(position.x, -half_length, half_length), 0.0);
	return length(position - closest) - radius;
}

float4 main(FragmentInput input) : SV_Target0 {
	const uint kind = (uint)(input.parameters.x + 0.5);
	if (kind == 0)
		return shape_texture.Sample(shape_sampler, input.uv) * input.color;

	float distance_field = 0.0;
	if (kind == 1)
		distance_field = hex_distance(input.local_position, input.half_size.y);
	else if (kind == 2)
		distance_field = length(input.local_position) - input.half_size.x;
	else if (kind == 3)
		distance_field = capsule_distance(input.local_position, input.half_size.x, input.half_size.y);
	else
		distance_field = rounded_box_distance(input.local_position, input.half_size,
			input.parameters.w);

	const float antialias = max(fwidth(distance_field), 0.001);
	const float outer = saturate(0.5 - distance_field / antialias);
	const float border_width = input.parameters.z;
	float4 color = input.color;
	if (border_width > 0.0) {
		const float inner = saturate(0.5 - (distance_field + border_width) / antialias);
		color = lerp(input.auxiliary, input.color, inner);
	}
	color.a *= outer;
	return color;
}
