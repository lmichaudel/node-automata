#include "sampler.hpp"
#include "common/log.hpp"

#include <SDL3/SDL.h>

Sampler::~Sampler() {
	release();
}

bool Sampler::init(SDL_GPUDevice* target_device, SamplerFilter filter) {
	if (device != nullptr || target_device == nullptr) {
		return false;
	}

	const bool linear = filter == SamplerFilter::Linear;
	const SDL_GPUSamplerCreateInfo info{
		.min_filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST,
		.mag_filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST,
		.mipmap_mode =
			linear ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	};
	sampler = SDL_CreateGPUSampler(target_device, &info);
	if (sampler == nullptr) {
		log::error("Failed to create GPU sampler: {}", SDL_GetError());
		return false;
	}
	device = target_device;
	return true;
}

void Sampler::release() {
	if (device != nullptr && sampler != nullptr) {
		SDL_ReleaseGPUSampler(device, sampler);
	}
	device = nullptr;
	sampler = nullptr;
}
