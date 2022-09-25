#include "luas.h"

#define COMPILE_LUAWRAPPER 1

#if COMPILE_LUAWRAPPER
// used for comparisons
//
#include "test/luawrapper/LuaContext.hpp"
#endif

struct TimeProfiling
{
	std::chrono::high_resolution_clock::time_point m_start;
	uint64_t									   cycles = 0;
	char										   name[128] = { 0 };

	TimeProfiling(const char* name)
	{
		strcpy_s(this->name, name);
		m_start = std::chrono::high_resolution_clock::now();
		cycles = __rdtsc();
	}

	~TimeProfiling()
	{
		const auto cycles_passed = __rdtsc() - cycles;
		const auto time_passed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_start).count();
		printf_s(FORMATV("{}: {:.3f} ms | {} mcs | {} cycles\n", name, static_cast<double>(time_passed) / 1000.f, time_passed, cycles_passed).c_str());
	}
};

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

luas::lua_fn fn1;
luas::lua_fn fn2;

int main()
{
#if COMPILE_LUAWRAPPER
	{
		LuaContext lua {};

		lua.writeVariable("a",
			std::vector<std::string>
		{
			"hello",
			"goodbye",
			"how are",
			"you"
		}
		);

		lua.writeFunction("tt", [](std::vector<int> v)
		{
			printf_s("%i\n", v.size());
		});

		lua.executeCode(R"(
)");
	}
#endif

	fatal_error_callback = [](const char*) { return 0; };
	error_callback = [](const char* err) { printf_s("%s\n", err); return 0; };

	struct Obj
	{
		int val = 0;
	};

	luas::ctx script;

	script.add_global("gv0", true);
	script.add_global("gv1", 5.0);
	script.add_global("gv2", -5.f);
	script.add_global("gv3", "test string");
	script.add_global("_obj0", new Obj{ 1000 });
	script.add_global("_obj1", new Obj{ 2000 });
	script.add_global("_obj2", new Obj{ 3000 });
	script.add_global("_vec", std::vector<Obj*> { new Obj{ 10 }, new Obj{ 20 }, new Obj{ 30 }, new Obj{ 40 } });
	script.add_global("_map", std::map<std::string, Obj*>
	{
		{ "wo1", new Obj{ 10 } },
		{ "wo2", new Obj{ 20 } },
		{ "wo3", new Obj{ 30 } },
		{ "wo4", new Obj{ 40 } },
	});

	script.add_function("getValue", [](Obj* v)
	{
		return v->val;
	});

	script.add_function("getTable", []()
	{
		std::unordered_map<Obj*, int> out;

		out.insert({ new Obj{ 100 }, 22 });
		out.insert({ new Obj{ 101 }, 33 });
		out.insert({ new Obj{ 102 }, 44 });
		out.insert({ new Obj{ 103 }, 55 });
		out.insert({ new Obj{ 104 }, 66 });
		out.insert({ new Obj{ 105 }, 77 }); 

		return out;
	});

	script.add_function("addEvent", [](luas::variadic_args va)
	{
		//printf_s("[addEvent] %s\n", str.c_str());
		//printf_s("[addEvent] %s\n", va.get<std::string>(0).c_str());
		//printf_s("variadic_args size: %i\n", va.size());

		//const auto vec = va.get<std::vector<Obj*>>(3);
		//const auto vec = va.get<std::map<Obj*, Obj*>>(3);
		const auto size = va.size();

		va.get<luas::lua_fn>(1).call();

		//for (int i = 0; i < va.values.size(); ++i)
		//	printf_s("type: %s\n", va.values[i].type().name());

		//s.call_safe_fn("tick0", 0, "ye :o");
	});

	/*script.add_function("addEvent", [](luas::state& s, const std::string& name, luas::lua_fn& v1, luas::lua_fn& v2, luas::variadic_args va)
	{
		printf_s("[addEvent] %s, \n", name.c_str());

		s.call_safe_fn("tick0", 0, "ye :o");

		//v1.call();
		//v2.call(50, 50, 50);

		fn1 = std::move(v1);
		fn2 = std::move(v2);
	});*/

	script.exec_string(R"(

function tick0(a)
	print("wtf insanity: ".. tostring(a));
end

function tick1()
	local test_table = getTable();
	for k, v in pairs(test_table) do
		print("k: " .. tostring(getValue(k)) .. " | v: " .. tostring(v));
	end
	return 1;
end

function tick2(a, b, c)
	return a + b + c;
end

function tick3(a, b)
	local _table = {};

	--_table[_obj0] = _obj0;
	--_table[_obj1] = _obj0;
	--_table[_obj2] = _obj1;

	addEvent("onTick", tick1, 1234.47, _table);

	--addEvent("onTick", tick1, tick2, true);
	return a - b;
end

)");

	BringWindowToTop(GetConsoleWindow());

	while (true)
	{
		debug_memory();

		//printf_s("\n--------------------------\n");

		std::tuple<float> v2 = script.call_safe<float>("tick3", 10, 7);

		printf_s(FORMATV("{}\n", std::get<0>(v2)).c_str());
		/*printf_s("should print positive now:\n");
		printf_s("tick1 res: %i\n", std::get<0>(fn1.call<int>()));
		printf_s("should print negative now:\n");
		printf_s("tick2 res: %i\n", std::get<0>(fn2.call<int>(1, 1, 1)));*/

		//Sleep(0);
	}

	return std::cin.get();
}