#ifdef _DEBUG
#pragma comment(lib, "lua544_static_debug.lib")
#else
#pragma comment(lib, "lua544_static.lib")
#endif

#define USE_DEBUG 1

#if USE_DEBUG
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

#include <iostream>
#endif

#define COMPILE_LUAWRAPPER 0

#if COMPILE_LUAWRAPPER
// used for comparisons
//
#include "test/luawrapper/LuaContext.hpp"
#endif

#include <luas.h>

void debug_memory()
{
	PROCESS_MEMORY_COUNTERS_EX data;
	GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&data), sizeof(data));

	float mb = static_cast<float>(data.WorkingSetSize) / powf(2.f, 20.f),
		kb = static_cast<float>(data.WorkingSetSize) / powf(2.f, 10.f);

	char buf[64] = { 0 };

	sprintf_s(buf, "Memory: %.3f MB | %.3f KB", mb, kb);
	SetConsoleTitleA(buf);
}

struct TimeProfiling
{
	std::chrono::high_resolution_clock::time_point m_start;
	uint64_t									   cycles = 0;
	std::string name = "";

	static inline std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> average_cycles;

	TimeProfiling(const std::string& name) : name(name)
	{
		if (!average_cycles.contains(name))
			average_cycles[name] = { 0ull, 0ull };

		m_start = std::chrono::high_resolution_clock::now();
		cycles = __rdtsc();
	}

	~TimeProfiling()
	{
		const auto cycles_passed = __rdtsc() - cycles;
		const auto time_passed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_start).count();

		auto& info = average_cycles[name];

		++info.first;
		info.second += cycles_passed;

		const auto average = info.second / info.first;

		printf_s(FORMATV("{}: {:.3f} ms | {} mcs | {} cycles (average cycles {})\n", name, static_cast<double>(time_passed) / 1000.f, time_passed, cycles_passed, average).c_str());
	}
};

struct smol
{
	float x;
};

struct vec3
{
	static inline int allocs = 0,
					  frees = 0;

	float x = 0.f, y = 0.f, z = 0.f;

	//std::vector<uint64_t> test;

	vec3()												{ /*printf_s("[ALLOC 1 %p] %i %i\n", this, ++allocs, frees);*/ /*for (int i = 0; i < 1000; ++i) test.push_back(i);*/ }
	vec3(float x, float y, float z) : x(x), y(y), z(z)	{ /*printf_s("[ALLOC 2 %p] %i %i\n", this, ++allocs, frees);*/ /*for (int i = 0; i < 1000; ++i) test.push_back(i);*/ }
	~vec3()												{ /*printf_s("[FREE %p] %i %i (size: %i)\n", this, allocs, ++frees, test.size());*/ }

	float get_x() { return x; }
	float get_y() { return y; }
	float get_z() { return z; }

	void set_x(float v) { x = v; }
	void set_y(float v) { y = v; }
	void set_z(float v) { z = v; }

	vec3 add(const vec3& v)
	{
		return { x + v.x, y + v.y, z + v.z };
	}

	float length() const { return std::sqrtf(x * x + y * y + z * z); }
};

int main()
{
	SetThreadAffinityMask(GetCurrentThread(), 0);
	
#if COMPILE_LUAWRAPPER
	{
		LuaContext lua {};
		lua.writeVariable("a", std::vector<std::string> { "hello", "goodbye", "how are", "you" });
		lua.registerFunction("tt", &vec3::get_x);
		lua.executeCode(R"()");
	}
#endif

	fatal_error_callback = [](const char*) { return 0; };
	error_callback = [](const char* err) { printf_s("[ERROR] %s\n", err); return 0; };

	luas::ctx script(true);

	{
		script.register_class<vec3, vec3(float, float, float)>(
			"vec3",
			luas::property("x", &vec3::set_x, &vec3::get_x),
			luas::property("y", &vec3::set_y, &vec3::get_y),
			luas::property("z", &vec3::set_z, &vec3::get_z),
			luas::function("len", &vec3::length),
			luas::function("add", &vec3::add)
			);
	}

	struct ay
	{
		std::vector<int> a;
	};

	auto _ay = new ay();

	script.add_global("test", _ay);

	//printf_s("0x%x\n", script.get()->get_global_var<std::string>("test").c_str());

	script.add_function("getVec", [](ay* a, const vec3& v)
	{
		//return std::vector<int>({ 1, 2, 3, 4, 5 });
		printf_s("%.2f %.2f %.2f\n", v.x, v.y, v.z);
	});

	script.exec_string(R"(
function test()

	local a = vec3(1, 2, 3);

	getVec(nil, a);

end
)");

	BringWindowToTop(GetConsoleWindow());

	while (true)
	{
		debug_memory();

		printf_s("\n----------- TICK -----------\n");

		{
			//TimeProfiling tp("time test");

			script.call_safe("test");
		}

		/*printf_s("should print positive now:\n");
		printf_s("tick1 res: %i\n", std::get<0>(fn1.call<int>()));
		printf_s("should print negative now:\n");
		printf_s("tick2 res: %i\n", std::get<0>(fn2.call<int>(1, 1, 1)));*/

		//Sleep(0);
	}

	return std::cin.get();
}