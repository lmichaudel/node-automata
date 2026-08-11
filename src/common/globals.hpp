#pragma once

#include "engine/assets/assets.hpp"
#include "engine/input/input.hpp"
#include "engine/platform/platform.hpp"
#include "engine/renderer/renderer.hpp"

#include "game/game.hpp"

inline Input* g_input;
inline Platform* g_platform;
inline Renderer* g_renderer;
inline Assets* g_assets;

inline Game* g_game;