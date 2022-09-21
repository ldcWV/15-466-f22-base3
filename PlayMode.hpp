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
	bool turtle_dead[MAX_TURTLES];
	int num_turtles_left;
	Sound::Sample* kill_sound;

	// Assassins
	static constexpr uint16_t NUM_ASSASSINS = 10;
	static constexpr uint16_t NUM_ASSASSIN_SCAN_SOUNDS = 7;
	static constexpr float dist_thresholds[NUM_ASSASSIN_SCAN_SOUNDS - 1] = {5, 10, 15, 20, 25, 30};
	bool played_assassin_scan_sound = false;
	Sound::Sample* assassin_scan_sounds[NUM_ASSASSIN_SCAN_SOUNDS];
	
	// Game over flag
	int game_over = 0;

	//camera:
	Scene::Camera *camera = nullptr;

};
