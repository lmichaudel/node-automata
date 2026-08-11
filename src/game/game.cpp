#include "game.hpp"
#include "common/globals.hpp"

void Game::tick() {
}

void Game::update() {
}

void Game::draw() {
	g_renderer->draw_circle(vec2{400}, 40, vec4{1.0f});
}