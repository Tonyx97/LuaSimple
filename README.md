# LuaSimple: C++20 wrapper for Lua

...why? because no other wrapper fits my needs, some have extreme compiler overhead and others are just too basic so I'm making my own one with the lowest template overhead to be able to compile several thousands of templated functions. This is WIP, I will upgrade and add new capabilities as I need them. Feel free to suggest features and fixes. The whole wrapper is located in [luas.h](LuaSimple/code/luas.h) as a single header file so all you need to do is include it in your project to use it.

# Features:

- Register Global Variables (including tables such as std::vector, std::map etc).
- Calling Lua functions from C++ and viceversa.
- Multiple Returns.
- Variadic Arguments.
- OOP Basic Support.
- Store Lua functions and call them from C++ whenever you want.
- Moving and modifying tables from C++ to Lua and viceversa.
- Since the wrapper focus on most important things, the code is not as templated as other wrappers so compile times are very acceptable for big projects. For example, in a high-end CPU it would take around 25 seconds to compile 8100 templated functions which is actually fast if you compare it to other wrappers (some of them can't even compile 8100 functions due to compiler overhead...).

# Possible Future Plans:

Metatables are an important feature as well, I'd like to implement them in the future when I need them.

# Documentation
# Global Variables

Let's start off with global variables. This is pretty straight forward, you can register common types in the lua context:

```cpp
luas::ctx script;

script.add_global("gv0", true);
script.add_global("gv1", 5.0);
script.add_global("gv2", -5.f);
script.add_global("gv3", "test string");

script.exec_string(R"(
print(gv0);
print(gv1);
print(gv2);
print(gv3);
)");

/* OUTPUT BELOW */
// true
// 5.0
// -5.0
// test string
```
- - - -
# Calling Lua Functions From C++

If you want to call a Lua function from your C++ code it's quite simple:

```cpp
luas::ctx script;

script.exec_string(R"(
function from_cpp(a, b, c)
  print(a);
  print(b);
  print(c);
	return a + b, "out string"
end
)");

std::tuple<float, std::string> result = script.call_safe<float, std::string>("from_cpp", 1.f, 2, ":o");

/* OUTPUT BELOW */
// 1.0
// 2
// :o
// 3 | out string
```
- - - -
# Calling C++ Functions From Lua

You can easily handle this:

```cpp
luas::ctx script;

// you can either return nothing, a single variable (without a tuple) or multiple
// variables using std::tuple
script.add_function("addEvent", [](float a, std::string str)
{
  std::cout << a << " | " << str << '\n';

  return std::make_tuple(a + 1.f, str + " received");
});

script.exec_string(R"(
local a, str = addEvent(10.0, "test");
print(a);
print(str);
)");

/* OUTPUT BELOW */
// 10 | test
// 11.0
// test received
```
- - - -
# Variadic Arguments

This wrapper supports variadic arguments, there is only one rule to use them, always declare them at the end of your arguments list:

```cpp
luas::ctx script;

script.add_function("addEvent", [](luas::variadic_args va)
{
  std::cout << "There is a total of " << va.size() << " arguments\n";
  std::cout << va.get<float>(0) << " | " << va.get<std::string>(1) << '\n';
});

script.exec_string(R"(
addEvent(10.0, "test");
)");

/* OUTPUT BELOW */
// There is a total of 2 arguments
// 10 | test
```
- - - -
# OOP

There is basic support for classes implementation:

```cpp
struct vec3
{
	float x = 0.f, y = 0.f, z = 0.f;

	vec3() {}
	vec3(float x, float y, float z) : x(x), y(y), z(z) {}
	~vec3() {}

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

luas::ctx script;

script.register_class<vec3, vec3(float, float, float)>(	// it only supports 1 constructor for now
  "vec3",
  luas::property("x", &vec3::set_x, &vec3::get_x),
  luas::property("y", &vec3::set_y, &vec3::get_y),
  luas::property("z", &vec3::set_z, &vec3::get_z),
  luas::function("len", &vec3::length),
  luas::function("add", &vec3::add)
);

// and then you can use it like this

script.exec_string(R("
local a = vec3(1, 2, 3);
local a2 = vec3(10, 20, 30);
local a3 = vec3(100, 200, 300);

local l = a:len();
local b = a:add(a2:add(a3));
"));
```
- - - -
# Store and Call Lua functions in C++

Some wrappers lack of this feature. In my opinion, it's a very important feature, mostly for game programming where you need to register events etc. You can send Lua functions from Lua to C++, store them and then call them later from your C++ code as well:

```cpp
luas::ctx script;

script.add_function("addEvent", [](std::string event_name, luas::lua_fn& fn) // lua_fn must be a reference
{
  std::cout << "Event name: " << event_name << '\n';

  // if you want to store the function, please use std::move
  // NOTE: keep in mind, you have to keep the reference so lua_fn isn't destroyed.
  // there are move operators overloaded to handle this properly so you must use std::move
  
  // 'call' in lua_fn also supports single and multiple returns
  //
  fn.call(1234);
});

script.exec_string(R"(
function someEvent(a)
  print("someEvent triggered: ".. tostring(a));
end
addEvent("printEvent", someEvent);
)");

/* OUTPUT BELOW */
// Event name: printEvent
// someEvent triggered: 1234

```
- - - -
# STL Containers (Tables)

STL containers are also support in both directions, from C++ to Lua and viceversa. You can also register global variables as std::vector, std::set, std::map etc. The usage is pretty straight forward:

```cpp
luas::ctx script;

struct Obj
{
  int val = 0;
};

script.add_function("getValue", [](Obj* v)
{
  return v->val;
});

script.add_function("getTable", []()
{
  std::unordered_map<Obj*, int> out;

  // don't mind the memory leaks here hehe
  
  out.insert({ new Obj { 100 }, 22 });
  out.insert({ new Obj { 101 }, 33 });
  out.insert({ new Obj { 102 }, 44 });
  out.insert({ new Obj { 103 }, 55 });
  out.insert({ new Obj { 104 }, 66 });
  out.insert({ new Obj { 105 }, 77 }); 

  return out;
});

script.exec_string(R"(
local test_table = getTable();
for k, v in pairs(test_table) do
  print("k: " .. tostring(getValue(k)) .. " | v: " .. tostring(v));
end
)");

/* OUTPUT BELOW */
// k: 100 | v: 22
// k: 105 | v: 77
// k: 102 | v: 44
// k: 104 | v: 66
// k: 103 | v: 55
// k: 101 | v: 33
```

And it works the other way around too:

```cpp
luas::ctx script;

script.add_function("printMap", [](std::unordered_map<std::string, int> m)
{
  for (const auto& [k, v] : m)
    std::cout << "k: " << k << " | v: " << v << '\n';
});

script.exec_string(R"(
local _table = {};
_table["str 1"] = 2;
_table["str 2"] = _table["str 1"] + 1;
_table["str 3"] = _table["str 2"] + 1;
printMap(_table);
)");

/* OUTPUT BELOW */
// k: str 3 | v: 4
// k: str 2 | v: 3
// k: str 1 | v: 2
```
