#include "debugger.hpp"
#include "engine/debug/metrics.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlgpu3.h>
#include <imgui.h>

Debugger::~Debugger() {
	release();
}

bool Debugger::init(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat target_format) {
	if (initialized || window == nullptr || device == nullptr) {
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
		ImGui::DestroyContext();
		return false;
	}
	ImGui_ImplSDLGPU3_InitInfo info{};
	info.Device = device;
	info.ColorTargetFormat = target_format;
	info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
	if (!ImGui_ImplSDLGPU3_Init(&info)) {
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	initialized = true;
	return true;
}

void Debugger::release() {
	if (!initialized) {
		return;
	}
	ImGui_ImplSDLGPU3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	initialized = false;
}

void Debugger::process_event(const SDL_Event& event) {
	if (!initialized) {
		return;
	}
	ImGui_ImplSDL3_ProcessEvent(&event);
	if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
		event.key.scancode == SDL_SCANCODE_F1) {
		visible = !visible;
	}
}

void Debugger::begin_frame() {
	if (!initialized) {
		return;
	}
	ImGui_ImplSDLGPU3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void Debugger::prepare_draw_data(SDL_GPUCommandBuffer* command) {
	if (!initialized) {
		return;
	}
	if (visible) {
		draw_metrics();
	}
	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();
	u64 draw_calls = 0;
	for (const ImDrawList* list : draw_data->CmdLists) {
		draw_calls += static_cast<u64>(list->CmdBuffer.Size);
	}
	metrics::set("Renderer/ImGui draw calls", static_cast<f64>(draw_calls), "calls");
	metrics::set("Renderer/ImGui vertices", draw_data->TotalVtxCount, "vertices");
	metrics::set("Renderer/ImGui indices", draw_data->TotalIdxCount, "indices");
	metrics::add("Renderer/Draw calls", static_cast<f64>(draw_calls), "calls");
	metrics::add("Renderer/Vertices", draw_data->TotalVtxCount, "vertices");
	ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command);
}

void Debugger::render(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* render_pass) {
	if (initialized) {
		ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), command, render_pass);
	}
}

bool Debugger::wants_mouse() const {
	return initialized && ImGui::GetIO().WantCaptureMouse;
}

bool Debugger::wants_keyboard() const {
	return initialized && ImGui::GetIO().WantCaptureKeyboard;
}

void Debugger::draw_metrics() {
	ImGui::SetNextWindowSize(ImVec2{650.0F, 430.0F}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Debugger (F1)", &visible)) {
		ImGui::End();
		return;
	}

	const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
								  ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
								  ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("metrics", 5, flags)) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Metric");
		ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.0F);
		ImGui::TableSetupColumn("Average", ImGuiTableColumnFlags_WidthFixed, 100.0F);
		ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthFixed, 90.0F);
		ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 90.0F);
		ImGui::TableHeadersRow();

		for (const metrics::Snapshot& metric : metrics::snapshots()) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(metric.tag.c_str());
			const auto value = [&](f64 number) {
				ImGui::Text("%.3f%s%s", number, metric.unit.empty() ? "" : " ",
							metric.unit.c_str());
			};
			ImGui::TableNextColumn();
			value(metric.value);
			ImGui::TableNextColumn();
			value(metric.average);
			ImGui::TableNextColumn();
			value(metric.minimum);
			ImGui::TableNextColumn();
			value(metric.maximum);
		}
		ImGui::EndTable();
	}
	ImGui::End();
}
