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

#include "print_stack.h"

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

int index_function(lua_State* L)
{
	lua_pushvalue(L, lua_upvalueindex(1));            // ud, k, mt

	// First we look for a function
	lua_pushstring(L, "__class");            // ud, k, mt, "__class"
	lua_rawget(L, -2);                       // ud, k, mt, __class table

	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);            // ud, k, mt
		return 1;
	}

	lua_pushvalue(L, 2);            // ud, k, mt, __class table, k
	lua_rawget(L, -2);              // ud, k, mt, __class table, function
	lua_remove(L, -2);              // ud, k, mt, function

	if (lua_isfunction(L, -1))
	{                                     // Found the function, clean up and return
		lua_remove(L, -2);            // ud, k, function
		return 1;
	}
	lua_pop(L, 1);            // ud, k, mt

	// Function not found, look for property
	lua_pushstring(L, "__get");            // ud, k, mt, "__get"
	lua_rawget(L, -2);                     // ud, k, mt, __get table

	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);            // ud, k, mt
		return 1;
	}

	lua_pushvalue(L, 2);            // ud, k, mt, __get table, k
	lua_rawget(L, -2);              // ud, k, mt, __get table, function
	lua_remove(L, -2);              // ud, k, mt, function

	if (lua_isfunction(L, -1))
	{                                     // Found the property,
		lua_remove(L, -2);            // ud, k, function

		lua_pushvalue(L, 1);            // push field
		lua_pushvalue(L, 2);            // push userdata
		lua_call(L, 2, 1);              // ud, k, result

		return 1;
	}
	lua_pop(L, 1);            // ud, k, mt

	return 1;
}

int newindex_function(lua_State* L)
{
	lua_pushvalue(L, lua_upvalueindex(1));            // ud, k, v, mt

	lua_pushstring(L, "__set");            // ud, k, v, mt, "__set"
	lua_rawget(L, -2);                     // ud, k, v, mt, __set table

	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);            // ud, k, v, mt
		return 0;
	}

	lua_pushvalue(L, 2);            // ud, k, v, mt, __set table, k
	lua_rawget(L, -2);              // ud, k, v, mt, __set table, function
	lua_remove(L, -2);              // ud, k, v, mt, function

	if (lua_isfunction(L, -1))
	{                                       // Found the property
		lua_pushvalue(L, 3);            // ud, k, v, mt, function, ud, v
		lua_pushvalue(L, 1);            // ud, k, v, mt, function, ud
		lua_pushvalue(L, 2);            // push field

		lua_call(L, 3, 0);            // ud, k, v, mt
		lua_pop(L, 1);            // ud, k, v

		return 0;
	}

	lua_pop(L, 1);            // ud, k, v, mt

	return 0;
}

int read_only(lua_State* L)
{
	luaL_error(L, "Property %s is read-only", lua_tostring(L, lua_upvalueindex(1)));
	lua_pushnil(L);
	return 1;
}

int write_only(lua_State* L)
{
	luaL_error(L, "Property %s is write-only", lua_tostring(L, lua_upvalueindex(1)));
	lua_pushnil(L);
	return 1;
}

void lua_classvariable(lua_State* L, const char* variable_name, lua_CFunction set, lua_CFunction get)
{
	lua_pushstring(L, "__set");
	lua_rawget(L, -2);

	if (!set)
	{
		lua_pushstring(L, variable_name);
		lua_pushstring(L, variable_name);
		lua_pushcclosure(L, read_only, 1);
		lua_rawset(L, -3);
	}
	else
	{
		lua_pushstring(L, variable_name);
		lua_pushstring(L, variable_name);
		lua_pushcclosure(L, set, 1);
		lua_rawset(L, -3);
	}

	lua_pop(L, 1);

	lua_pushstring(L, "__get");
	lua_rawget(L, -2);

	if (!get)
	{
		lua_pushstring(L, variable_name);
		lua_pushstring(L, variable_name);
		lua_pushcclosure(L, write_only, 1);
		lua_rawset(L, -3);
	}
	else
	{
		lua_pushstring(L, variable_name);
		lua_pushstring(L, variable_name);
		lua_pushcclosure(L, get, 1);
		lua_rawset(L, -3);
	}

	lua_pop(L, 1);
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

void lua_getclass(lua_State* L, const char* class_name)
{
	lua_pushstring(L, "mt");
	lua_rawget(L, LUA_REGISTRYINDEX);	// mt_table

	check_fatal(lua_istable(L, -1), "lua_getclass expected a table");

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

	lua_pushstring(L, "__get");
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
	lua_rawset(L, -3);
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

void lua_classfunction(lua_State* L, const char* fn_name, lua_CFunction fn)
{
	if (fn)
	{
		lua_pushstring(L, "__class");
		lua_rawget(L, -2);

		lua_pushstring(L, fn_name);
		lua_pushstring(L, fn_name);
		lua_pushcclosure(L, fn, 1);
		lua_rawset(L, -3);

		lua_pop(L, 1);
	}
}

using namespace luas;

template <typename ObjType>
struct ctor_caller
{
	template <typename... A, typename... In>
	static void push_and_call_impl(state& _s, int nargs, [[maybe_unused]] int i, In&&... args) requires (detail::is_empty_args<A...>)
	{
		new(lua_newuserdata(_s.get(), sizeof(ObjType))) ObjType(args...);
	}

	template <typename T, typename... A, typename... In>
	static void push_and_call_impl(state& _s, int nargs, int i, In&&... args)
	{
		push_and_call_impl<A...>(_s, nargs, i, args..., _s.pop_track<T>(i));
	}

	template <typename T>
	struct caller { using type = T; };

	template <typename R, typename... A>
	struct caller<R(A...)>
	{
		static void push_and_call(state& _s, int nargs) { push_and_call_impl<A...>(_s, nargs, -nargs); }
	};

	template <typename Ctor>
	static void call(state& _s, int nargs) { caller<Ctor>::push_and_call(_s, nargs); }
};

struct MemberAccessFns
{
	void* write = nullptr,
		* read = nullptr;

	MemberAccessFns() {}

	template <typename S, typename G>
	MemberAccessFns(S&& s, G&& g)
	{
		write = std::bit_cast<void*>(std::move(s));
		read = std::bit_cast<void*>(std::move(g));
	}
};

std::unordered_map<type_info*, std::unordered_map<std::string, MemberAccessFns>> class_member_access;

struct property_wrapper_base {};

template <typename S, typename G>
struct property_wrapper : public property_wrapper_base
{
	std::string key;

	S s;
	G g;

	template <typename Sx, typename Gx>
	property_wrapper(const std::string& key, Sx&& s, Gx&& g) : key(key), s(std::move(s)), g(std::move(g)) {}
};

template <typename S, typename G>
inline auto property(const std::string& key, S&& s, G&& g)
{
	return property_wrapper<std::decay_t<S>, std::decay_t<G>>(key, std::forward<S>(s), std::forward<G>(g));
}

template <typename T, typename Ctor, typename... A>
void register_class(ctx& cs, const std::string& name, A&&... args)
{
	static const auto type_info = const_cast<std::type_info*>(&typeid(T));

	auto create = [](lua_State* _L)
	{
		state s(_L);

		ctor_caller<T>::call<Ctor>(s, lua_gettop(_L));

		s.push_userdata(type_info);
		s.get_raw(LUA_REGISTRYINDEX);
		
		const auto class_name = s.to_string(-1).first;

		s.pop_n();

		lua_getclass(_L, class_name.c_str());

		s.set_metatable(-2);

		return 1;
	};

	auto destroy = [](lua_State* L)
	{
		if (const auto obj = (T*)lua_touserdata(L, -1))
		{
			obj->~T();

			lua_pushboolean(L, true);
		}
		else lua_pushboolean(L, false);

		return 1;
	};

	const auto L = cs.get_lua_state();
	const auto vm = cs.get();

	lua_newclass(L);
	lua_classfunction(L, "create", create);
	lua_classmetamethod(L, "__gc", destroy);

	auto iterate_fields = []<typename I, typename Ix = std::remove_cvref_t<I>, typename... IA>(const auto& self, lua_State* L, I&& v, IA&&... args)
	{
		// register properties
		
		if constexpr (std::derived_from<Ix, property_wrapper_base>)
		{
			lua_classvariable(L, v.key.c_str(),
			[](lua_State* L)	// setter
			{
				if (const auto ud = lua_touserdata(L, -2))
				{
					state s(L);

					const auto field = lua_tostring(L, -1);
					const auto obj = std::bit_cast<T*>(ud);
					const auto new_value = s.pop_read<typename detail::first_arg_member_fn<decltype(Ix::s)>::type>(-3);

					if (const auto fn_ptr = class_member_access[type_info][field].write)
						(*std::bit_cast<detail::keep_member_ptr_fn_v<decltype(Ix::s)>*>(&fn_ptr))(obj, new_value);
				}

				return 0;
			},
			[](lua_State* L)	// getter
			{
				if (const auto ud = lua_touserdata(L, -2))
				{
					const auto field = lua_tostring(L, -1);
					const auto obj = std::bit_cast<T*>(ud);

					if (const auto fn_ptr = class_member_access[type_info][field].read)
					{
						state(L).push((*std::bit_cast<detail::keep_member_ptr_fn_v<decltype(Ix::g)>*>(&fn_ptr))(obj));

						return 1;
					}
				}

				lua_pushnil(L);

				return 1;
			});

			class_member_access[type_info].insert({ v.key, MemberAccessFns(v.s, v.g) });
		}

		if constexpr (sizeof...(IA) > 0)
			self(self, L, std::forward<IA>(args)...);
	};

	iterate_fields(iterate_fields, L, std::forward<A>(args)...);

	/*lua_classfunction(L, "getX", [](lua_State* L)
	{
		const auto v = *std::bit_cast<T**>(lua_touserdata(L, -1));
		lua_pushnumber(L, v->x);
		return 1;
	});*/
	
	lua_registerclass(L, name.c_str());

	vm->push_userdata(type_info);
	vm->push(name);
	vm->set_raw(LUA_REGISTRYINDEX);
}

int allocs = 0,
	frees = 0;

struct vec3
{
	float x, y, z;

	vec3(float x, float y, float z) : x(x), y(y), z(z) { printf_s("[vec3] ctor %.2f %.2f %.2f (%i - %i)\n", x, y, z, allocs, frees); ++allocs; }
	~vec3() { printf_s("[vec3] dtor (%i - %i)\n", allocs, frees); ++frees; }

	float get_x() { return x; }
	float get_y() { return y; }
	float get_z() { return z; }

	void set_x(float v) { x = v; }
	void set_y(float v) { y = v; }
	void set_z(float v) { z = v; }
};

struct vec2
{
	float x, y;

	vec2(float x, float y) : x(x), y(y) { printf_s("[vec2] ctor %.2f %.2f\n", x, y); }
	~vec2() { printf_s("[vec2] dtor\n"); }

	float get_x() { return x; }
	float get_y() { return y; }

	void set_x(float v) { x = v; }
	void set_y(float v) { y = v; }
};

int main()
{
#if COMPILE_LUAWRAPPER
	{
		LuaContext lua{};

		lua.writeVariable("a",
			std::vector<std::string>
		{
			"hello",
				"goodbye",
				"how are",
				"you"
		}
		);
		
		lua.registerFunction("tt", &vec3::get_x);

		lua.executeCode(R"(
)");
	}
#endif

	fatal_error_callback = [](const char*) { return 0; };
	error_callback = [](const char* err) { printf_s("[ERROR] %s\n", err); return 0; };

	luas::ctx script(true);

	{
		register_class<vec3, vec3(float, float, float)>(
			script,
			"vec3",
			property("x", &vec3::set_x, &vec3::get_x),
			property("y", &vec3::set_y, &vec3::get_y),
			property("z", &vec3::set_z, &vec3::get_z)
			);

		/*register_class<vec2, vec2(float, float)>(
			script,
			"vec2",
			property("x", &vec2::set_x, &vec2::get_x),
			property("y", &vec2::set_y, &vec2::get_y)
			);*/
	}

	script.exec_string(R"(
function test()
	local a = vec3(1, 2, 3);
	getVec(a);
	--local b = vec2(1, 2);

	a.y = math.random(0, 10);

	--b.x = math.random(0, 10);
	--b.y = math.random(0, 10);

	--print(a.x);
	--print(a.y);
	--print(a.z);

	--print(b.x);
	--print(b.y);

	collectgarbage();
end
)");

	script.add_function("getVec", [](state& s, vec3* obj)
	{
		printf_s("lol %.2f %.2f %.2f\n", obj->x, obj->y, obj->z);
		/*const auto obj = lua_newuserdata(s.get(), sizeof(vec3));
		*std::bit_cast<vec3**>(obj) = new vec3(10.f, 20.f, 30.f);

		s.push_userdata(const_cast<std::type_info*>(&typeid(vec3)));
		s.get_raw(LUA_REGISTRYINDEX);

		const auto class_name = s.to_string(-1).first;

		s.pop_n();

		lua_getclass(s.get(), class_name.c_str());

		s.set_metatable(-1);
		print_stack(s.get());
		lua_call(s.get(), 0, 0);

		s.push(10.f, 20.f, 30.f);
		lua_call(s.get(), 3, 1);*/

		return obj;
	});

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