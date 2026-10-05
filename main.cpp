#include "Model.h"
#include "Animation.h"
#include "Animator.h"

#define PI 3.1415926535897

const unsigned int width = 800;
const unsigned int height = 800;


int main() {

	glfwInit();

	// GLFW version, 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "OpenglTest", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window); 
	gladLoadGL(); 

	glViewport(0, 0, width, height); // x:0, y:0 to x:800, y:800

	Shader shaderProgram("default.vert", "default.frag");
	Shader lightShader("light.vert", "light.frag");

	glm::vec3 lightPos = glm::vec3(1.0f, 0.5f, 1.0f); 
	glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	glm::mat4 lightModel = glm::translate(lightModel, lightPos);

	//lightShader.Activate();
	//glUniform4f(glGetUniformLocation(lightShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	glEnable(GL_DEPTH_TEST);
	
	Camera camera(width, height, glm::vec3(0.0f, 0.0f, 2.0f));

	Model model("models/animdemo/scene.gltf");
	model.position = glm::vec3(0.0f, 0.5f, -1.0f);
	Animation animdemo("models/animdemo/scene.gltf", &model);
	Animator animator(&animdemo);
	//glm::quat uprightFix = glm::angleAxis(glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	
	float lastFrame = 0.0f;
	

	while (!glfwWindowShouldClose(window)) {

		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		float currentFrame = (float)glfwGetTime();
		float deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		animator.UpdateAnimation(deltaTime);

		shaderProgram.Activate();
		auto transforms = animator.GetFinalBoneMatrices();
		for (int i = 0; i < transforms.size(); i++) {
			glUniformMatrix4fv(
				glGetUniformLocation(shaderProgram.ID, ("finalBonesMatrices[" + std::to_string(i) + "]").c_str()),
				1, GL_FALSE, glm::value_ptr(transforms[i])
			);
		}


		camera.Inputs(window);
		camera.updateMatrix(45.0f, 0.1f, 100.0f);
		/*
		float t = (float)glfwGetTime();
		glm::quat spin = glm::angleAxis(t, glm::vec3(0.0f, 1.0f, 0.0f));
		model.rotation = spin;
		*/


		model.Draw(shaderProgram, camera);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}


	shaderProgram.Delete();


	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;

}