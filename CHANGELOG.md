# GavelScript 0.4 Changes.
> There's been numerous memory leaks fixed.
> better memory management for GValueTables
> signifigant performance improvements.
> faster scopes (locals now properly exist!)
> state->callFunction(GChunk*, Args&&...) added, letting you call GavelScript-defined functions!
> fixed embarrasing bugs
> serializer fixes and improvements to support the new scope system
> more lua-like syntax. ';' only separate instructions if they're on the same line. eg. "i = 0; foo()"