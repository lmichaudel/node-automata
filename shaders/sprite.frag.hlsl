Texture2D<float4> sprite_texture : register(t0, space2);
SamplerState sprite_sampler : register(s0, space2);

struct FragmentInput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
	float2 local_position : TEXCOORD1;
	float2 half_size : TEXCOORD2;
	float4 color : TEXCOORD3;
	nointerpolation float4 corner_radii : TEXCOORD4;
	nointerpolation uint kind : TEXCOORD5;
	nointerpolation float effect_range : TEXCOORD6;
	nointerpolation uint antialiased_edges : TEXCOORD7;
	nointerpolation float4 auxiliary : TEXCOORD8;
};

float median3(float3 value) {
	return max(min(value.r, value.g), min(max(value.r, value.g), value.b));
}

float msdf_screen_range(float2 uv, float pixel_range) {
	uint width;
	uint height;
	sprite_texture.GetDimensions(width, height);
	const float2 unit_range = pixel_range / float2(width, height);
	const float2 dx = ddx(uv);
	const float2 dy = ddy(uv);
	const float2 screen_texture_size = rsqrt(dx * dx + dy * dy);
	return max(0.5 * dot(unit_range, screen_texture_size), 1.0);
}

float coverage(float signed_distance, bool antialiased, float blur_radius) {
	if (blur_radius > 0.0) {
		const float antialias_width = max(fwidth(signed_distance), 0.0001);
		const float feather = max(blur_radius, antialias_width);
		return 1.0 - smoothstep(-feather, feather, signed_distance);
	}
	if (!antialiased) {
		return signed_distance <= 0.0 ? 1.0 : 0.0;
	}
	const float antialias_width = max(fwidth(signed_distance), 0.0001);
	return saturate(0.5 - signed_distance / antialias_width);
}

bool edge_is_antialiased(float2 position, float2 half_size, uint edges) {
	// Bits are top, right, bottom, left. Pick the closest axis-aligned side;
	// rounded corners naturally split between their two neighboring sides.
	const float2 distance_to_edge = half_size - abs(position);
	uint edge;
	if (distance_to_edge.x < distance_to_edge.y) {
		edge = position.x < 0.0 ? 8 : 2;
	} else {
		edge = position.y < 0.0 ? 1 : 4;
	}
	return (edges & edge) != 0;
}

float rounded_box_distance(float2 position, float2 half_size, float4 radii) {
	radii = clamp(radii, 0.0, min(half_size.x, half_size.y));
	const bool top = position.y < 0.0;
	const bool left = position.x < 0.0;
	const float radius = top ? (left ? radii.x : radii.y) : (left ? radii.w : radii.z);
	const float2 q = abs(position) - half_size + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float grid_line_coverage(float coordinate, float spacing, float world_width,
						 float minimum_pixel_width) {
	const float distance_to_line = abs(frac(coordinate / spacing + 0.5) - 0.5) * spacing;
	const float pixel_world_size = max(fwidth(coordinate), 0.0001);
	const float half_width = max(world_width * 0.5, minimum_pixel_width * pixel_world_size * 0.5);
	return 1.0 - smoothstep(half_width - pixel_world_size * 0.5,
							 half_width + pixel_world_size * 0.5, distance_to_line);
}

float grid_coverage(float2 world_position, float spacing, float world_width,
					float minimum_pixel_width) {
	return max(grid_line_coverage(world_position.x, spacing, world_width, minimum_pixel_width),
			   grid_line_coverage(world_position.y, spacing, world_width, minimum_pixel_width));
}

float4 main(FragmentInput input) : SV_Target0 {
	// 0: texture, 1: circle, 2: rounded rectangle, 3: MSDF, 4: grid,
	// 5: quarter ring, 6: line, 7: perfect 90-degree rounded line.
	if (input.kind == 0) {
		return sprite_texture.Sample(sprite_sampler, input.uv) * input.color;
	}
	if (input.kind == 3) {
		const float3 sample_value = sprite_texture.Sample(sprite_sampler, input.uv).rgb;
		const float signed_distance = median3(sample_value) - 0.5;
		const float opacity = saturate(
			signed_distance * msdf_screen_range(input.uv, input.effect_range) + 0.5
		);
		return float4(input.color.rgb, input.color.a * opacity);
	}
	if (input.kind == 4) {
		const float cell_size = input.corner_radii.x;
		const float line_width = input.corner_radii.y;
		const float minimum_pixel_width = input.corner_radii.z;
		const float supergrid_size = cell_size * input.corner_radii.w;
		const float base_coverage = grid_coverage(
			input.local_position, cell_size, line_width, minimum_pixel_width
		);
		const float super_coverage = grid_coverage(
			input.local_position, supergrid_size, line_width * 1.5,
			minimum_pixel_width * 1.5
		);
		const float3 color = lerp(input.color.rgb, input.auxiliary.rgb, super_coverage);
		const float opacity = max(base_coverage * input.color.a,
							  super_coverage * input.auxiliary.a);
		return float4(color, opacity);
	}

	float signed_distance;
	float cap_coverage = 1.0;
	if (input.kind == 1) {
		signed_distance = length(input.local_position) - min(input.half_size.x, input.half_size.y);
	} else if (input.kind == 5) {
		const float turn = input.corner_radii.y > 0.5 ? 1.0 : -1.0;
		const float2 arc_center = float2(-input.half_size.x, turn * input.half_size.y);
		const float radius = min(input.half_size.x, input.half_size.y);
		const float2 arc_position = input.local_position - arc_center;
		signed_distance = abs(length(arc_position) - radius) -
			input.corner_radii.x * 0.5;
		// Blur and antialias only the curved radial edges. The two tangent caps use
		// a binary sector mask so they remain solid where straight sections join.
		cap_coverage = arc_position.x >= 0.0 && turn * arc_position.y <= 0.0 ? 1.0 : 0.0;
	} else if (input.kind == 6) {
		// Only the long edges participate in the distance field. The binary mask keeps
		// both butt caps completely free of blur and antialiasing.
		signed_distance = abs(input.local_position.y) - input.half_size.y;
		cap_coverage = abs(input.local_position.x) <= input.half_size.x ? 1.0 : 0.0;
	} else if (input.kind == 7) {
		const float turn = input.corner_radii.z > 0.5 ? 1.0 : -1.0;
		signed_distance = abs(length(input.local_position) - input.corner_radii.x) -
			input.corner_radii.y * 0.5;
		// Canonical arc runs from the left tangent to the bottom/top tangent. This
		// binary quadrant mask makes both tangent caps perfectly hard.
		cap_coverage = input.local_position.x <= 0.0 &&
			turn * input.local_position.y >= 0.0 ? 1.0 : 0.0;
	} else {
		signed_distance = rounded_box_distance(
			input.local_position,
			input.half_size,
			input.corner_radii
		);
	}

	const bool antialiased = input.kind == 1 || input.kind == 5 || input.kind == 6 ||
		input.kind == 7 ||
		edge_is_antialiased(input.local_position, input.half_size, input.antialiased_edges);
	return float4(input.color.rgb,
		input.color.a * coverage(signed_distance, antialiased, input.effect_range) * cap_coverage);
}
