#include "program.hpp"
#include "common/log.hpp"

#include <SDL3/SDL.h>
#include <string>

namespace {
	const char* native_shader_extension() {
#if defined(__APPLE__)
		return "msl";
#elif defined(_WIN32)
		return "dxil";
#else
		return "spv";
#endif
	}

	const char* native_shader_entrypoint() {
#if defined(__APPLE__)
		return "main0";
#else
		return "main";
#endif
	}

	SDL_GPUShader* load_shader(SDL_GPUDevice* device, const char* name, SDL_GPUShaderStage stage,
							   u32 sampler_count, u32 uniform_buffer_count) {
		const std::string filename = std::string{name} + "." + native_shader_extension();
		const char* base_path = SDL_GetBasePath();
		const std::string installed_path =
			std::string{base_path != nullptr ? base_path : ""} + "shaders/" + filename;

		size_t code_size = 0;
		void* code = SDL_LoadFile(installed_path.c_str(), &code_size);
		if (code == nullptr) {
			const std::string development_path = "shaders/compiled/" + filename;
			code = SDL_LoadFile(development_path.c_str(), &code_size);
		}
		if (code == nullptr) {
			log::error("Failed to load shader {}: {}", filename, SDL_GetError());
			return nullptr;
		}

		const SDL_GPUShaderCreateInfo create_info{
			.code_size = code_size,
			.code = static_cast<const u8*>(code),
			.entrypoint = native_shader_entrypoint(),
			.format = Program::native_shader_format(),
			.stage = stage,
			.num_samplers = sampler_count,
			.num_uniform_buffers = uniform_buffer_count,
		};
		SDL_GPUShader* shader = SDL_CreateGPUShader(device, &create_info);
		SDL_free(code);
		if (shader == nullptr) {
			log::error("Failed to create shader {}: {}", filename, SDL_GetError());
		}
		return shader;
	}
} // namespace

Program::~Program() {
	release();
}

bool Program::init(SDL_GPUDevice* target_device, u32 color_target_format,
				   const SDL_GPUVertexInputState& vertex_input) {
	if (pipeline != nullptr || target_device == nullptr) {
		return false;
	}

	device = target_device;
	SDL_GPUShader* vertex_shader =
		load_shader(device, "sprite.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
	SDL_GPUShader* fragment_shader =
		load_shader(device, "sprite.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
	if (vertex_shader == nullptr || fragment_shader == nullptr) {
		if (vertex_shader != nullptr)
			SDL_ReleaseGPUShader(device, vertex_shader);
		if (fragment_shader != nullptr)
			SDL_ReleaseGPUShader(device, fragment_shader);
		device = nullptr;
		return false;
	}

	const SDL_GPUColorTargetDescription color_target{
		.format = static_cast<SDL_GPUTextureFormat>(color_target_format),
		.blend_state =
			{
				.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
				.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
				.color_blend_op = SDL_GPU_BLENDOP_ADD,
				.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
				.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
				.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
				.enable_blend = true,
			},
	};
	const SDL_GPUGraphicsPipelineCreateInfo pipeline_info{
		.vertex_shader = vertex_shader,
		.fragment_shader = fragment_shader,
		.vertex_input_state = vertex_input,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.rasterizer_state =
			{
				.fill_mode = SDL_GPU_FILLMODE_FILL,
				.cull_mode = SDL_GPU_CULLMODE_NONE,
				.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			},
		.multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
		.target_info =
			{
				.color_target_descriptions = &color_target,
				.num_color_targets = 1,
			},
	};
	pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
	SDL_ReleaseGPUShader(device, vertex_shader);
	SDL_ReleaseGPUShader(device, fragment_shader);
	if (pipeline == nullptr) {
		log::error("Failed to create sprite graphics pipeline: {}", SDL_GetError());
		device = nullptr;
		return false;
	}
	return true;
}

void Program::release() {
	if (device != nullptr && pipeline != nullptr) {
		SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
	}
	device = nullptr;
	pipeline = nullptr;
}

u32 Program::native_shader_format() {
#if defined(__APPLE__)
	return SDL_GPU_SHADERFORMAT_MSL;
#elif defined(_WIN32)
	return SDL_GPU_SHADERFORMAT_DXIL;
#else
	return SDL_GPU_SHADERFORMAT_SPIRV;
#endif
}
