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

#define LUA_MYCLASS "vec3"

int lua_MyClass_new(lua_State* L) {
	// Optional argument to supply to the `MyClass` constructor; using `luaL_optinteger`, it defaults to 0.
	vec3** mcpp = reinterpret_cast<vec3**>(lua_newuserdata(L, sizeof(vec3**)));
	*mcpp = new vec3();
	
	luaL_setmetatable(L, LUA_MYCLASS);
	printf_s("created vec3\n");
	return 1;
}

// `MyClass` destructor, which corresponds to the `__gc` metamethod in Lua.
int lua_MyClass_delete(lua_State* L) {
	printf_s("deleted vec3\n");
	vec3* mcp = *reinterpret_cast<vec3**>(luaL_checkudata(L, 1, LUA_MYCLASS));
	delete mcp;
	return 0;
}

// `__newindex` metamethod for `MyClass` userdata that prevents any members from being added.
int lua_MyClass_newindex(lua_State* L) {
	return luaL_error(L, "attempt to modify a read-only object");
}

// `__newindex` metamethod for the `MyClass` table that prevents any methods from being added--I will explain more below.
int lua_MyClass_table_newindex(lua_State* L) {
	return luaL_error(L, "attempt to modify a read-only table");
}

// This one is for the class metamethods.
static const luaL_Reg MyClass_metamethods[] = {
	{"__gc", lua_MyClass_delete},
	{"__newindex", lua_MyClass_newindex},
	{nullptr, nullptr}
};

void class_test(lua_State* L)
{
	lua_newtable(L);
	lua_pushcfunction(L, lua_MyClass_new);
	lua_setfield(L, -2, "new");

	// Create a metatable for the global table `MyClass`--which was just created.
	lua_newtable(L);
	// Prevent access to the metatable.
	/*lua_pushliteral(L, "metatable");
	lua_setfield(L, -2, "__metatable");
	lua_pushcfunction(L, lua_MyClass_table_newindex);
	lua_setfield(L, -2, "__newindex");*/
	// Set this second table as the metatable for the one created above.
	lua_setmetatable(L, -2);
	// Call the first table "MyClass" and add it to the global environment table (_ENV).
	lua_setglobal(L, LUA_MYCLASS);

	// Create a metatable to be used by `MyClass` objects--this is different from the above tables because it will not contain the `new` method.
	luaL_newmetatable(L, LUA_MYCLASS);
	// Same as before, lock the metatable.
	lua_pushliteral(L, "metatable");
	lua_setfield(L, -2, "__metatable");
	// Add metamethods contained in the `luaL_Reg` struct `MyClass_metamethods`.
	luaL_setfuncs(L, MyClass_metamethods, 0);

	// Create an index--the `__index` metamethod--for the above table to use for `MyClass` objects.
	//lua_newtable(L);
	// Add methods.
	//luaL_setfuncs(L, MyClass_methods, 0);
	//lua_setfield(L, -2, "__index");
	// This pop operation is probably unnecessary since the Lua stack should be cleaned up when this function returns.
	lua_pop(L, 1);
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
function test()
	local v = vec3.new();
	print(v);
end
)");

	BringWindowToTop(GetConsoleWindow());

	while (true)
	{
		debug_memory();

		//printf_s("\n--------------------------\n");

		{
			//TimeProfiling tp("tick3");

			//script.call_safe("tick3");
		}

		script.call_safe("test");

		/*printf_s("should print positive now:\n");
		printf_s("tick1 res: %i\n", std::get<0>(fn1.call<int>()));
		printf_s("should print negative now:\n");
		printf_s("tick2 res: %i\n", std::get<0>(fn2.call<int>(1, 1, 1)));*/

		//Sleep(0);
	}

	return std::cin.get();
}