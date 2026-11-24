#pragma once
// Level: a map (.obj/.mtl) split into named objects, each with a type that decides
// whether it is drawn and whether it collides. Plus hand-placed boxes.
//
//   Solid   : drawn + collides              (walls, floors, crates)
//   Decor   : drawn, NO collision           (details, grates, fake windows)
//   Clip    : collides, NOT drawn           (invisible walls / player clip)
//   Trigger : neither - just a named volume (query with Level::Touching)
//
// Type of a map object is resolved in this order:  object name rule > material rule > default.
// Rules come from code (SetType / SetMaterialType / SetDefaultType) or a text file (LoadRules):
//
//     # comment
//     default solid
//     material sidewalk decor
//     entity0_brush3 clip
//     entity0_brush7 trigger
//
// Collision: one AABB per object when it is a box, exact triangles for anything else (slopes, pyramids, ...).
// Triangle surfaces up to PhysicsWorld::maxSlopeDeg are walkable, steeper ones are walls.

#include "Model.h"
#include "Physics.h"
#include "Primitives.h"
#include "Paths.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <iostream>

enum class ObjectType { Solid, Decor, Clip, Trigger };

inline bool ParseObjectType(std::string s, ObjectType& out) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	if (s == "solid")        out = ObjectType::Solid;
	else if (s == "decor")   out = ObjectType::Decor;
	else if (s == "clip")    out = ObjectType::Clip;
	else if (s == "trigger") out = ObjectType::Trigger;
	else return false;
	return true;
}

struct LevelObject {
	std::string name;
	std::string material;                  // first material (map objects only)
	ObjectType  type = ObjectType::Solid;
	AABB        bounds{};                  // world space
	std::vector<size_t> meshes;            // indices into the map Model (empty for hand-placed boxes)
	bool        exactBox = true;           // false = slopes / odd shapes: collides with its triangles instead of its bounds
	std::vector<Tri> tris;                 // world space, only kept when !exactBox
	glm::vec4   color = glm::vec4(1.0f);   // only used by hand-placed boxes

	bool Collides() const { return type == ObjectType::Solid || type == ObjectType::Clip; }
	bool Visible()  const { return type == ObjectType::Solid || type == ObjectType::Decor; }
	bool IsBox()    const { return meshes.empty(); }
};

class Level {
public:
	ObjectType defaultType = ObjectType::Solid;
	std::vector<LevelObject> objects;

	// Load assets/maps/map_<number>/ : the .obj inside it, plus the .rules file next to it (if any).
	// Textures are found through the .mtl, relative to that same folder (map_<number>/textures/...).
	bool LoadLevel(int number, glm::vec3 position = glm::vec3(0.0f), float scale = 1.0f / 32.0f) {
		std::string obj = Paths::MapFile(number, ".obj");
		if (obj.empty()) { std::cout << "Level: no .obj found for map_" << number << std::endl; return false; }
		LoadMap(obj.c_str(), position, scale);
		std::string rules = Paths::MapFile(number, ".rules");
		if (!rules.empty()) LoadRules(rules.c_str());
		return true;
	}

	// Load a map
	// Converts it to real life units since Quake Trenchbroom's units are 32 times the size. right here
	void LoadMap(const char* file, glm::vec3 position = glm::vec3(0.0f), float scale = 1.0f / 32.0f) {
		map = std::make_unique<Model>(file);
		map->position = position;
		map->scale = glm::vec3(scale);

		glm::mat4 world = map->GetModelTransform();
		std::map<std::string, size_t> byName;

		for (size_t i = 0; i < map->MeshCount(); i++) {
			const Mesh& mesh = map->GetMesh(i);
			glm::mat4 m = world * map->GetMeshTransform(i);

			const std::string& name = map->GetMeshName(i);
			auto it = byName.find(name);
			if (it == byName.end()) {
				LevelObject o;
				o.name = name;
				o.material = mesh.material.name;
				o.bounds = { glm::vec3(1e30f), glm::vec3(-1e30f) };
				objects.push_back(o);
				it = byName.emplace(name, objects.size() - 1).first;
			}
			LevelObject& o = objects[it->second];
			o.meshes.push_back(i);
			glm::mat3 nm = glm::mat3(m);
			for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3) {
				const Vertex& va = mesh.vertices[mesh.indices[k]];
				const Vertex& vb = mesh.vertices[mesh.indices[k + 1]];
				const Vertex& vc = mesh.vertices[mesh.indices[k + 2]];
				glm::vec3 hint = nm * (va.normal + vb.normal + vc.normal);
				Tri t;
				if (MakeTri(glm::vec3(m * glm::vec4(va.position, 1.0f)), glm::vec3(m * glm::vec4(vb.position, 1.0f)),
				            glm::vec3(m * glm::vec4(vc.position, 1.0f)), hint, t)) o.tris.push_back(t);
			}
			for (const Vertex& v : mesh.vertices) {
				glm::vec3 p = glm::vec3(m * glm::vec4(v.position, 1.0f));
				o.bounds.min = glm::min(o.bounds.min, p);
				o.bounds.max = glm::max(o.bounds.max, p);
			}
		}
		// A brush that fills its bounding box is a plain box (fast AABB). Anything else keeps its triangles.
		for (LevelObject& o : objects) {
			glm::vec3 sz = o.bounds.Size();
			float boxVol = sz.x * sz.y * sz.z;
			float vol = 0.0f;
			for (const Tri& t : o.tris) vol += glm::dot(t.a, glm::cross(t.b, t.c)) / 6.0f;
			o.exactBox = boxVol < 1e-9f || std::fabs(vol) >= boxVol * 0.995f;
			if (o.exactBox) { o.tris.clear(); o.tris.shrink_to_fit(); }
		}
		std::cout << "Level: loaded " << objects.size() << " objects from " << file << std::endl;
		ApplyRules();
	}

	// Hand-placed axis aligned box (needs no file). Keeps the type it is given unless a name rule overrides it.
	size_t AddBox(const std::string& name, glm::vec3 mn, glm::vec3 mx,
	              ObjectType type = ObjectType::Solid, glm::vec4 color = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)) {
		LevelObject o;
		o.name = name; o.type = type; o.bounds = { mn, mx }; o.color = color;
		objects.push_back(o);
		return objects.size() - 1;
	}

	// ---- rules ---- //
	void SetType(const std::string& objectName, ObjectType t)       { nameRules[objectName]		= t; ApplyRules(); }
	void SetMaterialType(const std::string& material, ObjectType t) { materialRules[material]	= t; ApplyRules(); }
	void SetDefaultType(ObjectType t)                               { defaultType				= t; ApplyRules(); }

	bool LoadRules(const char* file) {
		std::ifstream in(file);
		if (!in) { std::cout << "Level: no rules file '" << file << "' (using defaults)" << std::endl; return false; }
		std::string line;
		while (std::getline(in, line)) {
			size_t hash = line.find('#');
			if (hash != std::string::npos) line.erase(hash);
			std::istringstream ss(line);
			std::string a, b, c;
			if (!(ss >> a >> b)) continue;
			ObjectType t;
			if (a == "default")       { if (ParseObjectType(b, t)) defaultType = t; else Warn(line); }
			else if (a == "material") { if ((ss >> c) && ParseObjectType(c, t)) materialRules[b] = t; else Warn(line); }
			else                      { if (ParseObjectType(b, t)) nameRules[a] = t; else Warn(line); }
		}
		ApplyRules();
		return true;
	}

	LevelObject* Find(const std::string& name) {
		for (LevelObject& o : objects) if (o.name == name) return &o;
		return nullptr;
	}

	// ---- physics / queries ---- //
	void BuildPhysics(PhysicsWorld& world) const {
		world.statics.clear();
		world.tris.clear();
		for (const LevelObject& o : objects) {
			if (!o.Collides()) continue;
			if (o.exactBox) world.statics.push_back(o.bounds);
			else            world.tris.insert(world.tris.end(), o.tris.begin(), o.tris.end());
		}
	}

	//this box touching the named trigger volume?
	bool Touching(const std::string& triggerName, const AABB& box) const {
		for (const LevelObject& o : objects)
			if (o.name == triggerName && o.type == ObjectType::Trigger && o.bounds.Overlaps(box)) return true;
		return false;
	}

	// ---- drawing ---- //
	void Draw(Shader& shader, Camera& camera) {
		for (LevelObject& o : objects) {
			if (!o.Visible()) continue;
			if (o.IsBox()) {
				if (!boxMesh) boxMesh = std::make_unique<Mesh>(MakeBoxMesh(glm::vec4(1.0f)));
				boxMesh->material.baseColor = o.color;
				glFrontFace(GL_CCW);
				boxMesh->Draw(shader, camera, glm::translate(glm::mat4(1.0f), o.bounds.Center()) * glm::scale(glm::mat4(1.0f), o.bounds.Size()));
			}
			else for (size_t i : o.meshes) map->DrawMesh(i, shader, camera);
		}
	}

	void PrintObjects() const {
		static const char* names[] = { "solid", "decor", "clip", "trigger" };
		for (const LevelObject& o : objects)
			std::cout << "  " << o.name << "  [" << names[(int)o.type] << "]  material=" << o.material
			          << "  min(" << o.bounds.min.x << "," << o.bounds.min.y << "," << o.bounds.min.z << ")"
			          << " max(" << o.bounds.max.x << "," << o.bounds.max.y << "," << o.bounds.max.z << ")"
			          << (o.exactBox ? "" : "  [triangles]") << "\n";
	}

private:
	std::unique_ptr<Model> map;
	std::unique_ptr<Mesh>  boxMesh;
	std::map<std::string, ObjectType> nameRules, materialRules;

	static void Warn(const std::string& line) { std::cout << "Level: bad rule line: " << line << std::endl; }

	void ApplyRules() {
		for (LevelObject& o : objects) {
			auto n = nameRules.find(o.name);
			if (n != nameRules.end()) { o.type = n->second; continue; }
			if (o.IsBox()) continue;                       //hand placed boxes
			auto m = materialRules.find(o.material);
			o.type = (m != materialRules.end()) ? m->second : defaultType;
		}
	}
};
