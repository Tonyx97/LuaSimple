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

#include "luas.h"

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

struct vec3
{
	float x, y, z;
};

int object_allocator(lua_State* L)
{
	printf_s("Allocator called\n");

	if (!lua_istable(L, 1))
		return 0;

	int stack = lua_gettop(L);

	lua_pushstring(L, "create");
	lua_rawget(L, 1);

	if (lua_isfunction(L, -1))
	{
		for (int i = 2; i <= stack; i++)
		{
			lua_pushvalue(L, i);

			printf_s("pushed arg %i\n", i);
		}

		int args = stack - 1;

		printf_s("args %i\n", args);

		lua_call(L, args, LUA_MULTRET);

		return lua_gettop(L) - stack;
	}

	lua_pop(L, 1);

	return 1;
}

int index_function(lua_State* L)
{
	return 0;
}

int newindex_function(lua_State* L)
{
	return 0;
}

void lua_classmetamethod(lua_State* L, const char* method_name, lua_CFunction fn)
{
	if (fn)
	{
		lua_pushstring(L, method_name);
		lua_pushstring(L, method_name);
		lua_pushcclosure(L, fn, 1);
		lua_rawset(L, -3);
	}
}

void lua_initclasses(lua_State* L)
{
	lua_newtable(L);							// mt_table
	lua_setfield(L, LUA_REGISTRYINDEX, "mt");	// REGISTRY["mt"] = mt_table

	/*lua_newtable(L);							// table0
	lua_newtable(L);							// table1
	lua_pushstring(L, "k");
	lua_setfield(L, -2, "__mode");				// table1["__mode"] = "v"
	lua_setmetatable(L, -2);					// table0.__metatable = table1
	lua_setfield(L, LUA_REGISTRYINDEX, "ud");	// REGISTRY["ud"] = table0*/

	lua_getfield(L, LUA_REGISTRYINDEX, "mt");	// registry_mt = REGISTRY["mt"]
	lua_newtable(L);							// table2
	lua_pushcfunction(L, object_allocator);
	lua_setfield(L, -2, "__call");				// table2["__call"] = object_allocator
	lua_setfield(L, -2, "Generic");				// registry_mt["Generic"] = table2

	lua_pop(L, 1);
}

void lua_getclass(lua_State* L, const char* class_name)
{
	lua_pushstring(L, "mt");
	lua_rawget(L, LUA_REGISTRYINDEX);	// mt_table

	//check(lua_istable(L, -1), "oof");

	lua_pushstring(L, class_name);
	lua_rawget(L, -2);					// mt_table[class_name]

	lua_remove(L, -2);					// leave mt_table[class_name] at top of the stack
}

void lua_newclass(lua_State* L)
{
	lua_newtable(L);				// obj_table

	lua_pushstring(L, "__class");
	lua_newtable(L);				// class_table
	lua_getclass(L, "Generic");		// generic_table = mt_table["Generic"]
	lua_setmetatable(L, -2);		// class_table.__metatable = generic_table
	lua_rawset(L, -3);				// REGISTRY["_class"] = obj_table

	/*lua_pushstring(L, "__get");
	lua_newtable(L);
	lua_rawset(L, -3);

	lua_pushstring(L, "__set");
	lua_newtable(L);
	lua_rawset(L, -3);

	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_pushcclosure(L, index_function, 1);
	lua_rawset(L, -3);

	lua_pushstring(L, "__newindex");
	lua_pushvalue(L, -2);
	lua_pushcclosure(L, newindex_function, 1);
	lua_rawset(L, -3);*/
}

void lua_registerclass(lua_State* L, const char* class_name)
{
	lua_pushstring(L, "mt");
	lua_rawget(L, LUA_REGISTRYINDEX);	// mt_table = REGISTRY["mt"]

	// store in registry
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, class_name);	// mt_table[class_name] = mt_table

	lua_pop(L, 1);

	lua_getfield(L, -1, "__class");
	lua_setglobal(L, class_name);

	lua_pop(L, 1);
}

void lua_classfunction(lua_State* L, const char* szFunction, lua_CFunction fn)
{
	if (fn)
	{
		lua_pushstring(L, "__class");
		lua_rawget(L, -2);

		lua_pushstring(L, szFunction);
		lua_pushstring(L, szFunction);
		lua_pushcclosure(L, fn, 1);
		lua_rawset(L, -3);

		lua_pop(L, 1);
	}
}

int create_vec3(lua_State* L)
{
	printf_s("create called\n");

	*std::bit_cast<vec3**>(lua_newuserdata(L, sizeof(vec3))) = new vec3();

	return 1;
}

int destroy_vec3(lua_State* L)
{
	printf_s("destroy called\n");
	return 1;
}

void class_test(lua_State* L)
{
	lua_initclasses(L);

	{
		lua_newclass(L);
		lua_classmetamethod(L, "__gc", destroy_vec3);
		lua_classfunction(L, "create", create_vec3);
		lua_classfunction(L, "ay", [](lua_State* L)
		{
			const auto aaa = lua_touserdata(L, 1);
			printf_s("destroy called 0x%x\n", aaa);
			lua_pushnumber(L, 1234.5678f);
			return 1;
		});
		lua_registerclass(L, "vec3");
	}
}

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

	class_test(script.get()->get());

	script.exec_string(R"(
local v = vec3();
print(v);
)");

	/*script.add_function("getValue", [](Obj* v)
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

		va.get<luas::lua_fn>(1).call();

		//for (int i = 0; i < va.values.size(); ++i)
		//	printf_s("type: %s\n", va.values[i].type().name());

		//s.call_safe_fn("tick0", 0, "ye :o");
	});

	script.add_function("addEvent", [](luas::state& s, const std::string& name, luas::lua_fn& v1, luas::lua_fn& v2, luas::variadic_args va)
	{
		printf_s("[addEvent] %s, \n", name.c_str());

		s.call_safe_fn("tick0", 0, "ye :o");

		//v1.call();
		//v2.call(50, 50, 50);

		fn1 = std::move(v1);
		fn2 = std::move(v2);
	});

	script.exec_string(R"(

function tick0(a)
	print("wtf insanity: ".. tostring(a));
end

function tick1()
	local test_table = getTable();
	--for k, v in pairs(test_table) do
	--	print("k: " .. tostring(getValue(k)) .. " | v: " .. tostring(v));
	--end
	return 1;
end

function tick2(a, b, c)
	return a + b + c;
end

function tick3()
	addEvent("onTick", tick1, 1234.47, _map);
end

)");*/

	BringWindowToTop(GetConsoleWindow());

	while (true)
	{
		debug_memory();

		//printf_s("\n--------------------------\n");

		{
			//TimeProfiling tp("tick3");

			//script.call_safe("tick3");
		}

		/*printf_s("should print positive now:\n");
		printf_s("tick1 res: %i\n", std::get<0>(fn1.call<int>()));
		printf_s("should print negative now:\n");
		printf_s("tick2 res: %i\n", std::get<0>(fn2.call<int>(1, 1, 1)));*/

		//Sleep(0);
	}

	return std::cin.get();
}