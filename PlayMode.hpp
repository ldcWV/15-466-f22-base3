#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t pressed = 0;
	} left, right, up, down, r, z, x, space;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// Duck transforms
	Scene::Transform* duck = nullptr;
	glm::vec3 duck_initial_position;
	glm::quat duck_initial_rotation;

	// Turtle stuff
	static constexpr uint16_t MAX_TURTLES = 100;
	uint16_t num_turtles = 0;
	float turtle_z;
	glm::quat turtle_initial_rotation;
	Scene::Transform* turtles[MAX_TURTLES];
	float time_since_update = 0;
	float turtle_angles[MAX_TURTLES];
	int turtle_turn_dirs[MAX_TURTLES]; // -1 for left, 0 for still, 1 for right
	
	// Game over flag
	bool game_over = false;

	//camera:
	Scene::Camera *camera = nullptr;

};
