#ifndef PHYSICS_H
#define PHYSICS_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

// Axis-aligned bounding box.
struct AABB {
	glm::vec3 min;
	glm::vec3 max;

	glm::vec3 Center() const { return (min + max) * 0.5f; }
	glm::vec3 Size()   const { return max - min; }

	bool Overlaps(const AABB& o) const {
		return min.x < o.max.x && max.x > o.min.x &&
		       min.y < o.max.y && max.y > o.min.y &&
		       min.z < o.max.z && max.z > o.min.z;
	}

	static AABB FromCenter(const glm::vec3& c, const glm::vec3& half) { return { c - half, c + half }; }
};


// One collision triangle (world space). n is the outward unit normal.
struct Tri {
	glm::vec3 a, b, c;
	glm::vec3 n;
	AABB box;
};

// Builds a triangle whose normal points the same way as 'outwardHint' (pass vec3(0) to keep the winding).
inline bool MakeTri(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& outwardHint, Tri& out) {
	glm::vec3 n = glm::cross(b - a, c - a);
	float len = glm::length(n);
	if (len < 1e-8f) return false;
	n /= len;
	out.a = a; out.b = b; out.c = c;
	if (glm::dot(outwardHint, outwardHint) > 1e-12f && glm::dot(n, outwardHint) < 0.0f) { std::swap(out.b, out.c); n = -n; }
	out.n = n;
	out.box = { glm::min(a, glm::min(b, c)), glm::max(a, glm::max(b, c)) };
	return true;
}

struct Body {
	glm::vec3 position  = glm::vec3(0.0f);
	glm::vec3 halfExtents = glm::vec3(0.5f);
	glm::vec3 velocity  = glm::vec3(0.0f);

	bool  onGround    = false;
	float restitution = 0.0f;
	float stepHeight  = 0.0f;
	bool  useGravity = true;

	AABB Bounds() const { return AABB::FromCenter(position, halfExtents); }
};


class PhysicsWorld {
public:
	std::vector<AABB> statics;
	glm::vec3 gravity = glm::vec3(0.0f, -25.0f, 0.0f);

	// Sloped / non-box geometry. Surfaces up to maxSlopeDeg can be walked on, steeper ones act as walls.
	std::vector<Tri> tris;
	float maxSlopeDeg = 44.0f;

	void Step(Body& b, float dt) const {
		if (b.useGravity) b.velocity += gravity * dt;

		float maxMove = std::max({ std::fabs(b.velocity.x), std::fabs(b.velocity.y), std::fabs(b.velocity.z) }) * dt;
		int steps = std::max(1, (int)std::ceil(maxMove / 0.1f));
		float h = (dt / steps);

		bool grounded = false;
		for (int i = 0; i < steps; i++) {
			bool wasGrounded = grounded || (i == 0 && b.onGround);

			MoveAxis(b, 0, b.velocity.x * h, wasGrounded);
			MoveAxis(b, 2, b.velocity.z * h, wasGrounded);
			MoveAxis(b, 1, b.velocity.y * h, false);
			if (m_hitGround) grounded = true;
			if (!tris.empty()) ResolveTris(b, wasGrounded, grounded, h);
		}
		b.onGround = grounded;
	}

	bool OverlapsAny(const AABB& a) const {
		for (const AABB& s : statics) if (a.Overlaps(s)) return true;
		return false;
	}

private:
	mutable bool m_hitGround = false;
	static constexpr float kSkin = 0.005f;   // penetration smaller than this is treated as plain contact

	// ---------------- triangle collision ---------------- //
	static bool AxisSeparates(glm::vec3 axis, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& h) {
		float l2 = glm::dot(axis, axis);
		if (l2 < 1e-12f) return false;
		axis /= std::sqrt(l2);
		float p0 = glm::dot(v0, axis), p1 = glm::dot(v1, axis), p2 = glm::dot(v2, axis);
		float mn = std::min({ p0, p1, p2 }), mx = std::max({ p0, p1, p2 });
		float r = h.x * std::fabs(axis.x) + h.y * std::fabs(axis.y) + h.z * std::fabs(axis.z);
		const float eps = 1e-4f;   // touching does not count as overlapping
		return mn >= r - eps || mx <= -r + eps;
	}

	static bool TriOverlapsBox(const AABB& box, const Tri& t) {
		glm::vec3 c = box.Center(), h = box.Size() * 0.5f;
		glm::vec3 v0 = t.a - c, v1 = t.b - c, v2 = t.c - c;
		glm::vec3 e[3] = { v1 - v0, v2 - v1, v0 - v2 };
		const glm::vec3 u[3] = { glm::vec3(1,0,0), glm::vec3(0,1,0), glm::vec3(0,0,1) };
		for (int i = 0; i < 3; i++) if (AxisSeparates(u[i], v0, v1, v2, h)) return false;
		if (AxisSeparates(t.n, v0, v1, v2, h)) return false;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				if (AxisSeparates(glm::cross(u[i], e[j]), v0, v1, v2, h)) return false;
		return true;
	}

	// Height of the triangle's plane at (x,z) if that point is inside it (seen from above).
	static bool HeightAt(const Tri& t, float x, float z, float& y) {
		float d = (t.b.z - t.c.z) * (t.a.x - t.c.x) + (t.c.x - t.b.x) * (t.a.z - t.c.z);
		if (std::fabs(d) < 1e-9f) return false;
		float l1 = ((t.b.z - t.c.z) * (x - t.c.x) + (t.c.x - t.b.x) * (z - t.c.z)) / d;
		float l2 = ((t.c.z - t.a.z) * (x - t.c.x) + (t.a.x - t.c.x) * (z - t.c.z)) / d;
		float l3 = 1.0f - l1 - l2;
		const float eps = 1e-4f;
		if (l1 < -eps || l2 < -eps || l3 < -eps) return false;
		y = t.a.y - (t.n.x * (x - t.a.x) + t.n.z * (z - t.a.z)) / t.n.y;
		return true;
	}

	void ResolveTris(Body& b, bool wasGrounded, bool& grounded, float h) const {
		const float walkY = std::cos(maxSlopeDeg * 3.14159265f / 180.0f);

		// Walls first, THEN the ground snap, then walls again. If the snap ran first, a body pressed into a wall
		// (or hugging the side of a ramp) would get lifted by the ramp under the part that is inside the wall.
		PushOutSteep(b, walkY);
		SnapToWalkable(b, wasGrounded, grounded, h, walkY);
		PushOutSteep(b, walkY);
	}

	void SnapToWalkable(Body& b, bool wasGrounded, bool& grounded, float h, float walkY) const {
		// ---- walkable surfaces: follow the surface height under the feet ---- //
		const glm::vec3 hx = b.halfExtents;
		float feetY  = b.position.y - hx.y;
		// Only grounded bodies may climb a step. In the air we still allow what this substep's movement could have
		// sunk us into a slope (horizontal travel * tan(slope), slope < 45deg) plus the fall distance.
		float hdist = std::sqrt(b.velocity.x * b.velocity.x + b.velocity.z * b.velocity.z) * h;
		float snapUp = 0.12f + hdist * 1.2f + (wasGrounded ? b.stepHeight : 0.0f) + std::max(0.0f, -b.velocity.y) * h;
		float snapDown = (wasGrounded && b.velocity.y <= 0.0f) ? std::max(b.stepHeight, 0.12f) : 0.0f;

		// Sample points sit slightly inside the box. A body that merely touches the edge of a ramp (standing
		// next to it) must not count as standing on it, or it gets lifted up the ramp's side.
		const float inset = 0.02f;
		const float ix = std::max(0.0f, hx.x - inset), iz = std::max(0.0f, hx.z - inset);
		const float sx[5] = { 0.0f,  ix, -ix,  ix, -ix };
		const float sz[5] = { 0.0f,  iz,  iz, -iz, -iz };
		bool  found = false;
		float best = -1e30f;
		for (const Tri& t : tris) {
			if (t.n.y < walkY) continue;
			if (t.box.max.x < b.position.x - hx.x || t.box.min.x > b.position.x + hx.x) continue;
			if (t.box.max.z < b.position.z - hx.z || t.box.min.z > b.position.z + hx.z) continue;
			for (int i = 0; i < 5; i++) {
				float y;
				if (!HeightAt(t, b.position.x + sx[i], b.position.z + sz[i], y)) continue;
				if (y > feetY + snapUp || y < feetY - snapDown) continue;
				if (y > best) { best = y; found = true; }
			}
		}
		if (found) {
			// If the snapped position would be inside a box we are standing on (e.g. we just stepped from the top
			// of a ramp onto the flat floor it leads to), the box is the real ground. The ramp triangles under our
			// rear foot samples must not pull us back down into it.
			AABB snapped = AABB::FromCenter(glm::vec3(b.position.x, best + hx.y, b.position.z), hx);
			const glm::vec3 sk(kSkin);
			bool blocked = false;
			for (const AABB& s : statics)
				if (AABB{ snapped.min + sk, snapped.max - sk }.Overlaps(s)) { blocked = true; break; }

			if (!blocked) {
				if (b.velocity.y <= 0.0f) {
					b.position.y = best + hx.y;
					b.velocity.y = 0.0f;
					grounded = true;
				} else if (feetY < best) {
					b.position.y = best + hx.y;   // moving up (jumping): just don't sink into the surface
				}
			}
		}

	}

	// ---- steep surfaces and ceilings: block ---- //
	void PushOutSteep(Body& b, float walkY) const {
		const glm::vec3 hx = b.halfExtents;
		for (const Tri& t : tris) {
			if (t.n.y >= walkY) continue;
			AABB bb = b.Bounds();
			if (!bb.Overlaps(t.box)) continue;
			if (!TriOverlapsBox(bb, t)) continue;

			glm::vec3 c = bb.Center();
			float r = hx.x * std::fabs(t.n.x) + hx.y * std::fabs(t.n.y) + hx.z * std::fabs(t.n.z);
			float dist = glm::dot(c - t.a, t.n);
			if (dist < 0.0f) continue;
			float pen = r - dist;                            	// penetration of the box into the triangle's plane
			if (pen <= 0.0f) continue;
			pen = std::min(pen, 0.25f);

			if (t.n.y < -walkY) {                            	// ceiling
				b.position += t.n * pen;
				if (b.velocity.y > 0.0f) b.velocity.y = 0.0f;
			} else {                                         	//wall or too steep: push out horizontally only, so it can't be climbed
				glm::vec3 nh(t.n.x, 0.0f, t.n.z);
				float len = glm::length(nh);
				if (len < 1e-4f) {continue;}
				nh /= len;
				b.position += nh * (pen / len);
				float vn = b.velocity.x * nh.x + b.velocity.z * nh.z;
				if (vn < 0.0f) { b.velocity.x -= nh.x * vn; b.velocity.z -= nh.z * vn; }
			}
		}
	}

	bool MoveAxis(Body& b, int axis, float delta, bool allowStep) const {
		if (axis == 1) m_hitGround = false;
		if (delta == 0.0f) return false;

		glm::vec3 before = b.position;
		bool hit = false;
		b.position[axis] += delta;

		// Sliding contact on the OTHER axes (resting on a box top while walking sideways, ramp/plateau seams) is
		// not a hit. The axis we are moving along gets no skin, otherwise a body resting on the floor that falls
		// a few mm per substep would never register the floor and onGround would flicker (no friction).
		glm::vec3 skinOther(kSkin); skinOther[axis] = 0.0f;
		for (const AABB& s : statics) {
			AABB now = b.Bounds();
			if (!AABB{ now.min + skinOther, now.max - skinOther }.Overlaps(s)) continue;
			// We may only be stopped by the face we are actually approaching: moving +, we must have started on the
			// box's low side; moving -, on its high side. Otherwise (already inside it, or leaving it) snapping to
			// that face would throw the body across the whole box. Only block moves that go deeper.
			const float he = b.halfExtents[axis], side = 0.05f;
			bool fromFront = delta > 0.0f ? (before[axis] + he <= s.min[axis] + side)
			                              : (before[axis] - he >= s.max[axis] - side);
			if (!fromFront) {
				bool deeper = delta > 0.0f ? s.Center()[axis] > before[axis] : s.Center()[axis] < before[axis];
				if (deeper) b.position[axis] = before[axis];
				continue;
			}
			hit = true;
			if (delta > 0.0f) b.position[axis] = s.min[axis] - b.halfExtents[axis];
			else              b.position[axis] = s.max[axis] + b.halfExtents[axis];
		}

		if (!hit) return false;

		// Stair and ledge stepping: retry the same horizontal move from higher up.
		if (axis != 1 && allowStep && b.stepHeight > 0.0f) {
			glm::vec3 slid = b.position;
			b.position = before;
			b.position.y += b.stepHeight;
			if (!OverlapsAny(b.Bounds())) {
				b.position[axis] += delta;
				if (!OverlapsAny(b.Bounds())) {
					// Settle back down onto the step.
					b.position.y -= b.stepHeight;
					for (const AABB& s : statics)
						if (b.Bounds().Overlaps(s)) b.position.y = s.max.y + b.halfExtents.y;
					return false;   //no horizontal hit
				}
			}
			b.position = slid;
		}

		//Collision response
		if (axis == 1 && delta < 0.0f) m_hitGround = true;
		float v = b.velocity[axis];
		if ((delta > 0.0f && v > 0.0f) || (delta < 0.0f && v < 0.0f)) {
			float bounced = -v * b.restitution;
			b.velocity[axis] = (std::fabs(bounced) < 1.0f) ? 0.0f : bounced;   //tiny bounces
		}
		return true;
	}
};

#endif
