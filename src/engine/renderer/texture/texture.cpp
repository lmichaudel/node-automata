#include "texture.hpp"
#include "common/log.hpp"

#include <SDL3/SDL_gpu.h>
#include <cstring>
#include <limits>

Texture::~Texture() {
	release();
}

void Texture::release() {
	if (device != nullptr && gpu_texture != nullptr) {
		SDL_ReleaseGPUTexture(device, gpu_texture);
	}
	device = nullptr;
	gpu_texture = nullptr;
	texture_width = 0;
	texture_height = 0;
	filter = TextureFilter::Linear;
}

bool Texture::init(SDL_GPUDevice* target_device, u32 width, u32 height, std::span<const u8> pixels,
				   TextureFilter texture_filter) {
	if (device != nullptr || target_device == nullptr || width == 0 || height == 0 ||
		static_cast<u64>(width) * height * 4 > std::numeric_limits<u32>::max()) {
		return false;
	}

	const u32 byte_count = width * height * 4;
	if (pixels.size() < byte_count) {
		log::error("RGBA texture data is too small for {}x{} pixels", width, height);
		return false;
	}

	const SDL_GPUTextureCreateInfo texture_info{
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
		.width = width,
		.height = height,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = SDL_GPU_SAMPLECOUNT_1,
	};
	SDL_GPUTexture* texture = SDL_CreateGPUTexture(target_device, &texture_info);
	if (texture == nullptr) {
		log::error("Failed to create {}x{} GPU texture: {}", width, height, SDL_GetError());
		return false;
	}

	const SDL_GPUTransferBufferCreateInfo transfer_info{
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = byte_count,
	};
	SDL_GPUTransferBuffer* transfer_buffer =
		SDL_CreateGPUTransferBuffer(target_device, &transfer_info);
	if (transfer_buffer == nullptr) {
		log::error("Failed to create texture transfer buffer: {}", SDL_GetError());
		SDL_ReleaseGPUTexture(target_device, texture);
		return false;
	}

	void* destination = SDL_MapGPUTransferBuffer(target_device, transfer_buffer, false);
	if (destination == nullptr) {
		log::error("Failed to map texture transfer buffer: {}", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(target_device, transfer_buffer);
		SDL_ReleaseGPUTexture(target_device, texture);
		return false;
	}
	std::memcpy(destination, pixels.data(), byte_count);
	SDL_UnmapGPUTransferBuffer(target_device, transfer_buffer);

	SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(target_device);
	if (command_buffer == nullptr) {
		log::error("Failed to acquire texture upload command buffer: {}", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(target_device, transfer_buffer);
		SDL_ReleaseGPUTexture(target_device, texture);
		return false;
	}
	SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
	const SDL_GPUTextureTransferInfo source{
		.transfer_buffer = transfer_buffer,
		.pixels_per_row = width,
		.rows_per_layer = height,
	};
	const SDL_GPUTextureRegion destination_region{
		.texture = texture,
		.w = width,
		.h = height,
		.d = 1,
	};
	SDL_UploadToGPUTexture(copy_pass, &source, &destination_region, false);
	SDL_EndGPUCopyPass(copy_pass);
	if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
		log::error("Failed to submit texture upload: {}", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(target_device, transfer_buffer);
		SDL_ReleaseGPUTexture(target_device, texture);
		return false;
	}
	SDL_ReleaseGPUTransferBuffer(target_device, transfer_buffer);

	device = target_device;
	gpu_texture = texture;
	texture_width = width;
	texture_height = height;
	filter = texture_filter;
	return true;
}
