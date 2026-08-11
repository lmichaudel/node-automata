#pragma once

#include "common/types.hpp"

struct SDL_GPUDevice;
struct SDL_GPUSampler;

enum class SamplerFilter : u8 {
	Nearest,
	Linear,
};

class Sampler {
  public:
	Sampler() = default;
	~Sampler();
	Sampler(const Sampler&) = delete;
	Sampler& operator=(const Sampler&) = delete;

	bool init(SDL_GPUDevice* device, SamplerFilter filter);
	void release();

	SDL_GPUSampler* handle() const {
		return sampler;
	}

  private:
	SDL_GPUDevice* device{nullptr};
	SDL_GPUSampler* sampler{nullptr};
};
