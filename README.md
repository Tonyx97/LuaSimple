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
