#include "Resources/UI.h"
#include "Resources/shaderClass.h"
#include <font8x8_basic.h> //shitfont
#include <stb/stb_image.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstdio>
#include <iostream>


//colors 
const glm::vec4 UI::White       (1.0f, 1.0f, 1.0f, 1.0f);
const glm::vec4 UI::Black       (0.0f, 0.0f, 0.0f, 1.0f);
const glm::vec4 UI::Red         (1.0f, 0.2f, 0.2f, 1.0f);
const glm::vec4 UI::Green       (0.3f, 1.0f, 0.3f, 1.0f);
const glm::vec4 UI::Blue        (0.3f, 0.5f, 1.0f, 1.0f);
const glm::vec4 UI::Yellow      (1.0f, 0.9f, 0.2f, 1.0f);
const glm::vec4 UI::Orange      (1.0f, 0.55f, 0.1f, 1.0f);
const glm::vec4 UI::Gray        (0.6f, 0.6f, 0.6f, 1.0f);
const glm::vec4 UI::Transparent (0.0f, 0.0f, 0.0f, 0.0f);

// Atlas: 16x8 cells of 8x8 pixels = 128x64. Glyph c lives at cell (c % 16, c / 16).
//i was lazy about it
// Cell 0 (NUL, empty in the font) is filled solid white and used for rectangles.
static const int ATLAS_W = 128, ATLAS_H = 64, CELL = 8;

static const char* kVert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec4 aColor;
uniform vec2 screen;
out vec2 uv;
out vec4 color;
void main() {
	uv = aUV;
	color = aColor;
	gl_Position = vec4(aPos.x / screen.x * 2.0 - 1.0, 1.0 - aPos.y / screen.y * 2.0, 0.0, 1.0);
}
)";

static const char* kFrag = R"(#version 330 core
in vec2 uv;
in vec4 color;
uniform sampler2D atlas;
uniform int mode;
out vec4 FragColor;
void main() {
	vec4 t = texture(atlas, uv);
	if (mode == 0) FragColor = vec4(color.rgb, color.a * t.r);
	else           FragColor = color * t;
}
)";

static GLuint Compile(GLenum type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, nullptr, log); std::cout << "UI shader compile failed:\n" << log << std::endl; }
	return s;
}

bool UI::Init(int virtualW, int virtualH) {
	vw = virtualW; vh = virtualH;

	// shader (embedded so there is no file to lose)
	GLuint v = Compile(GL_VERTEX_SHADER, kVert), f = Compile(GL_FRAGMENT_SHADER, kFrag);
	program = glCreateProgram();
	glAttachShader(program, v); glAttachShader(program, f);
	glLinkProgram(program);
	GLint ok; glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok) { char log[512]; glGetProgramInfoLog(program, 512, nullptr, log); std::cout << "UI program link failed:\n" << log << std::endl; return false; }
	glDeleteShader(v); glDeleteShader(f);
	locScreen = glGetUniformLocation(program, "screen");
	locTex    = glGetUniformLocation(program, "atlas");
	locMode   = glGetUniformLocation(program, "mode");

	// font atlas
	std::vector<unsigned char> px(ATLAS_W * ATLAS_H, 0);
	for (int c = 0; c < 128; c++) {
		int cx = (c % 16) * CELL, cy = (c / 16) * CELL;
		for (int row = 0; row < 8; row++)
			for (int col = 0; col < 8; col++) {
				bool on = (c == 0) ? true : ((font8x8_basic[c][row] >> col) & 1);
				px[(cy + row) * ATLAS_W + cx + col] = on ? 255 : 0;
			}
	}
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_W, ATLAS_H, 0, GL_RED, GL_UNSIGNED_BYTE, px.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// dynamic vertex buffer
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
	glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
	glBindVertexArray(0);

	keyNow.assign(NKEYS, 0);
	keyPrev.assign(NKEYS, 0);
	return true;
}

void UI::Delete() {
	glDeleteProgram(program);
	glDeleteTextures(1, &tex);
	for (GLuint t : ownedTextures) glDeleteTextures(1, &t);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

// ---------------------------------------------------------------- frame

void UI::Begin(GLFWwindow* window, float deltaTime) {
	win = window;
	dt = deltaTime;
	verts.clear();
	batches.clear();

	// mouse
	double mx, my; int ww, wh;
	glfwGetCursorPos(window, &mx, &my);
	glfwGetWindowSize(window, &ww, &wh);
	if (ww > 0 && wh > 0) mouse = glm::vec2((float)mx * vw / ww, (float)my * vh / wh);
	bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	mousePressed = down && !mousePrev;
	mouseDown = down;
	mousePrev = down;

	// edge detection
	keyPrev = keyNow;
	const int tracked[] = { GLFW_KEY_ESCAPE, GLFW_KEY_ENTER, GLFW_KEY_SPACE, GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_W, GLFW_KEY_S };
	for (int k : tracked) keyNow[k] = glfwGetKey(window, k) == GLFW_PRESS;

	// age timed texts
	for (Timed& t : timed) t.left -= dt;
	timed.erase(std::remove_if(timed.begin(), timed.end(), [](const Timed& t) { return t.left <= 0.0f; }), timed.end());

	// draw them
	// stacked ones flow down from the top-left, newest at the bottom)
	float stackY = 20.0f;
	for (const Timed& t : timed) {
		glm::vec4 c = t.color;
		c.a *= std::min(1.0f, t.left / 0.5f);
		if (t.stacked) {
			TextShadow(20.0f, stackY, t.msg, t.scale, c);
			stackY += TextHeight(t.msg, t.scale) + 6.0f;
		} else {
			TextShadow(t.x, t.y, t.msg, t.scale, c);
		}
	}
}

void UI::End() {
	if (verts.empty()) return;

	// save the bits of GL state we change
	GLboolean depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND), cull = glIsEnabled(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(program);
	glUniform2f(locScreen, (float)vw, (float)vh);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(locTex, 0);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STREAM_DRAW);
	for (size_t i = 0; i < batches.size(); i++) {
		size_t end = (i + 1 < batches.size()) ? batches[i + 1].first : verts.size();
		if (end == batches[i].first) continue;
		glBindTexture(GL_TEXTURE_2D, batches[i].tex);
		glUniform1i(locMode, batches[i].mode);
		glDrawArrays(GL_TRIANGLES, (GLint)batches[i].first, (GLsizei)(end - batches[i].first));
	}
	glBindVertexArray(0);

	if (depth) glEnable(GL_DEPTH_TEST);
	if (cull)  glEnable(GL_CULL_FACE);
	if (!blend) glDisable(GL_BLEND);
}

bool UI::KeyPressed(int k) const {
	return k >= 0 && k < NKEYS && keyNow[k] && !keyPrev[k];
}

// ---------------------------------------------------------------- primitives

void UI::Use(GLuint texture, int mode) {
	if (batches.empty() || batches.back().tex != texture || batches.back().mode != mode)
		batches.push_back({ texture, mode, verts.size() });
}

void UI::Quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const glm::vec4& c) {
	Vertex a{ x0, y0, u0, v0, c.r, c.g, c.b, c.a };
	Vertex b{ x1, y0, u1, v0, c.r, c.g, c.b, c.a };
	Vertex d{ x1, y1, u1, v1, c.r, c.g, c.b, c.a };
	Vertex e{ x0, y1, u0, v1, c.r, c.g, c.b, c.a };
	verts.push_back(a); verts.push_back(b); verts.push_back(d);
	verts.push_back(a); verts.push_back(d); verts.push_back(e);
}

void UI::Glyph(float x, float y, unsigned char ch, int scale, const glm::vec4& c) {
	if (ch >= 128 || ch == ' ' || ch == 0) return;
	Use(tex, 0);
	float u0 = (ch % 16) * CELL / (float)ATLAS_W, v0 = (ch / 16) * CELL / (float)ATLAS_H;
	float u1 = u0 + CELL / (float)ATLAS_W,        v1 = v0 + CELL / (float)ATLAS_H;
	Quad(x, y, x + CELL * scale, y + CELL * scale, u0, v0, u1, v1, c);
}

void UI::Rect(float x, float y, float w, float h, const glm::vec4& c) {
	// sample the middle of the solid cell 0
	float u = 4.0f / ATLAS_W, v = 4.0f / ATLAS_H;
	Use(tex, 0);
	Quad(x, y, x + w, y + h, u, v, u, v, c);
}

void UI::RectOutline(float x, float y, float w, float h, const glm::vec4& c, float t) {
	Rect(x, y, w, t, c);
	Rect(x, y + h - t, w, t, c);
	Rect(x, y + t, t, h - 2 * t, c);
	Rect(x + w - t, y + t, t, h - 2 * t, c);
}

// ---------------------------------------------------------------- images

UI::Image UI::LoadImage(const std::string& path) {
	Image img;
	int n = 0;
	stbi_set_flip_vertically_on_load(false);
	unsigned char* data = stbi_load(path.c_str(), &img.w, &img.h, &n, 4);
	if (!data) { std::cout << "UI: could not load image: " << path << std::endl; return Image(); }
	glGenTextures(1, &img.id);
	glBindTexture(GL_TEXTURE_2D, img.id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	stbi_image_free(data);
	ownedTextures.push_back(img.id);
	return img;
}

void UI::DrawImage(const Image& img, float x, float y, float w, float h, const glm::vec4& tint) {
	if (!img.Valid()) return;
	Use(img.id, 1);
	Quad(x, y, x + w, y + h, 0.0f, 0.0f, 1.0f, 1.0f, tint);
}

// ---------------------------------------------------------------- text

void UI::Text(float x, float y, const std::string& msg, int scale, const glm::vec4& color) {
	scale = std::max(scale, 1);
	float cx = x, cy = y;
	for (unsigned char ch : msg) {
		if (ch == '\n') { cx = x; cy += CELL * scale; continue; }
		if (ch == '\t') { cx += CELL * scale * 4; continue; }
		Glyph(cx, cy, ch, scale, color);
		cx += CELL * scale;
	}
}

void UI::TextF(float x, float y, int scale, const glm::vec4& color, const char* fmt, ...) {
	char buf[1024];
	va_list args; va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	Text(x, y, buf, scale, color);
}

float UI::TextWidth(const std::string& msg, int scale) const {
	float best = 0.0f, cur = 0.0f;
	for (unsigned char ch : msg) {
		if (ch == '\n') { best = std::max(best, cur); cur = 0.0f; }
		else cur += CELL * std::max(scale, 1) * (ch == '\t' ? 4 : 1);
	}
	return std::max(best, cur);
}

float UI::TextHeight(const std::string& msg, int scale) const {
	int lines = 1;
	for (char ch : msg) if (ch == '\n') lines++;
	return (float)(lines * CELL * std::max(scale, 1));
}

void UI::TextCentered(float cx, float y, const std::string& msg, int scale, const glm::vec4& color) {
	// center each line separately
	size_t start = 0;
	float cy = y;
	while (true) {
		size_t nl = msg.find('\n', start);
		std::string line = msg.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
		Text(cx - TextWidth(line, scale) * 0.5f, cy, line, scale, color);
		if (nl == std::string::npos) break;
		start = nl + 1;
		cy += CELL * std::max(scale, 1);
	}
}

void UI::TextRight(float rx, float y, const std::string& msg, int scale, const glm::vec4& color) {
	size_t start = 0;
	float cy = y;
	while (true) {
		size_t nl = msg.find('\n', start);
		std::string line = msg.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
		Text(rx - TextWidth(line, scale), cy, line, scale, color);
		if (nl == std::string::npos) break;
		start = nl + 1;
		cy += CELL * std::max(scale, 1);
	}
}

void UI::TextShadow(float x, float y, const std::string& msg, int scale, const glm::vec4& color) {
	glm::vec4 s(0.0f, 0.0f, 0.0f, color.a * 0.8f);
	float o = (float)std::max(scale, 1);
	Text(x + o, y + o, msg, scale, s);
	Text(x, y, msg, scale, color);
}

void UI::TextFor(float x, float y, const std::string& msg, float seconds, int scale, const glm::vec4& color) {
	timed.push_back({ msg, x, y, seconds, seconds, scale, color, false });
}

void UI::Notify(const std::string& msg, float seconds, const glm::vec4& color) {
	timed.push_back({ msg, 0.0f, 0.0f, seconds, seconds, 2, color, true });
	if (timed.size() > 32) timed.erase(timed.begin());
}

// ---------------------------------------------------------------- widgets

static bool Inside(const glm::vec2& p, float x, float y, float w, float h) {
	return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
}

bool UI::Button(float x, float y, float w, float h, const std::string& label, int scale) {
	bool hover = Inside(mouse, x, y, w, h);
	TextCentered(x + w * 0.5f, y + (h - TextHeight(label, scale)) * 0.5f, label, scale, hover ? Yellow : White);
	return hover && mousePressed;
}

int UI::Menu(const std::string& title, const std::vector<std::string>& items, int& selected, float centerY) {
	if (items.empty()) return -1;
	int n = (int)items.size();
	selected = std::clamp(selected, 0, n - 1);

	const int   titleScale = 8, itemScale = 4;
	const float itemH = 64.0f, gap = 12.0f, itemW = 560.0f;
	float totalH = n * itemH + (n - 1) * gap;
	float top = centerY - totalH * 0.5f + 60.0f;
	float cx = vw * 0.5f;

	// keyboard
	if (KeyPressed(GLFW_KEY_DOWN) || KeyPressed(GLFW_KEY_S)) selected = (selected + 1) % n;
	if (KeyPressed(GLFW_KEY_UP)   || KeyPressed(GLFW_KEY_W)) selected = (selected + n - 1) % n;

	int picked = -1;
	if (KeyPressed(GLFW_KEY_ENTER) || KeyPressed(GLFW_KEY_SPACE)) picked = selected;

	// mouse (hover moves the selection, click picks)
	for (int i = 0; i < n; i++) {
		float y = top + i * (itemH + gap);
		if (Inside(mouse, cx - itemW * 0.5f, y, itemW, itemH)) {
			static glm::vec2 lastMouse(-1.0f);
// only when the mouse actually moved, so keys still work
			if (mouse != lastMouse) selected = i;
			lastMouse = mouse;
			if (mousePressed) { selected = i; picked = i; }
		}
	}

	// draw
	// just text, plus the cursor texture (or a ">") next to the selected entry
	TextShadow(cx - TextWidth(title, titleScale) * 0.5f, top - 160.0f, title, titleScale, Orange);
	for (int i = 0; i < n; i++) {
		float y = top + i * (itemH + gap);
		bool sel = (i == selected);
		float tw = TextWidth(items[i], itemScale), ty = y + (itemH - itemScale * CELL) * 0.5f;
		TextShadow(cx - tw * 0.5f, ty, items[i], itemScale, sel ? Yellow : White);
		if (sel) {
			if (menuCursor.Valid()) {
				float ih = itemH * 1.2f, iw = ih * menuCursor.w / (float)menuCursor.h;
				DrawImage(menuCursor, cx - tw * 0.5f - iw - 24.0f, y + (itemH - ih) * 0.5f, iw, ih);
			} else {
				TextShadow(cx - tw * 0.5f - itemScale * CELL * 2, ty, ">", itemScale, Yellow);
			}
		}
	}
	return picked;
}
