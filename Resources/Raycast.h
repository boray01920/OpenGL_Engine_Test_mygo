#pragma once
// Raycast: the equivalent of Quake's traceline().
//
//   RayHit h = Raycast(world, origin, direction, maxDistance);
//   if (h.hit) { h.point, h.normal, h.dist, h.kind }
//
// Tests every AABB in PhysicsWorld::statics and every triangle in PhysicsWorld::tris,
// so it sees exactly what the player collides with (Solid + Clip level objects).
// Units are meters same as the rest of the game.
//
//TODO: To trace against entities later, run RayVsAABB (below) over their boxes and keep the closest hit.

#include "Physics.h"
#include <glm/glm.hpp>
//#include <limits>

enum class HitKind { None, Box, Triangle };

struct RayHit {
	bool      hit  = false;
	float     dist = 0.0f;                  // distance from origin to the hit
	glm::vec3 point  = glm::vec3(0.0f);     // world space hit position
	glm::vec3 normal = glm::vec3(0.0f);     // surface normal, always facing the ray origin
	HitKind   kind = HitKind::None;
};

// Ray vs box (slab method). A ray starting inside the box does not hit it.
inline bool RayVsAABB(const glm::vec3& o, const glm::vec3& d, const AABB& box, float maxDist, float& tOut, glm::vec3& nOut) {
	float tMin = 0.0f, tMax = maxDist;
	int   axisHit = -1;
	float signHit = 0.0f;
	for (int i = 0; i < 3; i++) {
		if (std::fabs(d[i]) < 1e-9f) {
			if (o[i] < box.min[i] || o[i] > box.max[i]) return false;   // parallel and outside the slab
			continue;
		}
		float inv = 1.0f / d[i];
		float t1 = (box.min[i] - o[i]) * inv;
		float t2 = (box.max[i] - o[i]) * inv;
		float s = -1.0f;                         // entering through the min face -> normal points -axis
		if (t1 > t2) { std::swap(t1, t2); s = 1.0f; }
		if (t1 > tMin) { tMin = t1; axisHit = i; signHit = s; }
		tMax = std::min(tMax, t2);
		if (tMin > tMax) return false;
	}
	if (axisHit < 0) return false;               // started inside
	tOut = tMin;
	nOut = glm::vec3(0.0f);
	nOut[axisHit] = signHit;
	return true;
}

// Ray vs triangle (Moller-Trumbore), two sided.
inline bool RayVsTri(const glm::vec3& o, const glm::vec3& d, const Tri& t, float maxDist, float& tOut) {
	glm::vec3 e1 = t.b - t.a, e2 = t.c - t.a;
	glm::vec3 p = glm::cross(d, e2);
	float det = glm::dot(e1, p);
	if (std::fabs(det) < 1e-9f) return false;
	float inv = 1.0f / det;
	glm::vec3 s = o - t.a;
	float u = glm::dot(s, p) * inv;
	if (u < 0.0f || u > 1.0f) return false;
	glm::vec3 q = glm::cross(s, e1);
	float v = glm::dot(d, q) * inv;
	if (v < 0.0f || u + v > 1.0f) return false;
	float dist = glm::dot(e2, q) * inv;
	if (dist < 0.0f || dist > maxDist) return false;
	tOut = dist;
	return true;
}

// Closest hit along the ray. 'dir' does not need to be normalized.
inline RayHit Raycast(const PhysicsWorld& world, glm::vec3 origin, glm::vec3 dir, float maxDist) {
	RayHit best;
	float len = glm::length(dir);
	if (len < 1e-8f) return best;
	dir /= len;
	best.dist = maxDist;

	for (const AABB& box : world.statics) {
		float t; glm::vec3 n;
		if (RayVsAABB(origin, dir, box, best.dist, t, n)) {
			best.hit = true; best.dist = t; best.normal = n; best.kind = HitKind::Box;
		}
	}
	for (const Tri& tri : world.tris) {
		float t;
		if (RayVsTri(origin, dir, tri, best.dist, t)) {
			best.hit = true; best.dist = t; best.kind = HitKind::Triangle;
			best.normal = (glm::dot(tri.n, dir) < 0.0f) ? tri.n : -tri.n;
		}
	}
	if (best.hit) best.point = origin + dir * best.dist;
	else          best.dist = maxDist;
	return best;
}
