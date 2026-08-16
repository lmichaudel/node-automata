#pragma once

class State;

class Controller {
	State& state;

  public:
	Controller(State& state);

	void update();
	void draw();
};