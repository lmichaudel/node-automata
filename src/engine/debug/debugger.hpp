#pragma once

#include "common/types.hpp"

#include <SDL3/SDL_gpu.h>

union SDL_Event;
struct SDL_Window;

class Debugger {
  public:
	Debugger() = default;
	~Debugger();
	Debugger(const Debugger&) = delete;
	Debugger& operator=(const Debugger&) = delete;

	bool init(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat target_format);
	void release();
	void process_event(const SDL_Event& event);
	void begin_frame();
	void prepare_draw_data(SDL_GPUCommandBuffer* command);
	void render(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* render_pass);

	bool wants_mouse() const;
	bool wants_keyboard() const;
	vec4 clear_color() const;
	vec4 grid_color() const;
	vec4 supergrid_color() const;

  private:
	bool initialized{false};
	bool visible{true};
	vec4 clear_color_value{rgba(52, 52, 55)};
	vec4 grid_color_value{rgba(68, 68, 71, 210)};
	vec4 supergrid_color_value{rgba(122, 122, 125, 225)};

	void draw_metrics();
};
