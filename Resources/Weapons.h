#pragma once
// Weapon system, modelled on Quake 1's weapons.qc.
//
//   Weapons.h     (this file)  the machinery: ammo, selecting, refire timer, W_Attack, helpers like FireBullets.
//   WeaponDefs.h               the weapons themselves: one fire function + one table row each. Edit THAT to
//                              tweak damage / refire / spread, or to add a weapon.
//
//   attack_finished   -> attackFinished   (can't fire again until time reaches it)
//   W_Attack()        -> WeaponSystem::Attack()
//   W_BestWeapon()    -> WeaponSystem::BestWeapon()
//   impulse 1..8      -> number keys 1..9, matched against WeaponDef::slot
//   traceline()       -> Raycast() in Raycast.h, wrapped by TraceLine()
//   FireBullets()     -> WeaponSystem::FireBullets()
//   v_forward/right/up-> forward / right / up   (valid inside a fire function)
//   items / ammo_*    -> owned[] / ammo[]
//
// Fire is on RIGHT MOUSE or F because left mouse is the mouse-look button for now (see Camera.cpp).

#include "Raycast.h"
#include "Camera.h"
#include "Physics.h"
#include "Primitives.h"
#include "shaderClass.h"
#include "Model.h"
#include "Paths.h"
#include <glm/gtc/quaternion.hpp>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <iostream>

						// ---- ammo types ---- //
// AMMO_COUNT must stay LAST, it sizes the ammo arrays. New types go above it.
enum AmmoType { AMMO_NONE = -1, AMMO_SHELLS = 0, AMMO_NAILS, AMMO_COUNT };

// What a shot did. The game decides what to do with it (puff, sound, damage an entity...).
enum class ImpactType { Bullet, Melee };
struct Impact {
	ImpactType type;
	glm::vec3  point;
	glm::vec3  normal;
	float      damage;
	RayHit     hit;
};

class WeaponSystem;
using FireFn = void (*)(WeaponSystem&);

// One row of the weapon table
struct WeaponDef {
	int         id;               	// see WeaponDefs.h enum
	std::string name;
	int         slot;             	// number key that selects it (1..9)
	AmmoType    ammoType;         	// AMMO_NONE = melee, uses no ammo
	int         ammoPerShot;
	float       refire;           	// seconds between shots (attack_finished = time + X)
	FireFn      fire;             	// the W_Fire* function

							// ---- view model ---- //
	//offset = (right, up, forward) from the eye, size = (width, height, length)
	glm::vec3   vmOffset{ 0.25f, -0.22f, 0.5f };
	glm::vec3   vmSize{ 0.08f, 0.08f, 0.5f };
	glm::vec4   vmColor{ 0.5f, 0.5f, 0.5f, 1.0f };
	float       kick = 0.5f;      // kick back!

							// ---- static view model file (optional) ---- //
	// Path is relative to assets/models/, e.g. "weapons/nailgun.glb" (anything assimp reads: glb, gltf, obj, fbx...).
	// Model space: -Z is the muzzle direction, +Y is up, 1 unit = 1 meter (before vmModelScale).
	// vmOffset above still places it relative to the eye. Use scale / rotation to fix a model exported with other axes.
	std::string vmModel;
	float       vmModelScale = 1.0f;
	glm::vec3   vmModelRotation{ 0.0f };   // extra rotation in degrees (pitch, yaw, roll) applied to the model

							// ---- sound (optional) ---- //
	// Path relative to assets/sounds/, e.g. "s_weapons/famas.mp3". Empty = silent.
	// fireSoundMax > 0 cuts the sound off after that many seconds (0 = play the whole file).
	std::string fireSound;
	float       fireSoundMax = 0.0f;
	float       fireSoundVolume = 1.0f;
};

class WeaponSystem {
public:
							// ---- state ---- //
	std::vector<WeaponDef> defs;
	std::vector<bool>      owned;
	int   ammo[AMMO_COUNT] = {};
	int   maxAmmo[AMMO_COUNT] = {};
	int   current = 0;
	float time = 0.0f;               // game time, only advances in Update
	float worldScale = 1.0f;         // match the player's scale: multiplies ranges, view model offset/size
	float attackFinished = 0.0f;

	// ---- input bindings ---- //
	int fireMouseButton = GLFW_MOUSE_BUTTON_RIGHT;
	int fireKey         = GLFW_KEY_F;

	// ---- valid while a fire function runs (v_forward etc.) ---- //
	glm::vec3 origin  = glm::vec3(0.0f);   // eye position
	glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 right   = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 up      = glm::vec3(0.0f, 1.0f, 0.0f);
	const PhysicsWorld* world = nullptr;

	// 	called for every successful hit. 
	//	TODO: Hook up sounds, entity damage here.
	std::function<void(const Impact&)> onImpact;

	// called once per shot that actually fires (ammo spent), before the fire function runs. Hook sounds here.
	std::function<void(const WeaponDef&)> onFire;

	// ---- setup ---- //
	int AddWeapon(const WeaponDef& d) {
		assert(d.id == (int)defs.size() && "WeaponDef::id must match its position in the table");
		defs.push_back(d);
		owned.push_back(false);
		return d.id;
	}
	void Give(int id)                          { if (Valid(id)) owned[id] = true; }
	void GiveAmmo(AmmoType t, int n)           { if (t != AMMO_NONE) ammo[t] = std::min(ammo[t] + n, maxAmmo[t]); }
	const WeaponDef& Current() const           { return defs[current]; }

								// ---- selecting ---- //
	bool HasAmmoFor(const WeaponDef& d) const  { return d.ammoType == AMMO_NONE || ammo[d.ammoType] >= d.ammoPerShot; }

	bool Select(int id) {
		if (!Valid(id) || !owned[id]) return false;
		current = id;
		kickAmount = 0.0f;
		return true;
	}

	// 	Quake's W_BestWeapon 
	//	switch to the highest slot weapon we own and can fire
	//	Used when ammo runs out
	//  I will fix this to be more like Quake's behaviour later where you defne it manulally
	void BestWeapon() {
		int best = -1;
		for (const WeaponDef& d : defs)
			if (owned[d.id] && HasAmmoFor(d) && (best < 0 || d.slot > defs[best].slot)) best = d.id;
		if (best >= 0) current = best;
	}

	// Next / previous weapon in slot order. Skips weapons we don't own or can't fire. Wraps around.
	// dir = +1 next slot, -1 previous slot.
	void CycleWeapon(int dir) {
		std::vector<int> order;                               // ids sorted by slot
		for (const WeaponDef& d : defs) order.push_back(d.id);
		std::sort(order.begin(), order.end(), [&](int a, int b) { return defs[a].slot < defs[b].slot; });
		int n = (int)order.size();
		int pos = (int)(std::find(order.begin(), order.end(), current) - order.begin());
		for (int i = 1; i <= n; i++) {
			int id = order[((pos + dir * i) % n + n) % n];
			if (owned[id] && HasAmmoFor(defs[id])) { Select(id); return; }
		}
	}

	// Enables mouse wheel switching. Call once after creating the window.
	// (Uses GLFW's single scroll callback, so if you add your own later, call weapons.OnScroll(dy) from it instead.)
	void BindScroll(GLFWwindow* window) {
		scrollTarget() = this;
		glfwSetScrollCallback(window, [](GLFWwindow*, double, double dy) { if (scrollTarget()) scrollTarget()->OnScroll(dy); });
	}
	void OnScroll(double dy) { scrollAccum += (float)dy; }

	// ---- W_WeaponFrame) ---- //
	void Update(GLFWwindow* window, const Camera& camera, const PhysicsWorld& physics, float dt) {
		time += dt;
		world = &physics;

		// impulse: number keys pick a weapon by slot
		for (const WeaponDef& d : defs) {
			int key = GLFW_KEY_0 + d.slot;
			bool down = glfwGetKey(window, key) == GLFW_PRESS;
			if (down && !keyWasDown[d.slot]) Select(d.id);
			keyWasDown[d.slot] = down;
		}

		// mouse wheel: one notch = one weapon (call BindScroll(window) once to enable)
		int notches = (int)scrollAccum;
		if (notches != 0) { scrollAccum -= notches; CycleWeapon(notches > 0 ? 1 : -1); }

		bool wantFire = glfwGetMouseButton(window, fireMouseButton) == GLFW_PRESS || glfwGetKey(window, fireKey) == GLFW_PRESS;
		if (wantFire && time >= attackFinished) {
			SetupBasis(camera);
			Attack();
		}
		kickAmount = std::max(0.0f, kickAmount - dt * 6.0f);
	}

	// W_Attack
	void Attack() {
		const WeaponDef& d = defs[current];
		if (!HasAmmoFor(d)) { BestWeapon(); attackFinished = time + 0.2f; return; }
		if (d.ammoType != AMMO_NONE) ammo[d.ammoType] -= d.ammoPerShot;
		attackFinished = time + d.refire;
		kickAmount = d.kick;
		if (onFire) onFire(d);
		if (d.fire) d.fire(*this);
		if (!HasAmmoFor(defs[current])) BestWeapon();
	}

						// ---- fire functions ---- //

	// traceline from the eye along direction
	RayHit TraceLine(const glm::vec3& dir, float range) const { return Raycast(*world, origin, dir, range * worldScale); }

	// FireBullets: 'shots' rays
	// Each randomly spread by up to +-spreadX / +-spreadY (as tangent of the angle).
	void FireBullets(int shots, float spreadX, float spreadY, float damage, float range) {
		for (int i = 0; i < shots; i++) {
			glm::vec3 dir = glm::normalize(forward + right * (Random() * spreadX) + up * (Random() * spreadY));
			RayHit h = TraceLine(dir, range);
			if (h.hit) Report(ImpactType::Bullet, h, damage);
		}
	}

	// One perfectly straight ray along the crosshair, no spread. For nailgun-style / railgun-style weapons.
	void Hitscan(float damage, float range) { FireBullets(1, 0.0f, 0.0f, damage, range); }

	// Short straight trace for melee weapons.
	void Melee(float range, float damage) {
		RayHit h = TraceLine(forward, range);
		if (h.hit) Report(ImpactType::Melee, h, damage);
	}

	// random number in -1..1
	float Random() { return dist(rng); }

											// ---- view model ---- //
	void DrawViewModel(Shader& shader, Camera& camera) {
		if (defs.empty()) return;
		const WeaponDef& d = defs[current];
		SetupBasis(camera);
		glm::vec3 pos = camera.Position + (right * d.vmOffset.x + up * d.vmOffset.y + forward * (d.vmOffset.z - kickAmount * 0.15f)) * worldScale;
		glm::mat4 m(1.0f);
		m[0] = glm::vec4(right, 0.0f);
		m[1] = glm::vec4(up, 0.0f);
		m[2] = glm::vec4(-forward, 0.0f);
		m[3] = glm::vec4(pos, 1.0f);

		glClear(GL_DEPTH_BUFFER_BIT);   	// to never clips into walls

		Model* model = GetViewModel(d);
		if (model) {
			glm::mat4 rot = glm::mat4_cast(glm::quat(glm::radians(d.vmModelRotation)));
			glm::mat4 full = m * rot;
			model->position = pos;
			model->rotation = glm::quat_cast(glm::mat3(full));
			model->scale    = glm::vec3(d.vmModelScale * worldScale);
			model->Draw(shader, camera);
		}
		else {
			if (!viewBox) viewBox = std::make_unique<Mesh>(MakeBoxMesh(glm::vec4(1.0f)));
			viewBox->material.baseColor = d.vmColor;
			glFrontFace(GL_CCW);
			viewBox->Draw(shader, camera, m * glm::scale(glm::mat4(1.0f), d.vmSize * worldScale));
		}
	}

private:
	bool  Valid(int id) const { return id >= 0 && id < (int)defs.size(); }
	float kickAmount = 0.0f;
	bool  keyWasDown[10] = {};
	std::unique_ptr<Mesh> viewBox;
	float scrollAccum = 0.0f;
	static WeaponSystem*& scrollTarget() { static WeaponSystem* t = nullptr; return t; }

	// view model files, loaded the first time a weapon is drawn (needs the GL context)
	std::map<int, std::unique_ptr<Model>> viewModels;
	std::map<int, bool> viewModelTried;
	Model* GetViewModel(const WeaponDef& d) {
		if (d.vmModel.empty()) return nullptr;
		if (!viewModelTried[d.id]) {
			viewModelTried[d.id] = true;
			std::string path = (Paths::Root() / "assets" / "models" / d.vmModel).generic_string();
			auto m = std::make_unique<Model>(path.c_str());
			if (m->MeshCount() > 0) viewModels[d.id] = std::move(m);
			else std::cout << "Weapons: could not load view model '" << path << "', using the box" << std::endl;
		}
		auto it = viewModels.find(d.id);
		return it == viewModels.end() ? nullptr : it->second.get();
	}

	std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist{ -1.0f, 1.0f };

	void SetupBasis(const Camera& camera) {
		origin  = camera.Position;
		forward = glm::normalize(camera.Orientation);
		right   = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));   // ignores camera roll on purpose
		up      = glm::cross(right, forward);
	}

	void Report(ImpactType type, const RayHit& h, float damage) {
		if (onImpact) onImpact(Impact{ type, h.point, h.normal, damage, h });
	}
};


// ---------------------------------------------------------------------------
// 	Raycasting and impack markers
//	Replace with real puffs / decals later.
// ---------------------------------------------------------------------------
class ImpactMarkers {
public:
	float lifetime = 4.0f;

	void Add(const Impact& i) {
		glm::vec4 c = (i.type == ImpactType::Melee) ? glm::vec4(1.0f, 0.2f, 0.2f, 1.0f) : glm::vec4(1.0f, 0.9f, 0.2f, 1.0f);
		markers.push_back({ i.point + i.normal * 0.01f, c, lifetime });
	}

	void Update(float dt) {
		for (Marker& m : markers) m.life -= dt;
		markers.erase(std::remove_if(markers.begin(), markers.end(), [](const Marker& m) { return m.life <= 0.0f; }), markers.end());
	}

	void Draw(Shader& shader, Camera& camera) {
		if (markers.empty()) return;
		if (!box) box = std::make_unique<Mesh>(MakeBoxMesh(glm::vec4(1.0f)));
		glFrontFace(GL_CCW);
		for (const Marker& m : markers) {
			box->material.baseColor = m.color;
			box->Draw(shader, camera, glm::translate(glm::mat4(1.0f), m.pos) * glm::scale(glm::mat4(1.0f), glm::vec3(0.06f)));
		}
	}

private:
	struct Marker { glm::vec3 pos; glm::vec4 color; float life; };
	std::vector<Marker> markers;
	std::unique_ptr<Mesh> box;
};
