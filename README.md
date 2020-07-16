## GavelScript
Gavelscript is a small dynamically-typed functional programming language, all located in a single header library! Why? Well, having this library in a single header makes it stupidly simple for developers to include in their projects! Gavelscript is commited to being as simple as possible for the developer to use, letting you have a powerful scripting langauge for your C++ application without the headaches.

<p align="center">
    <img src="https://github.com/CPunch/GavelScript/raw/master/pics/demo.gif" style="max-width:100%;">
    <img src="https://github.com/CPunch/GavelScript/raw/master/pics/demo2.gif" style="max-width:100%;">
</p>

Gavelscript is in active development, however here are some highlights:
- [X] Human readable syntax
- [X] Fast stack-based bytecode virtual machine with a Mark & Sweep garbage collector
- [X] Pratt based parser for quickly and easily compiling scripts to bytecode
- [X] Built-in serializer and deserializer, allowing you to write compiled Gavelscript functions to files, or precompile scripts in memory
- [ ] Comprehensive and easy to use C++ API
- [ ] Complete hand-written documentation including examples both for the language & C++ API

# Installation
While GavelScript is meant as an extension scripting library (meaning you add it to your pre-existing program) you can play with the base standard library by compiling main.cpp

first, clone this repo

```bash
git clone https://github.com/CPunch/GavelScript.git
```

then if you have the GNU C++ compiler install simply run `make` in the root directory. 

(You can always edit the Makefile to use clang or another drop-in replacement for g++)

after you've compiled the demo, run the output binary from bin/Gavel

```bash
chmod +x bin/Gavel && ./bin/Gavel
```

# Documentation
Documentation is located in the [wiki](../../wiki/About)! 