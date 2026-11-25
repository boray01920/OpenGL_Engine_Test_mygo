#pragma once
//
//   Audio audio;                                   	// ocreated once in main
//   audio.Play("s_weapons/famas.mp3");            	 	// one-shot, plays to the end
//   audio.Play("s_weapons/famas.mp3", 0.6f, 0.25f);	// volume 0.6, cut off after 0.25 s
//   audio.PlayMusic("music/theme.mp3");            	// looping, streamed
//
// Paths are relative to assets/sounds/. mp3, wav, flac and ogg-less builds all work (miniaudio decodes mp3/wav/flac).
// Sounds are decoded once and cached by miniaudio

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "miniaudio.h"
#include "Paths.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class Audio {
public:
	float sfxVolume   = 1.0f;
	float musicVolume = 0.5f;
	int   maxVoices   = 32;       // oldest one-shot is cut when this many are playing

	Audio() {
		ready = ma_engine_init(nullptr, &engine) == MA_SUCCESS;
		if (!ready) std::cout << "Audio: could not start the audio engine, running silent" << std::endl;
	}
	~Audio() {
		if (!ready) return;
		StopMusic();
		for (Voice& v : voices) ma_sound_uninit(&v.sound->s);
		voices.clear();
		ma_engine_uninit(&engine);
	}
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

	// Decode a sound ahead of time
	void Preload(const std::string& file) {
		if (!ready || file.empty()) return;
		ma_sound s;
		if (ma_sound_init_from_file(&engine, FullPath(file).c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &s) == MA_SUCCESS)
			ma_sound_uninit(&s);
		else std::cout << "Audio: could not load '" << file << "'" << std::endl;
	}

	void Play(const std::string& file, float volume = 1.0f, float maxSeconds = 0.0f) {
		if (!ready || file.empty()) return;
		Reap();
		if ((int)voices.size() >= maxVoices) { ma_sound_uninit(&voices.front().sound->s); voices.erase(voices.begin()); }

		Voice v;
		v.sound = std::make_unique<Holder>();
		if (ma_sound_init_from_file(&engine, FullPath(file).c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &v.sound->s) != MA_SUCCESS) {
			std::cout << "Audio: could not play '" << file << "'" << std::endl;
			return;
		}
		ma_sound_set_volume(&v.sound->s, volume * sfxVolume);
		if (maxSeconds > 0.0f) {
			v.endMs = ma_engine_get_time_in_milliseconds(&engine) + (ma_uint64)(maxSeconds * 1000.0f);
			ma_sound_set_stop_time_in_milliseconds(&v.sound->s, v.endMs);
		}
		ma_sound_start(&v.sound->s);
		voices.push_back(std::move(v));
	}

	// Looping background musi
	void PlayMusic(const std::string& file, bool loop = true) {
		if (!ready) return;
		StopMusic();
		music = std::make_unique<Holder>();
		if (ma_sound_init_from_file(&engine, FullPath(file).c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, &music->s) != MA_SUCCESS) {
			std::cout << "Audio: could not load music '" << file << "'" << std::endl;
			music.reset();
			return;
		}
		ma_sound_set_looping(&music->s, loop ? MA_TRUE : MA_FALSE);
		ma_sound_set_volume(&music->s, musicVolume);
		ma_sound_start(&music->s);
	}

	void StopMusic() {
		if (!music) return;
		ma_sound_uninit(&music->s);
		music.reset();
	}

	void SetMusicVolume(float v) { musicVolume = v; if (music) ma_sound_set_volume(&music->s, v); }
	void SetMasterVolume(float v) { if (ready) ma_engine_set_volume(&engine, v); }

private:
	struct Holder { ma_sound s; };                 // ma_sound can't be moved, so it lives on the heap
	struct Voice  { std::unique_ptr<Holder> sound; ma_uint64 endMs = 0; };

	ma_engine engine{};
	bool ready = false;
	std::vector<Voice> voices;
	std::unique_ptr<Holder> music;

	static std::string FullPath(const std::string& file) { return (Paths::Root() / "assets" / "sounds" / file).generic_string(); }

	// free the voices that finished or hit their cut-off time
	void Reap() {
		ma_uint64 now = ma_engine_get_time_in_milliseconds(&engine);
		voices.erase(std::remove_if(voices.begin(), voices.end(), [&](Voice& v) {
			bool done = ma_sound_at_end(&v.sound->s) || (v.endMs && now >= v.endMs);
			if (done) ma_sound_uninit(&v.sound->s);
			return done;
		}), voices.end());
	}
};
