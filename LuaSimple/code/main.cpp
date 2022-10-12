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

#include "luas.h"

using namespace luas;

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
		lua_pushvalue(L, 2);            // push field
		lua_pushvalue(L, 1);            // ud, k, v, mt, function, ud
		lua_pushvalue(L, 3);            // ud, k, v, mt, function, ud, v

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

void lua_newclass(lua_State* L)
{
	lua_newtable(L);				// obj_table

	lua_pushstring(L, "__class");
	lua_newtable(L);				// class_table
	state(L).get_class("Generic");
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

struct smol
{
	float x;
};

struct vec3
{
	float x = 0.f, y = 0.f, z = 0.f;

	vec3() {}
	vec3(float x, float y, float z) : x(x), y(y), z(z) {/* printf_s("[vec3] ctor %.2f %.2f %.2f\n", x, y, z); */ }
	~vec3() { /*printf_s("[vec3] dtor\n"); */ }

	float get_x() { return x; }
	float get_y() { return y; }
	float get_z() { return z; }

	void set_x(float v) { x = v; }
	void set_y(float v) { y = v; }
	void set_z(float v) { z = v; }

	vec3 add(const vec3& v)
	{
		auto res = vec3(x + v.x, y + v.y, z + v.z);
		return res;
	}

	float length() const { return std::sqrtf(x * x + y * y + z * z); }
};

template <typename ObjType>
struct ctor_caller
{
	template <typename... A, typename... In>
	static void push_and_call_impl(state& _s, int nargs, [[maybe_unused]] int i, In&&... args) requires (detail::is_empty_args<A...>)
	{
		new (lua_newuserdata(_s.get(), sizeof(ObjType))) ObjType(args...);
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

template <typename T>
concept is_return_type_in_eax = std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_pointer_v<T>;

template <typename T>
struct class_fn_caller { using type = T; };

template <typename R, typename Tx, typename... A>
struct class_fn_caller<R(__thiscall*)(Tx*, A...)>
{
	template <typename... Args, typename... In>
	static int _impl(state& _s, void* fn, Tx* _this, int nargs, [[maybe_unused]] int i, In... args) requires (detail::is_empty_args<Args>)
	{
		if constexpr (std::is_void_v<R>)
			std::bit_cast<R(__thiscall*)(Tx*, A...)>(fn)(_this, args...);
		else if constexpr (is_return_type_in_eax<R>)
			return _s.push(std::bit_cast<R(__thiscall*)(Tx*, A...)>(fn)(_this, args...));
		else
		{
			R out;

			return _s.push(std::bit_cast<R*(__thiscall*)(Tx*, R*, A...)>(fn)(_this, &out, args...));
		}

		return 0;
	}

	template <typename T, typename... Args, typename... In>
	static int _impl(state& _s, void* fn, Tx* _this, int nargs, int i, In&&... args)
	{
		return _impl<Args...>(_s, fn, _this, nargs, i, args..., _s.pop_track<T>(i));
	}

	static int call(state& _s, void* fn, int nargs) { variadic_arg_check<A...>(); return _impl<A...>(_s, fn, _s.pop_read<Tx*>(-nargs), nargs, -nargs + 1); }
};

template <typename T, typename Ctor, typename... A>
bool register_class(ctx& cs, const std::string& name, A&&... args)
{
	static const auto type_info = TYPEINFO(T);

	auto create = [](lua_State* L)
	{
		state s(L);

		if (const auto class_info = s.get_info()->get_class(type_info))
		{
			ctor_caller<T>::call<Ctor>(s, s.get_top());

			s.get_class(class_info->name);
			s.set_metatable(-2);
		}
		else s.push_nil();

		return 1;
	};

	auto destroy = [](lua_State* L)
	{
		if (const auto obj = (T*)lua_touserdata(L, -1))
		{
			// call destructor before lua frees the instance
			
			obj->~T();

			lua_pushboolean(L, true);
		}
		else lua_pushboolean(L, false);

		return 1;
	};

	const auto L = cs.get_lua_state();
	const auto s = cs.get();

	// create state info if it doesn't exist

	const auto state_info = s->get_info();

	if (state_info->has_class(type_info))
		return false;

	const auto class_info = state_info->add_class(type_info);

	class_info->name = name;

	// start defining the class

	auto iterate_args = []<typename I, typename Ix = std::remove_cvref_t<I>, typename... IA>(const auto& self, lua_State* L, I&& v, IA&&... args)
	{
		// register properties
		
		if constexpr (std::derived_from<Ix, property_wrapper_base>)
		{
			lua_classvariable(L, v.key.c_str(),
			[](lua_State* L)	// setter
			{
				state s(L);

				if (const auto field_info = s.get_info()->get_class(type_info)->get_field_info(s.pop_read<std::string>(-3)))
					return class_fn_caller<detail::keep_member_ptr_fn_v<decltype(Ix::s)>>::call(s, field_info->write, -2);

				return s.push_nil();
			},
			[](lua_State* L)	// getter
			{
				state s(L);

				if (const auto field_info = s.get_info()->get_class(type_info)->get_field_info(s.pop_read<std::string>(-1)))
					return class_fn_caller<detail::keep_member_ptr_fn_v<decltype(Ix::g)>>::call(s, field_info->read, s.get_top());

				return s.push_nil();
			});

			state(L).get_info()->get_class(type_info)->add_field(v.key, v.s, v.g);
		}
		else if constexpr (std::derived_from<Ix, function_wrapper_base>)
		{
			lua_classfunction(L, v.key.c_str(), [](lua_State* L)	// method
			{
				state s(L);

				if (const auto fn_ptr = s.get_info()->get_class(type_info)->get_function(TYPEINFO(Ix::value)))
					return class_fn_caller<detail::keep_member_ptr_fn_v<decltype(Ix::value)>>::call(s, fn_ptr, s.get_top());

				return s.push_nil();
			});

			state(L).get_info()->get_class(type_info)->add_function(TYPEINFO(Ix::value), v.value);
		}
		
		if constexpr (sizeof...(IA) > 0)
			self(self, L, std::forward<IA>(args)...);
	};

	lua_newclass(L);
	lua_classfunction(L, "create", create);
	lua_classmetamethod(L, "__gc", destroy);

	iterate_args(iterate_args, L, std::forward<A>(args)...);

	lua_registerclass(L, name.c_str());

	return true;
}

int main()
{
	SetThreadAffinityMask(GetCurrentThread(), 0);
	
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
			property("z", &vec3::set_z, &vec3::get_z),
			function("len", &vec3::length),
			function("add", &vec3::add)
			);
	}

	script.exec_string(R"(
function test()
	local a = vec3(1, 2, 3);
	local a2 = vec3(10, 20, 30);
	local a3 = getVec();

	a.y = 100.0;

	local l = a:len();
	local b = a:add(a2:add(a3));

	b.y = b.y + l;

	--print("------ " .. tostring(l) .. " -----");
	--print(tostring(a.x) .. ", " .. tostring(a.y) .. ", " .. tostring(a.z));
	--print(tostring(b.x) .. ", " .. tostring(b.y) .. ", " .. tostring(b.z));

	--collectgarbage();
end
)");

	script.add_function("getVec", [](state& s)
	{
		//printf_s("lol %.2f %.2f %.2f\n", obj->x, obj->y, obj->z);

		return vec3(10.f, 20.f, 30.f);
	});

	BringWindowToTop(GetConsoleWindow());

	while (true)
	{
		debug_memory();

		//printf_s("\n--------------------------\n");

		{
			TimeProfiling tp("time test");

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