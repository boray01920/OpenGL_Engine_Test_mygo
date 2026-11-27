#pragma once

#include "Camera.h"
#include "Physics.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <algorithm>
#include <cmath>


class Player {
public:
    // ---- tuning ----
    float maxSpeed       = 15.0f;
    float groundAccel    = 10.0f;
    float airAccel       = 1.0f;
    float friction       = 6.0f;
    float stopSpeed      = 3.125f;
    float gravity        = 25.0f;
    float jumpVelocity   = 9.4375f;
    bool  bunnyhop       = true;

    bool  cpmAirStrafe   = true; 
    float airStrafeAccel = 90.0f;
    float airStrafeSpeed = 0.9375f;

    float eyeOffset      = 1.6f;
    float bobAmp         = 0.04f;
    float bobFreq        = 0.0f;
    float rollAmount     = 0.0f;
    float rollSmoothing  = 10.0f;

    Body body;

    // scale: 1 = original size. Everything length-based (hull, eye, step, speeds, gravity, jump) is multiplied by it
    // so the movement feels identical, just in a bigger body.
    explicit Player(glm::vec3 feetPos = glm::vec3(0.0f), float scale = 1.0f) {
        maxSpeed *= scale; stopSpeed *= scale; gravity *= scale; jumpVelocity *= scale;
        airStrafeSpeed *= scale; eyeOffset *= scale; bobAmp *= scale;
        body.halfExtents = glm::vec3(0.3f, 0.9f, 0.3f) * scale;
        body.stepHeight  = 0.35f * scale;
        body.useGravity  = false;
        Teleport(feetPos);
    }

    void Teleport(const glm::vec3& feetPos) {
        body.position = feetPos + glm::vec3(0.0f, body.halfExtents.y + 0.001f, 0.0f);
        body.velocity = glm::vec3(0.0f);
        body.onGround = false;
        landDip = 0.0f;
    }

    glm::vec3 FeetPosition() const { return body.position - glm::vec3(0.0f, body.halfExtents.y, 0.0f); }

    void Update(GLFWwindow* window, Camera& camera, PhysicsWorld& world, float dt) {
        if (dt <= 0.0f) return;

        // input ---------
        auto key = [&](int k) { return glfwGetKey(window, k) == GLFW_PRESS; };
        float fwdIn  = (key(GLFW_KEY_W) ? 1.0f : 0.0f) - (key(GLFW_KEY_S) ? 1.0f : 0.0f);
        float sideIn = (key(GLFW_KEY_D) ? 1.0f : 0.0f) - (key(GLFW_KEY_A) ? 1.0f : 0.0f);
        bool jumpHeld = key(GLFW_KEY_SPACE);
        bool jumpWanted = jumpHeld && (bunnyhop || !prevJumpHeld);
        prevJumpHeld = jumpHeld;

        glm::vec3 fwd(camera.Orientation.x, 0.0f, camera.Orientation.z);
        fwd = (glm::length(fwd) > 1e-4f) ? glm::normalize(fwd) : glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));

        glm::vec3 wish = fwd * fwdIn + right * sideIn;
        float wishLen = glm::length(wish);
        glm::vec3 wishDir = (wishLen > 1e-4f) ? wish / wishLen : glm::vec3(0.0f);

        // ---- jump / friction ---- //
        bool jumped = false;
        if (body.onGround && jumpWanted) {
            body.velocity.y = jumpVelocity;
            body.onGround = false;
            jumped = true;
        }

        if (body.onGround && !jumped) ApplyFriction(dt);

        // ---- acceleration ---- //
        if (wishLen > 0.0f) {
            if (body.onGround) {
                Accelerate(wishDir, maxSpeed, groundAccel, dt);
            } else if (cpmAirStrafe && fwdIn == 0.0f && sideIn != 0.0f) {
                Accelerate(wishDir, airStrafeSpeed, airStrafeAccel, dt);
            } else {
                Accelerate(wishDir, maxSpeed, airAccel, dt);
            }
        }

        // ---- gravity + collision ---- //
        float preVy = body.velocity.y;
        bool wasOnGround = body.onGround;
        body.velocity.y -= gravity * dt;
        world.Step(body, dt);

        if (!wasOnGround && body.onGround && preVy < -4.0f)
            landDip = std::min(0.15f, -preVy * 0.012f);

        // ---- camera ---- //
        UpdateCameraEffects(camera, right, dt);
    }

private:
    float bobTime = 0.0f;
    float bobBlend = 0.0f;
    float landDip = 0.0f;
    float rollDeg = 0.0f;
    bool  prevJumpHeld = false;

    void ApplyFriction(float dt) {
        float speed = std::sqrt(body.velocity.x * body.velocity.x + body.velocity.z * body.velocity.z);
        if (speed < 1e-4f) return;
        float control = std::max(speed, stopSpeed);
        float newSpeed = std::max(0.0f, speed - control * friction * dt);
        float scale = newSpeed / speed;
        body.velocity.x *= scale;
        body.velocity.z *= scale;
    }

    void Accelerate(const glm::vec3& wishDir, float wishSpeed, float accel, float dt) {
        float current = glm::dot(glm::vec3(body.velocity.x, 0.0f, body.velocity.z), wishDir);
        float addSpeed = wishSpeed - current;
        if (addSpeed <= 0.0f) return;
        float accelSpeed = std::min(accel * wishSpeed * dt, addSpeed);
        body.velocity.x += wishDir.x * accelSpeed;
        body.velocity.z += wishDir.z * accelSpeed;
    }

    void UpdateCameraEffects(Camera& camera, const glm::vec3& right, float dt) {
        glm::vec2 hv(body.velocity.x, body.velocity.z);
        float hSpeed = glm::length(hv);

        //head bob
        float targetBlend = (body.onGround && hSpeed > 0.5f) ? 1.0f : 0.0f;
        bobBlend += (targetBlend - bobBlend) * std::min(1.0f, 10.0f * dt);
        bobTime += hSpeed * 0.35f * bobFreq * dt;
        float bob = std::sin(bobTime * glm::two_pi<float>()) * bobAmp * bobBlend;

        landDip = std::max(0.0f, landDip - landDip * 8.0f * dt - 0.2f * dt);

        glm::vec3 feet = FeetPosition();
        camera.Position = feet + glm::vec3(0.0f, eyeOffset + bob - landDip, 0.0f);

        //strafe roll dont use it... or i dont remember what this did
        float side = glm::dot(glm::vec3(body.velocity.x, 0.0f, body.velocity.z), right);
        float targetRoll = glm::clamp(side / maxSpeed, -1.0f, 1.0f) * rollAmount;
        rollDeg += (targetRoll - rollDeg) * std::min(1.0f, rollSmoothing * dt);
        camera.Up = glm::rotate(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rollDeg), glm::normalize(camera.Orientation));
    }
};
