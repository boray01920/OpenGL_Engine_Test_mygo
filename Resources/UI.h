#pragma once
// UI 
//
//   Coordinates are VIRTUAL PIXELS, origin = top-left, +y goes down.
//   glm::vec4 (r,g,b,a) 0..1. 
//   Scale is an integer multiplier of the 8x8 font (1 = tiny, 2 = 16px, 3 = 24px sand so on).
//
//   Per frame:
//       ui.Begin(window);          // polls mouse / keys, clears the batch
//       ... ui.Text(...), ui.Menu(...), etc ...
//       ui.End();                  // draws everything (call after the 3D stuff, before swap)
//
//   Examples:
//       ui.Text(20, 20, "Hello");                                       // white, scale 2
//       ui.Text(20, 60, "Health: 100", 3, UI::Red);                     // bigger + red
//       ui.TextF(20, 100, 2, UI::Yellow, "Ammo: %d / %d", a, b);        // printf style
//       ui.TextCentered(960, 100, "QUAKE PRO", 6, UI::Orange);          // centered on x
//       ui.TextRight(1900, 20, "60 FPS");                               // right aligned to x
//       ui.TextShadow(...)                                              // text with a drop shadow
//       ui.Notify("Picked up shotgun", 3.0f);                           // message in the corner that fades out
//       ui.TextFor(x, y, "Secret found!", 2.0f);                        // text at a fixed spot for N seconds
//       ui.Rect(x, y, w, h, color);  ui.RectOutline(x, y, w, h, color, thickness);
//       UI::Image img = ui.LoadImage("path/to/file.png");  ui.DrawImage(img, x, y, w, h);   // textures (png/tga/jpg...)
//       ui.menuCursor = ui.LoadImage(...);                              // texture shown next to the hovered menu entry
//       int hit = ui.Menu("MAIN MENU", {"Play","Options","Quit"}, selected);   // -1 = nothing chosen this frame

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdarg>

class UI {
public:
						// ---- colors ---- //
	static const glm::vec4 White, Black, Red, Green, Blue, Yellow, Orange, Gray, Transparent;

	// A texture you can draw with DrawImage. id == 0 means "failed to load / not loaded".
	struct Image { GLuint id = 0; int w = 0, h = 0; bool Valid() const { return id != 0; } };

						// ---- setup ---- //
	// virtualW/H = the coordinate space you draw in. Needs a current OpenGL context.
	bool Init(int virtualW, int virtualH);
	void Delete();

						// ---- frame ---- //
	void Begin(GLFWwindow* window, float deltaTime);   // call once per frame, first
	void End();                                         // call once per frame, last (draws)

						// ---- text ---- //
	void Text        (float x, float y, const std::string& msg, int scale = 2, const glm::vec4& color = White);
	void TextF       (float x, float y, int scale, const glm::vec4& color, const char* fmt, ...);
	void TextCentered(float cx, float y, const std::string& msg, int scale = 2, const glm::vec4& color = White);
	void TextRight   (float rx, float y, const std::string& msg, int scale = 2, const glm::vec4& color = White);
	void TextShadow  (float x, float y, const std::string& msg, int scale = 2, const glm::vec4& color = White);
	float TextWidth  (const std::string& msg, int scale = 2) const;   // handles '\n' (widest line)
	float TextHeight (const std::string& msg, int scale = 2) const;   // handles '\n'

	// *EVENT* that lasts for a few seconds
	void TextFor(float x, float y, const std::string& msg, float seconds, int scale = 2, const glm::vec4& color = White);
	// new notifs go to bottom
	void Notify(const std::string& msg, float seconds = 3.0f, const glm::vec4& color = White);

						// ---- images ---- //
	Image LoadImage(const std::string& path);   // returns an invalid Image (and prints why) if the file is missing
	void  DrawImage(const Image& img, float x, float y, float w, float h, const glm::vec4& tint = White);

						// ---- shapes ---- //
	void Rect       (float x, float y, float w, float h, const glm::vec4& color);
	void RectOutline(float x, float y, float w, float h, const glm::vec4& color, float thickness = 2.0f);

						// ---- widgets ---- //
	// true the frame the button is clicked (left mouse). Hovered buttons get highlighted.
	bool Button(float x, float y, float w, float h, const std::string& label, int scale = 3);

	// Shown to the left of the hovered/selected menu entry. If it isn't loaded, a text ">" is used instead.
	Image menuCursor;

	int Menu(const std::string& title, const std::vector<std::string>& items, int& selected, float centerY = 540.0f);

	// input state
	glm::vec2 mouse{ 0.0f };
	bool mouseDown = false;
	bool mousePressed = false;          
	bool KeyPressed(int glfwKey) const;

	int Width() const  { return vw; }
	int Height() const { return vh; }

private:
	struct Vertex { float x, y, u, v, r, g, b, a; };
	struct Batch  { GLuint tex; int mode; size_t first; };   // mode 0 = font atlas (alpha from red), 1 = plain image
	struct Timed  { std::string msg; float x, y, left, total; int scale; glm::vec4 color; bool stacked; };

	GLuint vao = 0, vbo = 0, tex = 0, program = 0;
	GLint  locScreen = -1, locTex = -1, locMode = -1;
	std::vector<GLuint> ownedTextures;
	std::vector<Batch>  batches;
	void Use(GLuint texture, int mode);
	int vw = 1920, vh = 1080;
	std::vector<Vertex> verts;
	std::vector<Timed>  timed;
	float dt = 0.0f;

	GLFWwindow* win = nullptr;
	static constexpr int NKEYS = GLFW_KEY_LAST + 1;
	std::vector<char> keyNow, keyPrev;
	bool mousePrev = false;

	void Quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const glm::vec4& c);
	void Glyph(float x, float y, unsigned char ch, int scale, const glm::vec4& c);
};
