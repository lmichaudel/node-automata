#pragma once

#include "engine/input/input.hpp"
#include "engine/platform/platform.hpp"
#include "engine/renderer/renderer.hpp"

#include "core/controller/controller.hpp"
#include "core/state/state.hpp"
#include "core/viewer/viewer.hpp"

inline Input* g_input;
inline Platform* g_platform;
inline Renderer* g_renderer;

inline State* g_state;
inline Controller* g_controller;
inline Viewer* g_viewer;