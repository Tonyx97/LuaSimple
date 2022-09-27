# LuaSimple: C++20 wrapper for Lua

...why? because no other wrapper fits my needs, some have extreme compiler overhead and others are just too basic so I'm making my own one with the lowest template overhead to be able to compile several thousands of templated functions. This is WIP, I will upgrade and add new capabilities as I need them. Feel free to suggest features and fixes. The whole wrapper is located in [luas.h](LuaSimple/code/luas.h) as a single header file so all you need to do is include it in your project to use it.

# Documentation
- - - -
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

