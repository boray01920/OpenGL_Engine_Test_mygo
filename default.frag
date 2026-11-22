#version 330 core
out vec4 FragColor;

in vec3 crntPos;
in vec3 Normal;
in vec3 color;
in vec2 texCoord;

uniform sampler2D diffuse0;
uniform sampler2D specular0;
uniform sampler2D emissive0;

uniform vec4 lightColor;
uniform vec3 lightPos;
uniform vec3 camPos;
uniform bool hasDiffuse;
uniform bool hasSpecular;
uniform bool hasEmissive;
uniform vec4 baseColorFactor;
uniform vec3 emissiveFactor;

void main() {
	float ambient = 0.20f;

	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(lightPos - crntPos);

	float diffuse = max(dot(normal, lightDirection), 0.0f);
	
	float specularLight = 0.50f;

	vec3 viewDirection = normalize(camPos - crntPos);
	vec3 reflectionDirection = reflect(-lightDirection, normal);

	float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 16);
	float specular = specAmount * specularLight;

	vec4 baseColor = (hasDiffuse ? texture(diffuse0, texCoord) : vec4(1.0)) * baseColorFactor;
	float specMask = hasSpecular ? texture(specular0, texCoord).r : 0.0;
	vec3 emissive = emissiveFactor * (hasEmissive ? texture(emissive0, texCoord).rgb : vec3(1.0));

	FragColor = vec4(baseColor.rgb * lightColor.rgb * (diffuse + ambient) + specMask * specular + emissive, baseColor.a);
}