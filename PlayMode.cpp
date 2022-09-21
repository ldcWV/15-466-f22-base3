#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint duck_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > duck_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("duck.pnct"));
	duck_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > duck_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("duck.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = duck_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = duck_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

std::default_random_engine gen;
std::uniform_real_distribution<float> distribution(0, 1);

PlayMode::PlayMode() : scene(*duck_scene) {
	for (auto& transform : scene.transforms) {
		if (transform.name == "Duck") {
			duck = &transform;
		} else if (transform.name.substr(0, 6) == "Turtle") {
			turtles[num_turtles++] = &transform;
			if (transform.name == "Turtle") { // Original reference turtle
				turtle_z = transform.position.z;
				turtle_initial_rotation = transform.rotation;
			}
		}
	}
	if (duck == nullptr) {
		throw std::runtime_error("Duck not found.");
	}

	duck_initial_position = duck->position;
	duck_initial_rotation = duck->rotation;

	// spawn turtles
	static float pi = acosf(-1.f);
	for (uint16_t i = 0; i < num_turtles; i++) {
		static float radius = 50.f;
		float tx, ty;
		while(true) {
			tx = -1 + 2*distribution(gen);
			ty = -1 + 2*distribution(gen);
			float dist = tx*tx + ty*ty;
			if (dist > 1 || dist < 0.25) continue;
			turtles[i]->position.x = tx * radius;
			turtles[i]->position.y = ty * radius;
			turtles[i]->position.z = turtle_z;
			break;
		}
		turtle_angles[i] = distribution(gen) * 2 * pi;
		turtle_turn_dirs[i] = 0;
		turtle_dead[i] = false;
	}
	for (uint16_t i = 0; i < NUM_ASSASSIN_SCAN_SOUNDS; i++) {
		assassin_scan_sounds[i] = new Sound::Sample(data_path("s" + std::to_string(i) + ".wav"));
	}

	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	bool is_down = evt.type == SDL_KEYDOWN;
	if (is_down && evt.key.keysym.sym == SDLK_ESCAPE) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		return true;
	}
	if (evt.key.keysym.sym == SDLK_LEFT) {
		left.pressed = is_down;
		return true;
	} else if (evt.key.keysym.sym == SDLK_RIGHT) {
		right.pressed = is_down;
		return true;
	} else if (evt.key.keysym.sym == SDLK_UP) {
		up.pressed = is_down;
		return true;
	} else if (evt.key.keysym.sym == SDLK_DOWN) {
		down.pressed = is_down;
		return true;
	} else if (evt.key.keysym.sym == SDLK_r) {
		r.pressed = is_down;
		return true;
	} else if (evt.key.keysym.sym == SDLK_z) {
		z.pressed = is_down;
		return true;
	} else if (evt.key.keysym.sym == SDLK_x) {
		x.pressed = is_down;
		return true;
	} else if (evt.key.keysym.sym == SDLK_SPACE) {
		space.pressed = is_down;
		return true;
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (game_over) {
		if (r.pressed) {
			game_over = 0;

			// Reset duck position
			duck->position = duck_initial_position;

			// Reset turtles
			static float pi = acosf(-1.f);
			for (uint16_t i = 0; i < num_turtles; i++) {
				static float radius = 50.f;
				float tx, ty;
				while(true) {
					tx = -1 + 2*distribution(gen);
					ty = -1 + 2*distribution(gen);
					float dist = tx*tx + ty*ty;
					if (dist > 1 || dist < 0.25) continue;
					turtles[i]->position.x = tx * radius;
					turtles[i]->position.y = ty * radius;
					turtles[i]->position.z = turtle_z;
					break;
				}
				turtle_angles[i] = distribution(gen) * 2 * pi;
				turtle_turn_dirs[i] = 0;
				turtle_dead[i] = false;
			}
		}
		return;
	}

	{ // update duck position
		int ctr = (left.pressed ^ right.pressed) + (up.pressed ^ down.pressed);
		float speed = space.pressed ? 40.f : 20.f;
		float strafe = (ctr == 1 ? speed : speed/sqrtf(2.f)) * elapsed;
		if (left.pressed) duck->position -= glm::vec3(0.f, strafe, 0.f);
		if (right.pressed) duck->position += glm::vec3(0.f, strafe, 0.f);
		if (up.pressed) duck->position -= glm::vec3(strafe, 0.f, 0.f);
		if (down.pressed) duck->position += glm::vec3(strafe, 0.f, 0.f);

		// clip to inside the pond
		static float pond_radius = 50.f;
		float dist = glm::distance(duck->position, duck_initial_position);
		if (dist > pond_radius) {
			glm::vec3 diff = duck->position - duck_initial_position;
			float angle = atan2f(diff.y, diff.x);
			duck->position.x = pond_radius * cosf(angle);
			duck->position.y = pond_radius * sinf(angle);
		}

		// turn towards movement direction
		{
			float dx = float(right.pressed - left.pressed);
			float dy = float(up.pressed - down.pressed);
			if (dx != 0.f || dy != 0.f) {
				dx /= sqrtf(abs(dx) + abs(dy));
				dy /= sqrtf(abs(dx) + abs(dy));
				float angle = atan2f(-dx, dy);
				duck->rotation = duck_initial_rotation * glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
			}
		}
	}

	{ // update turtle positions
		time_since_update += elapsed;
	 	// randomly change during direction of some turtles
		if (time_since_update > 0.2f) {
			time_since_update = 0;
			for (uint16_t i = 0; i < num_turtles; i++) {
				if (distribution(gen) < 0.2f) { // only some turtles change direction
					float rng = distribution(gen);
					if (rng < 1.f/3.f) {
						turtle_turn_dirs[i] = -1;
					} else if (rng < 2.f / 3.f) {
						turtle_turn_dirs[i] = 0;
					} else {
						turtle_turn_dirs[i] = 1;
					}
				}
			}
		}
		// update turtles by moving then turning
		static float speed = 5.f;
		static float turn_speed = 1.5f;
		static float pi = acosf(-1.f);
		for (uint16_t i = 0; i < num_turtles; i++) {
			turtles[i]->position.x += speed * elapsed * cosf(turtle_angles[i]);
			turtles[i]->position.y += speed * elapsed * sinf(turtle_angles[i]);
			turtle_angles[i] += turtle_turn_dirs[i] * elapsed * turn_speed;
			if (turtle_angles[i] < 0) turtle_angles[i] += 2 * pi;
			if (turtle_angles[i] > 0) turtle_angles[i] -= 2 * pi;
		}
		// clip turtles into pond
		for (uint16_t i = 0; i < num_turtles; i++) {
			static float pond_radius = 50.f;
			float dist = glm::distance(turtles[i]->position, glm::vec3(0.f, 0.f, turtle_z));
			if (dist > pond_radius) {
				glm::vec3 diff = turtles[i]->position - glm::vec3(0.f, 0.f, turtle_z);
				float angle = atan2f(diff.y, diff.x);
				turtles[i]->position.x = pond_radius * cosf(angle);
				turtles[i]->position.y = pond_radius * sinf(angle);
				turtle_angles[i] += pi;
			}
		}
		// rotate turtles
		for (uint16_t i = 0; i < num_turtles; i++) {
			turtles[i]->rotation = turtle_initial_rotation * glm::angleAxis(turtle_angles[i], glm::vec3(0.f, 0.f, 1.f));
		}
	}

	{ // assassin scan
		if (z.pressed) {
			if (!played_assassin_scan_sound) {
				// find distance of closest assassin turtle
				float closest = 100.f;
				for (uint16_t i = 0; i < NUM_ASSASSINS; i++) {
					closest = std::min(closest, glm::distance(turtles[i]->position, duck->position));
				}
				uint16_t chosen_sound = 0;
				for (uint16_t i = 0; i < NUM_ASSASSIN_SCAN_SOUNDS - 1; i++) {
					if (closest < dist_thresholds[i]) {
						chosen_sound = NUM_ASSASSIN_SCAN_SOUNDS - 1 - i;
						break;
					}
				}
				Sound::play(*assassin_scan_sounds[chosen_sound]);
				played_assassin_scan_sound = true;
			}
		} else {
			played_assassin_scan_sound = false;
		}
	}

	{ // eat normal turtles
		for (uint16_t i = NUM_ASSASSINS; i < num_turtles; i++) {
			float dist = glm::distance(turtles[i]->position, duck->position);
			if (dist < 2.f) {
				turtle_dead[i] = true;
			}
		}
	}

	{ // move dead turtles out of screen
		for (uint16_t i = NUM_ASSASSINS; i < num_turtles; i++) {
			if (turtle_dead[i]) {
				turtles[i]->position.z = -10;
			}
		}
	}

	{ // touch assassin = death
		for (uint16_t i = 0; i < NUM_ASSASSINS; i++) {
			float dist = glm::distance(turtles[i]->position, duck->position);
			if (dist < 2.f) {
				game_over = 1;
			}
		}
	}

	{ // All turtles dead = win
		num_turtles_left = 0;
		for (uint16_t i = NUM_ASSASSINS; i < num_turtles; i++) {
			if (!turtle_dead[i]) num_turtles_left++;
		}
		if (num_turtles_left == 0) game_over = 2;
	}

}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	auto draw_text = [&](std::string text) {
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text(text,
			glm::vec3(-aspect + 0.2 * H, -0.5f + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(text,
			glm::vec3(-aspect + 0.2 * H + ofs, -0.5f + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	};

	if (game_over == 0) {
		draw_text("Turtles remaining: " + std::to_string(num_turtles_left));
	} else if (game_over == 1) {
		draw_text("You touched an assassin! Press R to restart.");
	} else if (game_over == 2) {
		draw_text("You captured all the turtles! Press R to restart.");
	}
}
