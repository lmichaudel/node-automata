#pragma once

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

  private:
	bool initialized{false};
	bool visible{true};

	void draw_metrics();
};
