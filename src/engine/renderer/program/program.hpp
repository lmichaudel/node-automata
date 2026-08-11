#pragma once

#include "common/types.hpp"

struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUVertexInputState;

class Program {
  public:
	Program() = default;
	~Program();
	Program(const Program&) = delete;
	Program& operator=(const Program&) = delete;

	bool init(SDL_GPUDevice* device, u32 color_target_format,
			  const SDL_GPUVertexInputState& vertex_input);
	void release();

	SDL_GPUGraphicsPipeline* handle() const {
		return pipeline;
	}
	static u32 native_shader_format();

  private:
	SDL_GPUDevice* device{nullptr};
	SDL_GPUGraphicsPipeline* pipeline{nullptr};
};
