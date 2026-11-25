#pragma once
// The weapons themselves, I define them here
//
// To add a weapon:
//   	1. add its name to the WeaponId enum (order = table order)
//   	2. write a W_Fire_<Name>(WeaponSystem& w) function
//   	3. add one AddWeapon({...}) row in RegisterWeapons()
// 
//
//   	w.origin / w.forward / w.right / w.up       (Quake's v_forward stuff)
//   	w.FireBullets(shots, spreadX, spreadY, damage, range)
//   	w.Melee(range, damage)
//   	w.TraceLine(dir, range)                      raw raycast for custom behaviour
//
// Units are meters (Quake's 32 units = 1 m): axe reach 64u = 2 m, shotgun range 2048u = 64 m.

#include "Weapons.h"

enum WeaponId { WEAPON_AXE = 0, WEAPON_SHOTGUN, WEAPON_NAILGUN, WEAPON_COUNT };


// ---------------- fire functions ---------------- //

// traceline 64 units, damage 20, refire 0.3s
inline void W_Fire_Axe(WeaponSystem& w) {
	w.Melee(2.0f, 20.0f);
}

// 6 pellets, spread 0.04 x 0.04, 4 damage each, refire 0.5s
inline void W_Fire_Shotgun(WeaponSystem& w) {
	w.FireBullets(6, 0.04f, 0.04f, 4.0f, 64.0f);
}

inline void W_Fire_Nailgun(WeaponSystem& w) {
	w.Hitscan(9.0f, 128.0f);
}


// ---------------- the table ---------------- //

inline void RegisterWeapons(WeaponSystem& w) {
	w.maxAmmo[AMMO_SHELLS] = 100;
	w.maxAmmo[AMMO_NAILS]  = 200;

	w.AddWeapon({
		WEAPON_AXE, "Axe",
		/*slot*/ 		1, 
		/*ammoType*/ 	AMMO_NONE, 
		/*ammoPerShot*/ 0, 
		/*refire*/ 		0.3f,
		W_Fire_Axe,
		/*vmOffset*/ { 0.28f, -0.25f, 0.55f }, /*vmSize*/ { 0.06f, 0.06f, 0.6f }, /*vmColor*/ { 0.6f, 0.6f, 0.65f, 1.0f },
		/*kick*/ 	1.0f
	});

	w.AddWeapon({
		WEAPON_SHOTGUN, "Shotgun",
		/*slot*/ 		2, 
		/*ammoType*/ 	AMMO_SHELLS, 
		/*ammoPerShot*/ 1, 
		/*refire*/ 		0.5f,
		W_Fire_Shotgun,
		/*vmOffset*/ { 0.25f, -0.2f, 0.6f }, /*vmSize*/ { 0.09f, 0.09f, 0.7f }, /*vmColor*/ { 0.35f, 0.25f, 0.15f, 1.0f },
		/*kick*/ 	0.8f
	});

	w.AddWeapon({
		WEAPON_NAILGUN, "Nailgun",
		/*slot*/ 		3,
		/*ammoType*/ 	AMMO_NAILS,
		/*ammoPerShot*/ 1,
		/*refire*/ 		0.1f,
		W_Fire_Nailgun,
		/*vmOffset*/ { 0.25f, -0.2f, 0.6f }, /*vmSize*/ { 0.09f, 0.09f, 0.7f }, /*vmColor*/ { 0.5f, 0.5f, 0.15f, 1.0f },
		/*kick*/ 	0.8f,
		/*vmModel*/ "weapons/famas/famas.obj",		//"weapons/nailgun.glb"
		/*vmModelScale*/ 1.0f,
		/*vmModelRotation*/ { 0.5f, 0.0f, 0.0f },
		/*fireSound*/ "s_weapons/famas.mp3",
		/*fireSoundMax*/ 0.11f		// cut the clip after 0.2 s so rapid fire stays tight, 0 = full file
	});
}

// what you get on spawn, freeloader you are
inline void GiveStartingLoadout(WeaponSystem& w) {
	w.Give(WEAPON_AXE);
	w.Give(WEAPON_SHOTGUN);
	w.Give(WEAPON_NAILGUN);
	w.GiveAmmo(AMMO_SHELLS, 25);
	w.Select(WEAPON_SHOTGUN);
	w.GiveAmmo(AMMO_NAILS, 100);
}
