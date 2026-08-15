struct FragmentInput {
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

static const uint WATER = 0u;
static const uint LAND = 1u;
static const uint MOUNTAIN = 2u;
static const uint ROAD = 3u;
static const uint CELL_BITS = 5u;
static const uint CELL_MASK = 0x1Fu;

// Packed slots are center, top, right, bottom, left.
uint cell_data(uint neighborhood, uint slot) {
	return (neighborhood >> (slot * CELL_BITS)) & CELL_MASK;
}

uint cell_kind(uint cell) {
	return cell & 0x3u;
}

uint cell_elevation(uint cell) {
	return (cell >> 2u) & 0x7u;
}

bool material_occupied(uint cell, uint material) {
	const uint kind = cell_kind(cell);
	return material == LAND ? kind != WATER : kind == material;
}

bool elevation_occupied(uint cell, uint elevation) {
	return cell_kind(cell) != WATER && cell_elevation(cell) >= elevation;
}

float corner_inside(float2 sample_position, float2 center, float radius) {
	const float distance = length(sample_position - center) - radius;
	const float antialias = max(fwidth(distance), 0.001);
	return 1.0 - smoothstep(-antialias, antialias, distance);
}

float corner_outside(float2 sample_position, float2 center, float radius) {
	return 1.0 - corner_inside(sample_position, center, radius);
}

float rounded_coverage(float2 sample_position, float size, float requested_radius,
					   bool center, bool top, bool right, bool bottom, bool left) {
	if (requested_radius <= 0.0) {
		return center ? 1.0 : 0.0;
	}
	const float radius = min(requested_radius, size * 0.5);
	if (center) {
		float result = 1.0;
		if (!top && !left && sample_position.x < radius && sample_position.y < radius)
			result = min(result, corner_inside(sample_position, float2(radius, radius), radius));
		if (!top && !right && sample_position.x > size - radius && sample_position.y < radius)
			result = min(result, corner_inside(sample_position, float2(size - radius, radius), radius));
		if (!bottom && !right && sample_position.x > size - radius && sample_position.y > size - radius)
			result = min(result, corner_inside(sample_position, float2(size - radius, size - radius), radius));
		if (!bottom && !left && sample_position.x < radius && sample_position.y > size - radius)
			result = min(result, corner_inside(sample_position, float2(radius, size - radius), radius));
		return result;
	}

	float result = 0.0;
	if (top && left && sample_position.x < radius && sample_position.y < radius)
		result = max(result, corner_outside(sample_position, float2(radius, radius), radius));
	if (top && right && sample_position.x > size - radius && sample_position.y < radius)
		result = max(result, corner_outside(sample_position, float2(size - radius, radius), radius));
	if (bottom && right && sample_position.x > size - radius && sample_position.y > size - radius)
		result = max(result, corner_outside(sample_position, float2(size - radius, size - radius), radius));
	if (bottom && left && sample_position.x < radius && sample_position.y > size - radius)
		result = max(result, corner_outside(sample_position, float2(radius, size - radius), radius));
	return result;
}

float material_coverage(uint neighborhood, uint material, float2 position,
						float size, float radius) {
	return rounded_coverage(position, size, radius,
		material_occupied(cell_data(neighborhood, 0u), material),
		material_occupied(cell_data(neighborhood, 1u), material),
		material_occupied(cell_data(neighborhood, 2u), material),
		material_occupied(cell_data(neighborhood, 3u), material),
		material_occupied(cell_data(neighborhood, 4u), material));
}

float elevation_coverage(uint neighborhood, uint elevation, float2 position,
						 float size, float radius) {
	return rounded_coverage(position, size, radius,
		elevation_occupied(cell_data(neighborhood, 0u), elevation),
		elevation_occupied(cell_data(neighborhood, 1u), elevation),
		elevation_occupied(cell_data(neighborhood, 2u), elevation),
		elevation_occupied(cell_data(neighborhood, 3u), elevation),
		elevation_occupied(cell_data(neighborhood, 4u), elevation));
}

float grid_line_coverage(float coordinate, float spacing, float world_width,
						 float minimum_pixel_width) {
	const float distance_to_line =
		abs(frac(coordinate / spacing + 0.5) - 0.5) * spacing;
	const float pixel_world_size = max(fwidth(coordinate), 0.0001);
	const float half_width = max(world_width * 0.5,
		minimum_pixel_width * pixel_world_size * 0.5);
	return 1.0 - smoothstep(half_width - pixel_world_size * 0.65,
							 half_width + pixel_world_size * 0.65, distance_to_line);
}

float grid_coverage(float2 world_position, float spacing, float world_width,
					float minimum_pixel_width) {
	return max(
		grid_line_coverage(world_position.x, spacing, world_width, minimum_pixel_width),
		grid_line_coverage(world_position.y, spacing, world_width, minimum_pixel_width));
}

float4 main(FragmentInput input) : SV_Target0 {
	static const float3 palette[4] = {
		float3(0.27, 0.52, 0.65),
		float3(0.48, 0.68, 0.40),
		float3(0.47, 0.43, 0.39),
		float3(0.78, 0.64, 0.40)
	};
	const float2 sample_position = input.uv * input.tile_size;

	// Resolve every material transition in this fragment; no blended terrain
	// layers or additional draw calls are required.
	const float land = material_coverage(
		input.neighborhood, LAND, sample_position, input.tile_size, input.radius);
	const float mountain = material_coverage(
		input.neighborhood, MOUNTAIN, sample_position, input.tile_size, input.radius);
	const float road = material_coverage(
		input.neighborhood, ROAD, sample_position, input.tile_size, input.radius);
	float3 color = lerp(palette[WATER], palette[LAND], land);
	color = lerp(color, palette[MOUNTAIN], mountain);
	color = lerp(color, palette[ROAD], road);

	// Elevation is visual-only. Threshold masks create rounded cartographic
	// bands locally in the shader instead of becoming render passes.
	float depth = 0.0;
	for (uint elevation = 2u; elevation <= 5u; ++elevation) {
		const float coverage = elevation_coverage(
			input.neighborhood, elevation, sample_position, input.tile_size, input.radius);
		depth = lerp(depth, float(elevation - 1u) * input.height_darkening, coverage);
	}
	const float shade = max(0.35, 1.0 - depth);
	color *= lerp(1.0, shade, land);

	const uint tile_kind = cell_kind(cell_data(input.neighborhood, 0u));
	const bool tile_is_buildable = tile_kind != WATER && tile_kind != MOUNTAIN;
	if (input.grid_parameters.w > 0.5 && tile_is_buildable) {
		const float tile_grid = grid_coverage(input.world_position, input.tile_size,
			input.grid_parameters.x, input.grid_parameters.y);
		const float distance_from_cursor = distance(input.world_position, input.grid_fade.xy);
		const float fade = 1.0 - smoothstep(
			input.tile_size, input.grid_fade.z, distance_from_cursor);
		// Multiplicative tint preserves each buildable terrain material's hue.
		const float darkness = tile_grid * input.grid_parameters.z * fade;
		color *= 1.0 - darkness;
	}
	return float4(color, 1.0);
}
