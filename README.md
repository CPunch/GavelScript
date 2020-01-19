# GavelScript
This is a small single-header embeddable scripting language with an emphasis on [embeddability](#capi) and human-readability. This is still very much an experimental language, so please don't use this in actual projects yet haha.

![demo png](pics/GavelScriptDemo.gif "This is src/main.cpp")

Some features include:
- [X] Dynamically-typed
- [X] Simple human-readable syntax (Srry Khang)
- [X] Serialization
- [X] User-definable C Functions which can be used in the GavelScript environment
- [ ] Usertypes, basically pointers that can be stored as GValues *In progress!*
- [X] If statements
- [X] Simple control-flow with else, else if, etc.
- [X] While Loops
- [X] Simple Data Structures (Tables!)
- [X] Functions, and return values **Experimental!**
- [X] Debug & Error handling **Experimental!**
- [ ] Order of operations for boolean, and arithmetic operators

> NOTICE: Error-handling is still experimental, don't expect it to be correct 100% of the time. Even line numbers can be wrong. If you run into an issue, 99% of the time it's probably gavel's fault.

# Documentation

Documentation is located in the [wiki](wiki/About)! 