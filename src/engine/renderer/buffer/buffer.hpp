#pragma once

#include "common/types.hpp"

struct SDL_GPUBuffer;
struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUTransferBuffer;

enum class BufferUsage : u8 {
	Vertex,
	Index,
};

class Buffer {
  public:
	Buffer() = default;
	~Buffer();
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;

	bool init(SDL_GPUDevice* device, u32 size, BufferUsage usage, bool streaming = false);
	void release();
	bool upload(SDL_GPUCommandBuffer* command, const void* data, u32 size, bool cycle = false);

	SDL_GPUBuffer* handle() const {
		return buffer;
	}
	u32 capacity() const {
		return buffer_size;
	}

  private:
	SDL_GPUDevice* device{nullptr};
	SDL_GPUBuffer* buffer{nullptr};
	SDL_GPUTransferBuffer* transfer_buffer{nullptr};
	u32 buffer_size{0};
};
