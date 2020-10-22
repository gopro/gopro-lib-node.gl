Technological choices
=====================

## C

Fast, simple, low-level and relatively portable. The OpenGL API is in C as
well.

### Why not C++?

We could spend decades elaborating on all the reasons why `C++` is a horrible
language, but instead we will recommend a few related reads:

- The author of ZeroMQ, [Why should I have written ZeroMQ in C, not C++ (part
  I)](http://250bpm.com/blog:4) and [part II](http://250bpm.com/blog:8)
- [Dorin Lazăr](https://dorinlazar.ro/why-c-sucks-2016-02-edition/)
- [Linus Torvalds on C++](http://harmful.cat-v.org/software/c++/linus)

## OpenGL

OpenGL is portable, mature and implementations are fast. Maybe it will be true
for Vulkan in the future, but until then, OpenGL is pretty much the only
option when targeting desktop and mobile.

## Python

It is very fast and simple to script in Python. Bindings in other languages
could be added.
