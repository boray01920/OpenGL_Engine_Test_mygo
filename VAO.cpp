#include "VAO.h"

VAO::VAO() {
	glGenVertexArrays(1, &ID);
}

void VAO::LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset) {
	VBO.Bind();
	if (type == GL_INT || type == GL_UNSIGNED_INT) {
		glVertexAttribIPointer(layout, numComponents, type, (GLsizei)stride, offset);
	}
	else {
		glVertexAttribPointer(layout, numComponents, type, GL_FALSE, (GLsizei)stride, offset);
	}
	glEnableVertexAttribArray(layout);
	VBO.Unbind();
}

void VAO::Bind() {
	glBindVertexArray(ID);
}

void VAO::Unbind() {
	glBindVertexArray(0);
}

void VAO::Delete() {
	glDeleteVertexArrays(1, &ID);
}