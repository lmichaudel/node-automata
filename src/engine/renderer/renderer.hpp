#pragma once

#include "common/types.hpp"
#include "engine/renderer/buffer/buffer.hpp"
#include "engine/renderer/program/program.hpp"
#include "engine/renderer/sampler/sampler.hpp"
#include "engine/renderer/sprite/sprite.hpp"

#include <array>
#include <vector>

struct SDL_GPUDevice;
struct SDL_Window;
class Texture;

class Renderer {
  public:
	static constexpr usize LAYER_COUNT = 12;

	Renderer() = default;
	~Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	bool init(SDL_Window* window);
	void release();
	void begin_frame(vec4 clear_color = vec4{0.025F, 0.025F, 0.035F, 1.0F});
	void draw(const Sprite& sprite);
	void draw_texture(Texture* texture, vec2 center, vec2 size, vec4 color = vec4{1.0F},
					  vec4 uv_rect = vec4{0.0F, 0.0F, 1.0F, 1.0F}, f32 rotation = 0.0F,
					  u8 layer = 0);
	void draw_circle(vec2 center, f32 radius, vec4 color, u8 layer = 0);
	void draw_rounded_rect(vec2 center, vec2 size, f32 radius, vec4 color, f32 rotation = 0.0F,
						   u8 layer = 0);
	bool end_frame();

	SDL_GPUDevice* gpu_device() const;

  private:
	struct Instance {
		vec2 position;
		vec2 size;
		vec4 uv_rect;
		vec4 color;
		vec4 parameters;
	};

	struct DrawCommand {
		Texture* texture;
		u32 first_instance;
		u32 instance_count;
	};

	struct Batch {
		std::vector<Instance> instances{};
		std::vector<DrawCommand> commands{};
	};

	static constexpr usize MAX_SPRITES = 16'384;

	SDL_Window* window{nullptr};
	SDL_GPUDevice* device{nullptr};
	Program program{};
	Buffer vertex_buffer{};
	Buffer index_buffer{};
	Buffer instance_buffer{};
	Sampler nearest_sampler{};
	Sampler linear_sampler{};
	Texture* white_texture{nullptr};
	std::vector<Instance> instances{};
	std::array<Batch, LAYER_COUNT> batches{};
	vec4 clear_color{0.0F};
	usize sprite_count{0};

	bool overflow_reported{false};

	bool create_program();
	bool create_buffers();
	bool create_samplers();
	bool upload_static_data();
};
