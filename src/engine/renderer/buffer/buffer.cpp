#include "buffer.hpp"
#include "common/log.hpp"

#include <SDL3/SDL_gpu.h>
#include <cstring>

Buffer::~Buffer() {
	release();
}

void Buffer::release() {
	if (device == nullptr) {
		return;
	}
	if (transfer_buffer != nullptr) {
		SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
	}
	if (buffer != nullptr) {
		SDL_ReleaseGPUBuffer(device, buffer);
	}
	device = nullptr;
	buffer = nullptr;
	transfer_buffer = nullptr;
	buffer_size = 0;
}

bool Buffer::init(SDL_GPUDevice* target_device, u32 size, BufferUsage usage, bool streaming) {
	if (device != nullptr || target_device == nullptr || size == 0) {
		return false;
	}

	const SDL_GPUBufferCreateInfo buffer_info{
		.usage =
			usage == BufferUsage::Vertex ? SDL_GPU_BUFFERUSAGE_VERTEX : SDL_GPU_BUFFERUSAGE_INDEX,
		.size = size,
	};
	buffer = SDL_CreateGPUBuffer(target_device, &buffer_info);
	if (buffer == nullptr) {
		log::error("Failed to create GPU buffer: {}", SDL_GetError());
		return false;
	}

	device = target_device;
	buffer_size = size;
	if (streaming) {
		const SDL_GPUTransferBufferCreateInfo transfer_info{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = size,
		};
		transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
		if (transfer_buffer == nullptr) {
			log::error("Failed to create GPU upload buffer: {}", SDL_GetError());
			release();
			return false;
		}
	}
	return true;
}

bool Buffer::upload(SDL_GPUCommandBuffer* command, const void* data, u32 size, bool cycle) {
	if (device == nullptr || buffer == nullptr || command == nullptr || data == nullptr ||
		size == 0 || size > buffer_size) {
		return false;
	}

	SDL_GPUTransferBuffer* transfer = transfer_buffer;
	const bool temporary_transfer = transfer == nullptr;
	if (temporary_transfer) {
		const SDL_GPUTransferBufferCreateInfo transfer_info{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = size,
		};
		transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
		if (transfer == nullptr) {
			log::error("Failed to create temporary GPU upload buffer: {}", SDL_GetError());
			return false;
		}
	}

	void* destination = SDL_MapGPUTransferBuffer(device, transfer, !temporary_transfer && cycle);
	if (destination == nullptr) {
		log::error("Failed to map GPU upload buffer: {}", SDL_GetError());
		if (temporary_transfer) {
			SDL_ReleaseGPUTransferBuffer(device, transfer);
		}
		return false;
	}
	std::memcpy(destination, data, size);
	SDL_UnmapGPUTransferBuffer(device, transfer);

	SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command);
	const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer};
	const SDL_GPUBufferRegion target{.buffer = buffer, .size = size};
	SDL_UploadToGPUBuffer(copy_pass, &source, &target, cycle);
	SDL_EndGPUCopyPass(copy_pass);
	if (temporary_transfer) {
		SDL_ReleaseGPUTransferBuffer(device, transfer);
	}
	return true;
}
