#pragma once

#define USE_DEBUG 1

#if USE_DEBUG
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

#include <iostream>
#endif

#include <intrin.h>
#include <type_traits>
#include <format>

#include <lua/lua.hpp>

#ifdef _DEBUG
#pragma comment(lib, "lua544_static_debug.lib")
#else
#pragma comment(lib, "lua544_static.lib")
#endif

#define FORMAT(t, a) std::vformat(t, std::make_format_args(a...))
#define FORMATV(t, ...) std::vformat(t, std::make_format_args(__VA_ARGS__))

#define LUA_GET_TYPENAME(i) lua_typename(_state, lua_type(_state, i))

extern "C"
{
	using error_callback_t = int(*)(const char*);

	inline error_callback_t fatal_error_callback = nullptr;
	inline error_callback_t error_callback = nullptr;
};

template <typename... A>
inline void check_fatal(bool condition, const char* text, A... args)
{
	if (condition)
		return;

	fatal_error_callback(FORMAT(text, args).c_str());
	exit(EXIT_FAILURE);
}

namespace luas
{
	struct lua_fn;
	class state;

	namespace detail
	{
		template <typename, template <typename...> typename>
		struct is_specialization : std::false_type {};

		template <template <typename...> typename C, typename... A>
		struct is_specialization<C<A...>, C> : std::true_type {};

		template <typename... A>
		concept is_empty_args = sizeof...(A) == 0;

		template <typename T>
		concept is_tuple = is_specialization<T, std::tuple>::value;

		template <typename T>
		concept is_string_array = std::is_array_v<T> && std::is_same_v<std::remove_all_extents_t<std::remove_cvref_t<T>>, char>;

		template <typename T>
		concept is_bool = std::is_same_v<T, bool>;

		template <typename T>
		concept is_integer = std::is_integral_v<T> && !is_bool<T>;

		template <typename T>
		concept is_integer_or_bool = is_integer<T> || is_bool<T>;

		template <typename T>
		concept is_state = std::is_same_v<T, state>;

		template <typename T>
		concept is_userdata = std::is_pointer_v<T>;

		template <typename T>
		struct fn_return_type { using type = T; };

		template <typename R, typename... A>
		struct fn_return_type<R(*)(A...)> { using type = R; };

		template <typename T>
		using fn_return_type_v = fn_return_type<T>::type;

		template <typename T>
		struct fn_args_count { static const int value = 0; };

		template <typename R, typename... A>
		struct fn_args_count<R(*)(A...)> { static const int value = sizeof...(A); };

		template <typename R, typename... A>
		struct fn_args_count<R(*)(state&, A...)> { static const int value = sizeof...(A); };

		template <typename T>
		static constexpr auto fn_args_count_v = fn_args_count<T>::value;

		struct TupleIterator
		{
			template <typename Fn, typename... A>
			static void iterate_impl(const Fn& fn) requires (sizeof...(A) == 0) {}

			template <typename Fn, typename T, typename... A>
			static void iterate_impl(const Fn& fn)
			{
				fn(T{});	// kinda hacky but compiler optimizes it

				iterate_impl<Fn, A...>(fn);
			}

			template <typename T>
			struct iterator { using type = T; };

			template <typename... A>
			struct iterator<std::tuple<A...>>
			{
				template <typename Fn>
				static void iterate(const Fn& fn) { iterate_impl<Fn, A...>(fn); }
			};
		};

		template <typename T>
		struct remove_member_ptr_fn { using type = void; };

		template <typename T, typename R, typename... A>
		struct remove_member_ptr_fn<R(T::*)(A...)> { typedef R type(A...); };

		template <typename T, typename R, typename... A>
		struct remove_member_ptr_fn<R(T::*)(A...) const> { typedef R type(A...); };

		template <typename T, typename R, typename... A>
		struct remove_member_ptr_fn<R(T::*)(A...) volatile> { typedef R type(A...); };

		template <typename T, typename R, typename... A>
		struct remove_member_ptr_fn<R(T::*)(A...) const volatile> { typedef R type(A...); };

		template <typename T>
		struct function_type { using type = std::decay<typename remove_member_ptr_fn<decltype(&std::decay<T>::type::operator())>::type>::type; };

		template <typename T>
		using function_type_v = function_type<T>::type;
	}

	namespace tuple
	{
		// for each
		//
		template <int i = 0, typename T, int size = std::tuple_size_v<std::decay_t<T>>, typename Fn, typename... A>
		constexpr void for_each(T&& tuple, Fn&& fn, A&&... args)
		{
			if constexpr (i < size)
			{
				std::invoke(fn, args..., std::get<i>(tuple));

				if constexpr (i + 1 < size)
					for_each<i + 1>(std::forward<T>(tuple), std::forward<Fn>(fn), std::forward<A>(args)...);
			}
		}

		// reverse for each
		//
		template <typename T, int i = std::tuple_size_v<std::decay_t<T>> - 1, typename Fn, typename... A>
		constexpr void for_each_reverse(T&& tuple, Fn&& fn, A&&... args)
		{
			if constexpr (i >= 0)
			{
				std::invoke(fn, args..., std::get<i>(tuple));

				if constexpr (i - 1 >= 0)
					for_each_reverse<T, i - 1>(std::forward<T>(tuple), std::forward<Fn>(fn), std::forward<A>(args)...);
			}
		}
	}

	class state
	{
	public:

		static inline void _on_error(lua_State* vm, const std::string& err)
		{
			lua_Debug dbg;

			lua_getstack(vm, 1, &dbg);
			lua_getinfo(vm, ">nSl", &dbg);

			std::string dbg_info;

			if (dbg.name)					dbg_info = FORMATV("[Fn: {}, Line {}] {}", dbg.name, dbg.currentline, err);
			else if (dbg.currentline != -1) dbg_info = FORMATV("[Line {}] {}", dbg.currentline, err);
			else							dbg_info = FORMATV("{}", err);

			error_callback(dbg_info.c_str());
		}

		template <typename T = int, typename... A>
		static inline T _throw_error(lua_State* vm, const std::string& err, const A&... args) { _on_error(vm, FORMATV(err, args...)); return T{}; }

	private:

		lua_State* _state = nullptr;

		template <typename... A>
		void throw_error(const std::string& err, const A&... args) { _on_error(_state, FORMATV(err, args...)); }

		void push_fn(const std::string& fn)
		{
			get_global(fn);

			if (!lua_isfunction(_state, -1))
				_throw_error(_state, "Function {} undefined", fn);
		}

		template <typename T, typename DT = std::remove_cvref_t<T>>
		void _push(const T& value) requires(detail::is_bool<DT>) { lua_pushboolean(_state, value); }

		template <typename T, typename DT = std::remove_cvref_t<T>>
		void _push(const T& value) requires(detail::is_integer<DT>) { lua_pushinteger(_state, value); }

		template <typename T, typename DT = std::remove_cvref_t<T>>
		void _push(const T& value) requires(std::is_floating_point_v<DT>) { lua_pushnumber(_state, value); }

		template <typename T, typename DT = std::remove_cvref_t<T>>
		void _push(const T& value) requires(std::is_same_v<DT, std::string>) { lua_pushstring(_state, value.c_str()); }

		template <typename T, typename DT = std::remove_cvref_t<T>>
		void _push(const T& value) requires(detail::is_string_array<DT>) { lua_pushstring(_state, value); }

		template <typename T, typename DT = std::remove_cvref_t<T>>
		void _push(const T& value) requires(detail::is_userdata<DT> && !std::is_trivial_v<std::remove_pointer_t<DT>>) { lua_pushlightuserdata(_state, value); }

		template <typename T, typename DT = std::remove_cvref_t<T>>
		DT _pop(int index) requires(detail::is_bool<DT>)
		{
			if (!lua_isboolean(_state, index))
				return _throw_error<DT>(_state, "Expected 'bool' value, got '{}'", LUA_GET_TYPENAME(index));

			return static_cast<DT>(lua_toboolean(_state, index));
		}

		template <typename T, typename DT = std::remove_cvref_t<T>>
		DT _pop(int index) requires(detail::is_integer<DT>)
		{
			if (!lua_isinteger(_state, index))
				return _throw_error<DT>(_state, "Expected 'integer' value, got '{}'", LUA_GET_TYPENAME(index));

			return static_cast<DT>(lua_tointeger(_state, index));
		}

		template <typename T, typename DT = std::remove_cvref_t<T>>
		DT _pop(int index) requires(std::is_floating_point_v<DT>)
		{
			if (!lua_isnumber(_state, index))
				return _throw_error<DT>(_state, "Expected 'number' value, got '{}'", LUA_GET_TYPENAME(index));

			return static_cast<DT>(lua_tonumber(_state, index));
		}

		template <typename T, typename DT = std::remove_cvref_t<T>>
		DT _pop(int index) requires(std::is_same_v<DT, std::string>)
		{
			if (!lua_isstring(_state, index))
				return _throw_error<DT>(_state, "Expected 'string' value, got '{}'", LUA_GET_TYPENAME(index));

			return DT(lua_tostring(_state, index));
		}

		template <typename T, typename DT = std::remove_cvref_t<T>>
		DT _pop(int index) requires(std::is_same_v<DT, lua_fn>);

		template <typename T, typename DT = std::remove_cvref_t<T>>
		DT _pop(int index) requires(detail::is_userdata<DT>)
		{
			if (!lua_isuserdata(_state, index))
				return _throw_error<DT>(_state, "Expected '{}' value, got '{}'", typeid(DT).name(), LUA_GET_TYPENAME(index));

			return std::bit_cast<DT>(lua_touserdata(_state, index));
		}

		void get_global(const std::string& name) { lua_getglobal(_state, name.c_str()); }

		bool call_protected(int nargs, int nreturns)
		{
			if (lua_pcall(_state, nargs, nreturns, 0) == 0)
				return true;

			_throw_error(_state, lua_tostring(_state, -1));

			return false;
		}

		template <typename T>
		void print_stack(int index)
		{
			printf_s("\n--------(index %i)---------\n", index);

			for (int i = -get_top(); i <= 0; ++i)
				printf_s("%i: %s (%s)\n", i, LUA_GET_TYPENAME(i), lua_type(_state, i) == LUA_TSTRING ? lua_tostring(_state, i) : "");
		}

	public:

		state() {}
		state(lua_State* _state) : _state(_state) {}
		~state() { make_invalid(); }

		void set_panic()
		{
			lua_atpanic(_state, [](lua_State* vm)
			{
				const auto str = lua_tostring(vm, -1);

				lua_pop(vm, 1);

				return fatal_error_callback(str);
			});
		}

		void make_invalid() { _state = nullptr; }
		void open_libs() { luaL_openlibs(_state); }
		void set_global(const char* index) { lua_setglobal(_state, index); }

		void unref(int v) { luaL_unref(_state, LUA_REGISTRYINDEX, v); }
		void push_value(int index) { lua_pushvalue(_state, index); }

		int ref() const { return luaL_ref(_state, LUA_REGISTRYINDEX); }
		int get_raw(int i, int n) const { return lua_rawgeti(_state, i, n); }
		int upvalue_index(int i) const { return lua_upvalueindex(i); }

		template <typename T>
		void push_c_closure(const T& value) { lua_pushcclosure(_state, value, 1); }

		template <typename T>
		T* new_userdata() { return static_cast<T*>(lua_newuserdata(_state, sizeof(T))); }

		template <typename T>
		T* to_userdata(int index) { return static_cast<T*>(lua_touserdata(_state, index)); }

		void exec_string(const std::string_view& code)
		{
			if (luaL_dostring(_state, code.data()) != 0)
				throw_error(lua_tostring(_state, -1));
		}

		void push() {}
		void pop_n(int n) { lua_pop(_state, n); }

		template <typename T, typename... A>
		void push(const T& value, A&&... args)
		{
			_push(value);
			push(args...);
		}

		template <bool pop_it = true, typename T, typename DT = std::remove_cvref_t<T>>
		DT pop(int index = -1)
		{
			auto value = _pop<T>(index);

			if (pop_it)
				pop_n(1);

			return value;
		}

		template <typename... A>
		bool call_safe_fn(const std::string& fn, int nreturns = 0, A&&... args)
		{
			push_fn(fn);
			push(args...);

			return call_protected(sizeof...(A), nreturns);
		}

		template <typename... A>
		bool call_safe(int nreturns = 0, A&&... args)
		{
			push(args...);

			return call_protected(sizeof...(A), nreturns);
		}

		int get_top() const { return lua_gettop(_state); }

		lua_State* get() const { return _state; }
		lua_State* operator * () const { return get(); }

		operator bool() const { return !!_state; }
	};

	template <typename Fn>
	struct lua_c_caller
	{
		template <typename... A, typename... In>
		static int push_and_call_impl(state& _s, int nargs, [[maybe_unused]] int index, In&&... args) requires (detail::is_empty_args<A>)
		{
			using return_type = detail::fn_return_type_v<Fn>;

			const auto pop_args = [&]() { if (nargs > 0) _s.pop_n(nargs); };
			const auto fn = *_s.to_userdata<Fn>(_s.upvalue_index(1));

			if constexpr (std::is_void_v<return_type>)
			{
				fn(args...);
				pop_args();
			}
			else
			{
				const auto ret = fn(args...);

				pop_args();

				if constexpr (detail::is_tuple<return_type>)
				{
					tuple::for_each(ret, [&]<typename T>(const T& item) { _s.push(item); });

					return std::tuple_size_v<return_type>;
				}
				else if constexpr (!std::is_void_v<return_type>)
				{
					static_assert(std::is_trivial_v<return_type>, "Return type is non-trivial");

					_s.push(ret);

					return 1;
				}
			}

			return 0;
		}

		template <typename T, typename... A, typename... In>
		static int push_and_call_impl(state& _s, int nargs, int index, In&&... args)
		{
			return push_and_call_impl<A...>(_s, nargs, index + 1, args..., _s.pop<false, T>(index));
		}

		template <typename T>
		struct caller { using type = T; };

		template <typename R, typename... A>
		struct caller<R(*)(A...)>
		{
			static int push_and_call(state& _s, int nargs) { return push_and_call_impl<A...>(_s, nargs, -nargs); }
		};
		
		template <typename R, typename... A>
		struct caller<R(*)(state&, A...)>
		{
			static int push_and_call(state& _s, int nargs) { return push_and_call_impl<A...>(_s, nargs, -nargs, _s); }
		};

		static int call(state& _s, int nargs) { return caller<Fn>::push_and_call(_s, nargs); }
	};

	struct lua_fn
	{
		state vm {};

		int ref = 0;

		lua_fn(state* _vm) : vm(_vm->get()) { ref = vm.ref(); }
		~lua_fn() { free_ref(); }

		lua_fn() {}
		lua_fn(const lua_fn&) = delete;
		lua_fn(lua_fn&& other) { *this = std::move(other); }

		void free_ref()
		{
			if (valid())
			{
				vm.unref(ref);
				vm.make_invalid();
			}
		}

		lua_fn& operator=(const lua_fn&) = delete;
		lua_fn& operator=(lua_fn&& other)
		{
			// free the current ref if we own one already

			free_ref();

			// move the ref

			if (other)
			{
				vm = other.vm;
				ref = std::exchange(other.ref, 0);

				other.vm.make_invalid();
			}

			return *this;
		}

		operator bool() const { return valid(); }

		// todo - use this shit to throw errors in calls etc
		//
		bool valid() const { return !!vm; }

		template <typename... T, typename... A>
		void call(A&&... args) requires(detail::is_empty_args<T...>)
		{
			vm.get_raw(LUA_REGISTRYINDEX, ref);
			vm.call_safe(0, args...);
		}

		template <typename... T, typename... A>
		std::tuple<T...> call(A&&... args)
		{
			std::tuple<T...> out {};

			vm.get_raw(LUA_REGISTRYINDEX, ref);

			if (vm.call_safe(sizeof...(T), args...))
				tuple::for_each_reverse(out, [&]<typename T>(T& item) { item = vm.pop<true, T>(); });

			return out;
		}
	};

	template <typename T, typename DT>
	DT state::_pop(int index) requires(std::is_same_v<DT, lua_fn>)
	{
		if (!lua_isfunction(_state, index))
			return _throw_error<DT>(_state, "Expected 'function' value, got '{}'", LUA_GET_TYPENAME(index));

		push_value(index);

		return lua_fn(this);
	}

	class ctx
	{
	private:

		/**
		 * VARIABLES
		 */

		state* vm = nullptr;

		/**
		 * FUNCTIONS
		 */

		template <typename T, typename Fn>
		void register_fn(const char* index, Fn&& fn)
		{
			const auto function = [](lua_State* _state) -> int
			{
				state vm(_state);

				static constexpr int expected_nargs = detail::fn_args_count_v<T>;

				if (const int nargs = vm.get_top(); expected_nargs == nargs)
					return lua_c_caller<T>::call(vm, nargs);
				else return state::_throw_error(*vm, "Expected {} arguments, got {}", expected_nargs, nargs);
			};

			const auto fn_obj_loc = vm->new_userdata<T>();

			check_fatal(fn_obj_loc, "Could not allocate placeholder for function");

			new (fn_obj_loc) T(std::move(fn));

			vm->push_c_closure(function);
			vm->set_global(index);
		}

	public:

		ctx(bool open_libs = true)
		{
			vm = new state(luaL_newstate());

			check_fatal(vm, "Could not allocate new");

			vm->set_panic();

			if (open_libs)
				vm->open_libs();
		}

		ctx(const ctx&) = delete;
		ctx(ctx&& other) = delete;

		~ctx()
		{
			lua_close(vm->get());

			delete vm;
		}

		ctx& operator=(const ctx&) = delete;
		ctx& operator=(ctx&&) = delete;

		void exec_string(const std::string_view& code) { vm->exec_string(code); }

		template <typename... T, typename... A>
		void call_safe(const std::string& fn, A&&... args) requires(detail::is_empty_args<T...>)
		{
			vm->call_safe_fn(fn, 0, args...);
		}

		template <typename... T, typename... A>
		std::tuple<T...> call_safe(const std::string& fn, A&&... args)
		{
			std::tuple<T...> out {};

			if (vm->call_safe_fn(fn, sizeof...(T), args...))
				tuple::for_each_reverse(out, [&]<typename T>(T& item) { item = vm->pop<true, T>(); });

			return out;
		}

		template <typename T>
		void add_function(const char* index, T&& fn)
		{
			register_fn<detail::function_type_v<T>>(index, std::forward<T>(fn));
		}

		template <typename T>
		void add_global(const char* index, T&& value)
		{
			vm->push(value);
			vm->set_global(index);
		}
	};
};