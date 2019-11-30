# GavelScript
This is a small single-header embeddable scripting language with an emphasis on embedability. This is still very much an experimental language. Please don't use this in actual projects yet haha.

Some features include:
- [X] Dynamically-typed
- [X] C-like syntax
- [X] User-definable C Functions which can be used in the GavelScript environment
- [ ] Usertypes, basically pointers that can be stored as GValues
- [X] If statements
- [ ] Simple control-flow with else, else if, etc.
- [ ] Loops 
- [ ] Functions
- [ ] Order of operations for boolean, and arithmetic operators
- [ ] Debug & Error handling

# Documentation

## Variables
Variables can be assigned to any type at any point. This is the flexability of having a dynamically-typed language. 

There are 4 main GavelScript types right now.

| DataType | Description | 
| ----------- | ----------- |
| Double | This can store any double, arithmetic can also be done on these |
| String | Stores a length of characters. In the VM these are just a normal C-String. | 
| Boolean | Stores true or false. Can be used in if statments for some simple control-flow |
| C Function | These are C Functions set by your C++ program. |

For example, to create a string use
```javascript
stringTest = "Hello World!";
```

or maybe you wanna know the answer to life the universe and everything
```javascript
doubleTest = 42;
```

## Arithmetic
This lets you do some math to double variables. Order of operations is not yet currently available :(

There are also 4 arithmetic operators available.

| Operator | Description |
| + | Addition |
| - | Subtraction |
| * | Multiplication |
| / | Division |

So, some simple maths would look like
```javascript
mathsTest = 3235*24;
```

## Boolean operators
This lets you ask questions and get their results. 

There are 4 main boolean operators so far.

| Operator | Description |
| == | Equals to |
| > | Less than |
| < | More than |
| <= | Less than or equals to |
| >= | More than or equals to |

So, some simple booleans would look like
```javascript
boolTest = 1 == 1;
```
or even just
```javascript
boolTestBetter = true;
```

## Functions 
This lets you call other chunks of code. However for now, you can only call C Functions that the C++ program has pre-defined for you.

Here are the default C Functions available

| Identifier | Description |
| print | Prints all arguments passed into std::cout |

So for example, calling print could be as easy as
```javascript
test = "Hello world! I am currently ";
myAge = 15+3;
print(test, myAge, " years old!");
```

Which would output:
> Hello world! I am currently 18 years old!

NOTICE: Due to some stack limitations, you CANNOT pass more than 255 arguments to any function! Sorry! This will probably change in the future.

Also to see how you can define your own C Functions see this foot note. [^1]

## If statements 
This lets for have some simple control flow over your script.

The syntax for the if statement looks like so
```javascript
if (true) { 
    // your script!!
}
```

Everything in between the brackets {} lets you define a scope. Everything in that scope will be executed. 

You could however just have a one-liner like
```if (1 == 1) 
    print("it's true!");
```

## Comments
These mark comments so you can document your script. These are ignored by the compiler.

```javascript
test = "hi!!!"; // makes a variable called test assigned to string "hi!!!"
```

# C API and how you can embed it in your projects!
NOTICE: This is constantly changing so please don't use this until I release a stable version!! For a more up-to-date version *check the example main.cpp!!*

So, I am made this project 1st, to have the bragging rights of "I made my own scripting langauge" and 2nd, so I can embed it in future projects where people might want to add their own behavior to it.
If you think the API should be different or made to be easier, please open an issue!!! 

First of all, to add GavelScript to your project, just include the header! No need to worry about "downloading/compiling libraries" GavelScript is all self-contained and made using pure C++17 features.
```c++
#include "gavel.h"
```

Now, to run a script you'll first have to create a GState. This will hold your stack and direct your program flow.

You can make a GState just like so:

```c++
GState* yaystate = new GState();
```

Now you'll need to generate the chunk for your script. This can be done easily like so:
```c++
GavelCompiler testScript(R"(
    testVar = 200*2.5;
    testVar2 = testVar + 3;
    boolTest = false == 1 == 2;
    if (boolTest) { // this is a comment and will be ignored by the compiler!!!
        print("((200*2.5)+3)/2 = ", testVar2 / 2);
        if (testVar2 > 500) { // comparing 503 > 500 so should be true
            print("hi : ", testVar);
        }
    }
    print("i should always print! goodbye!!!");
)");
_gchunk mainChunk = testScript.parse();
```

Okay, so now that you have a compiled GavelScript chunk, you'll need to add the base libraries to it.

```c++
Gavel::lib_loadLibrary(&mainChunk);
```

Then you can run it!
```c++
Gavel::executeChunk(yaystate, &mainChunk);
```

The output for that script btw looks like:
> ((200*2.5)+3)/2 = 251.5
> hi : 500
> i should always print! goodbye!!!"

# Define custom C Functions
[^1]: defining custom C Functions
To add your own C Function to be used in a GavelScript, you're going to add it to the enivronment of your mainChunk.

Gavel C Functions also have a specific syntax. They need to return a GValue, and accept both a GState* and int arguments. 
For example the C Function for print looks like:
```c++
GValue lib_print(GState* state, int args) {
    // for number of arguments, print + pop
    for (int i = args; i >= 0; i--) {
        GValue* _t = state->getTop(i);
        std::cout << _t->toString();
    }
    std::cout << std::endl;

    // returns nothing, so return a null so the VM knows,
    return CREATECONST_NULL();
}
```

now to actually add it to the chunk's environment, you use:
```c++
GChunk::setVar(&mainChunk, "print", new CREATECONST_CFUNC(lib_print));
```
    