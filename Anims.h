#ifndef ANIMS_CLASS_H
#define ANIMS_CLASS_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <GLFW/glfw3.h>

void UpdateLightAnim(glm::vec3& lightPos, glm::mat4& lightModel, const glm::vec3& pyramidPos, float radius);

#endif