#version 330 core

//in vec2 texCoord;
//uniform sampler2D tex0;

out vec4 FragColor;

uniform vec4 lightColor;

void main()
{
	FragColor = lightColor;
}