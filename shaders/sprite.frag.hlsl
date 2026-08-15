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
	float2 world_position : TEXCOORD9;
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

float distance_to_segment(float2 sample_position, float2 start, float2 end) {
	const float2 segment = end - start;
	const float along = saturate(dot(sample_position - start, segment) /
		max(dot(segment, segment), 0.0001));
	return length(sample_position - (start + segment * along));
}

float4 main(FragmentInput input) : SV_Target0 {
	// 0: texture, 1: circle, 2: rounded rectangle, 3: MSDF, 4: grid,
	// 5: quarter ring, 6: line, 7: perfect 90-degree rounded line,
	// 8: animated water tile, 9: land tile, 10: connected animated river.
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
	if (input.kind == 8) {
		const float time = input.corner_radii.x;
		const float2 world = input.world_position;
		const float wave_a = sin(world.x * 0.045 + world.y * 0.022 + time * 0.85);
		const float wave_b = sin(world.x * -0.018 + world.y * 0.067 - time * 0.62);
		const float broad_wave = wave_a * 0.55 + wave_b * 0.45;
		const float ripple = smoothstep(0.78, 0.96,
			0.5 + 0.5 * sin(world.x * 0.095 - world.y * 0.052 + time * 1.15 + wave_b));

		const float2 edge_distance = input.half_size - abs(input.local_position);
		float coast_distance = 10000.0;
		coast_distance = min(coast_distance,
			input.auxiliary.x > 0.5 ? input.local_position.y + input.half_size.y : 10000.0);
		coast_distance = min(coast_distance,
			input.auxiliary.y > 0.5 ? edge_distance.x : 10000.0);
		coast_distance = min(coast_distance,
			input.auxiliary.z > 0.5 ? edge_distance.y : 10000.0);
		coast_distance = min(coast_distance,
			input.auxiliary.w > 0.5 ? input.local_position.x + input.half_size.x : 10000.0);
		const float shore = 1.0 - smoothstep(1.0, 6.5, coast_distance);
		const float foam = shore * smoothstep(0.50, 0.95,
			0.5 + 0.5 * sin(world.x * 0.24 + world.y * 0.19 - time * 1.45));

		float3 color = input.color.rgb;
		color *= 0.96 + broad_wave * 0.035;
		color = lerp(color, float3(0.57, 0.79, 0.80), ripple * 0.13);
		color = lerp(color, float3(0.76, 0.88, 0.82), shore * 0.20 + foam * 0.18);
		return float4(color, input.color.a);
	}
	if (input.kind == 9) {
		const float edge_distance = min(input.half_size.x - abs(input.local_position.x),
			input.half_size.y - abs(input.local_position.y));
		const float pixel_world_size = max(fwidth(edge_distance), 0.0001);
		const float grid = 1.0 - smoothstep(0.45 - pixel_world_size, 0.45 + pixel_world_size,
			edge_distance);
		const float variation = 0.012 * sin(input.world_position.x * 0.071 +
			input.world_position.y * 0.047);
		const float visible_grid = grid * input.corner_radii.x;
		return float4(input.color.rgb * (1.0 + variation - visible_grid * 0.09), input.color.a);
	}
	if (input.kind == 10) {
		const float2 position = input.local_position / input.half_size;
		float river_distance = 1.0;
		river_distance = min(river_distance,
			input.auxiliary.x > 0.5 ? distance_to_segment(position, 0.0, float2(0.0, -1.0)) : 1.0);
		river_distance = min(river_distance,
			input.auxiliary.y > 0.5 ? distance_to_segment(position, 0.0, float2(1.0, 0.0)) : 1.0);
		river_distance = min(river_distance,
			input.auxiliary.z > 0.5 ? distance_to_segment(position, 0.0, float2(0.0, 1.0)) : 1.0);
		river_distance = min(river_distance,
			input.auxiliary.w > 0.5 ? distance_to_segment(position, 0.0, float2(-1.0, 0.0)) : 1.0);

		const float antialias_width = max(fwidth(river_distance), 0.002);
		const float bank = 1.0 - smoothstep(0.34 - antialias_width, 0.34 + antialias_width,
			river_distance);
		const float water = 1.0 - smoothstep(0.27 - antialias_width, 0.27 + antialias_width,
			river_distance);
		const float flow = smoothstep(0.72, 0.98, 0.5 + 0.5 * sin(
			dot(input.world_position, float2(0.095, 0.135)) - input.corner_radii.x * 2.4));
		const float3 bank_color = float3(0.30, 0.42, 0.34);
		float3 water_color = input.color.rgb * (0.96 + flow * 0.10);
		water_color = lerp(water_color, float3(0.67, 0.84, 0.82), flow * 0.22);
		return float4(lerp(bank_color, water_color, water), bank * input.color.a);
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
