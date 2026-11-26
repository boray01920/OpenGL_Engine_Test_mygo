#include "Resources/Model.h"
#include "Resources/Animation.h"
#include "Resources/Animator.h"
#include "Resources/Physics.h"
#include "Resources/Player.h"
#include "Resources/Primitives.h"
#include "Resources/Level.h"
#include "Resources/WeaponDefs.h"
#include "Resources/Audio.h"
#include "Resources/UI.h"

#define PI 3.1415926535897

const unsigned int width = 1280;
const unsigned int height = 720;


int main() {

	glfwInit();

	// GLFW version, 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "Quake Pro", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window); 
	gladLoadGL(); 

	glViewport(0, 0, width, height); // x:0, y:0 to x:800, y:800

	Shader shaderProgram(Paths::Shader("default.vert").c_str(), Paths::Shader("default.frag").c_str());
	Shader lightShader(Paths::Shader("light.vert").c_str(), Paths::Shader("light.frag").c_str());

	glm::vec3 lightPos = glm::vec3(2.0f, 10.0f, 2.0f);
	glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	glm::mat4 lightModel = glm::translate(glm::mat4(1.0f), lightPos);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	glEnable(GL_DEPTH_TEST);
	
	for (int i = 0; i < 100; i++) {
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()),
			1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
	}

	Camera camera(width, height, glm::vec3(0.0f, 0.0f, 2.0f));
	camera.movementEnabled = false;

						// ---------------- level ---------------- //
	// maptest.obj 
	// Quake units (32 units = 1 m). 
	// floor slab top is at y=176 -> 5.5 m, so shift down to put it at y=0.
	Level level;
	const int levelNumber = 99;                       //assets/maps/map_99/
	level.LoadLevel(levelNumber, glm::vec3(0.0f, -5.5f, 0.0f), 1.0f / 32.0f);   // loads the .obj and its .rules (edit the text file to change object types, no rebuild needed))
	level.PrintObjects();

	PhysicsWorld world;
	level.BuildPhysics(world);

						// ------- Harcoded data for spawning the player ------- //
	// 
	// y = -6.0 is the perfect for spawning on the ground at y:0 value of trenchbroom
	// I keep it at -5.1 so you can feel the spawning jump but anything above this should instantly spawn the player with no visible effect
	// Every level will use this spawnpoint and the maps will be made accordingly to this spawnpoint...
	// That is until i figure out entities in trenchbroom
	const glm::vec3 spawnPoint(0.0f, -5.1f, -10.0f); 
	Player player(spawnPoint);
	camera.Orientation = glm::vec3(0.0f, 0.0f, -1.0f);

						// ---------------- weapons ---------------- //
	WeaponSystem weapons;
	RegisterWeapons(weapons);
	GiveStartingLoadout(weapons);
	weapons.BindScroll(window);   // mouse wheel

						// ---------------- audio ---------------- //
	Audio audio;
	for (const WeaponDef& d : weapons.defs) audio.Preload(d.fireSound);
	weapons.onFire = [&](const WeaponDef& d) { audio.Play(d.fireSound, d.fireSoundVolume, d.fireSoundMax); };
	audio.PlayMusic("music/kessoku.mp3");   // drop a file in assets/sounds/music/ and uncomment
	ImpactMarkers impacts;
	weapons.onImpact = [&](const Impact& i) { impacts.Add(i); };   //later impact stuff

						// ---------------- ui ---------------- //
	UI ui;
	ui.Init(width, height); //initialize ui system for the dedicated window size
	ui.menuCursor = ui.LoadImage((Paths::Root() / "assets" / "ui" / "nina.png").generic_string());   // texture next to the hovered menu entry

	enum class GameState { MainMenu, Playing, Paused };
	GameState state = GameState::MainMenu;
	int menuSelected = 0;
	float fps = 0.0f;

	float lastFrame = 0.0f;

	while (!glfwWindowShouldClose(window)) {

		glClearColor(0.54f, 0.84f, 0.89f, 1.0f); //sky color rgb(139, 215, 228)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		float currentFrame = (float)glfwGetTime();
		float deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		deltaTime = std::min(deltaTime, 0.05f);

		ui.Begin(window, deltaTime);
		fps = glm::mix(fps, 1.0f / std::max(deltaTime, 0.0001f), 0.05f);

		// ESC toggles pause while in a level
		if (ui.KeyPressed(GLFW_KEY_ESCAPE)) {
			if (state == GameState::Playing)     state = GameState::Paused;
			else if (state == GameState::Paused) state = GameState::Playing;
		}

		// ---- input + physics (frozen while a menu is up) ---- //
		if (state == GameState::Playing) {
			camera.Inputs(window);   // mouse look only
			player.Update(window, camera, world, deltaTime);
			weapons.Update(window, camera, world, deltaTime);   // after the player so the camera is in its final position
			impacts.Update(deltaTime);
		}

		shaderProgram.Activate();
		
		camera.updateMatrix(60.0f, 0.05f, 100.0f);

		level.Draw(shaderProgram, camera);
		impacts.Draw(shaderProgram, camera);
		weapons.DrawViewModel(shaderProgram, camera);   // last clears depth so it draws over everything

		// ---------------- UI draw last---------------- //
		if (state == GameState::Playing) {
			// crosshair
			ui.TextCentered(width / 2, height / 2 - 12, "⋅", 3, UI::White);

			// ammo / weapon, bottom right
			const WeaponDef& wd = weapons.Current();
			ui.TextRight(width - 30, height - 100, wd.name, 4, UI::Orange);
			if (wd.ammoType != AMMO_NONE)
				ui.TextF(width - 30 - ui.TextWidth("000", 5), height - 60, 5, UI::Yellow, "%3d", weapons.ammo[wd.ammoType]);

			ui.TextRight(width - 20, 20, std::to_string((int)fps) + " FPS", 2, UI::Gray);
		}
		else {

			if (state == GameState::MainMenu) {
				int pick = ui.Menu("BIG NAME", { "  Play  ", "Options", "  Quit  " }, menuSelected);
				if (pick == 0) state = GameState::Playing;
				if (pick == 1) ui.Notify("lorem ipsum", 2.0f, UI::Yellow);
				 //am i fucking schizophrenic why is there a green triangle in the editor
				if (pick == 2) glfwSetWindowShouldClose(window, GLFW_TRUE);
				ui.TextCentered(width / 2, height - 60, "lorem ipsum", 2, UI::Gray);
			}
			else {
				int pick = ui.Menu("pause", { "Resume", "Main Menu", "Quit" }, menuSelected);
				if (pick == 0) state = GameState::Playing;
				if (pick == 1) { state = GameState::MainMenu; menuSelected = 0; }
				if (pick == 2) glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		}
		ui.End();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}


	ui.Delete();
	shaderProgram.Delete();


	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;

}

//to defy