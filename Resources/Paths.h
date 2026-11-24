#pragma once
// Locates the project's assets no matter where the exe is started from
//	this is more for development purposes
//   assets/maps/map_<N>/<anything>.obj   (+ .mtl, .rules and textures/ next to it)
//   shaders/*.vert|*.frag

#include <filesystem>
#include <iostream>
#include <string>

extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void*, char*, unsigned long);

namespace Paths {

namespace fs = std::filesystem;

// 	looks for a folder containing "assets"
//	walking upward first from the working directory,
// 	then from the exe's own folder (so it still works when launched from somewhere else)
inline const fs::path& Root() {
	static const fs::path root = [] {
		std::error_code ec;
		fs::path starts[2];
		starts[0] = fs::current_path(ec);
		char buf[1024];
		if (GetModuleFileNameA(nullptr, buf, sizeof(buf))) starts[1] = fs::path(buf).parent_path();
		for (const fs::path& start : starts)
			for (fs::path cur = start; !cur.empty(); cur = cur.parent_path()) {
				if (fs::is_directory(cur / "assets", ec)) return cur;
				if (cur == cur.parent_path()) break;
			}
		std::cout << "Paths: could not find an 'assets' folder above " << starts[0].string()
		          << " or " << starts[1].string() << std::endl;
		return starts[0];
	}();
	return root;
}

inline std::string Shader(const std::string& file) { return (Root() / "shaders" / file).generic_string(); }

// assets/maps/map_<number>
inline fs::path MapDir(int number) { return Root() / "assets" / "maps" / ("map_" + std::to_string(number)); }

// 	first file with the given extension (".obj", ".rules") inside the map folder
//	empty if none
// 	pray that it isnt empty
inline std::string MapFile(int number, const std::string& ext) {
	std::error_code ec;
	fs::path dir = MapDir(number);
	if (!fs::is_directory(dir, ec)) { std::cout << "Paths: map folder not found: " << dir.string() << std::endl; return ""; }
	for (const fs::directory_entry& e : fs::directory_iterator(dir, ec))
		if (e.is_regular_file() && e.path().extension() == ext) return e.path().generic_string();
	return "";
}

}
