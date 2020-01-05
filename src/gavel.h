/* 

     ██████╗  █████╗ ██╗   ██╗███████╗██╗     
    ██╔════╝ ██╔══██╗██║   ██║██╔════╝██║     
    ██║  ███╗███████║██║   ██║█████╗  ██║     
    ██║   ██║██╔══██║╚██╗ ██╔╝██╔══╝  ██║     
    ╚██████╔╝██║  ██║ ╚████╔╝ ███████╗███████╗
     ╚═════╝ ╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚══════╝
        Copyright (c) 2019-2020, Seth Stubbs

Register-Based VM, Inspired by the Lua Source project :) 
    - Each instruction is encoded as a 32bit integer.
    - Stack-based VM with max 32 instructions (13 currently used)
    - dynamically-typed
    - basic control-flow
    - basic loops (while)
    - easily embeddable (hooray plugins!)
    - custom lexer & parser
    - user-definable c functionc
    - serialization support
    - and of course, free and open source!

    Just a little preface here, while creating your own chunks yourself is *possible* there are very
    little checks in the actual VM, so expect crashes. A lot of crashes. However if you are determined
    to write your own bytecode and set up the chunks & constants yourself, take a look at CREATE_i() and 
    CREATE_iAx(). These macros will make crafting your own instructions easier.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL RANDOLPH VOORHIES OR SHANE GRANT BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _GSTK_HPP
#define _GSTK_HPP

#include <iostream>
#include <iomanip>
#include <type_traits>
#include <vector>
#include <map>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <math.h>

// add x to show debug info
#define DEBUGLOG(x) 

// if this is defined, it will dump the stack after an objection is thrown
//#define _GAVEL_DUMP_STACK_OBJ

// if this is defined, all objections will be printed to the console
// #define _GAVEL_OUTPUT_OBJ

#ifndef BYTE // now we have BYTE for both VC++ & the G++ compiler
    #define BYTE unsigned char
#endif

// version info
#define GAVEL_MAJOR "0"
#define GAVEL_MINOR "1"

// basic syntax rules
#define GAVELSYNTAX_COMMENTSTART    '/'
#define GAVELSYNTAX_ASSIGNMENT      '='
#define GAVELSYNTAX_OPENSCOPE       '{'
#define GAVELSYNTAX_ENDSCOPE        '}'
#define GAVELSYNTAX_OPENCALL        '('
#define GAVELSYNTAX_ENDCALL         ')'
#define GAVELSYNTAX_STRING          '\"'
#define GAVELSYNTAX_SEPARATOR       ','
#define GAVELSYNTAX_ENDOFLINE       ';'

// control-flow
#define GAVELSYNTAX_IFCASE          "if"
#define GAVELSYNTAX_ELSECASE        "else"

// bool ops
#define GAVELSYNTAX_BOOLOPEQUALS    "=="
#define GAVELSYNTAX_BOOLOPLESS      '<'
#define GAVELSYNTAX_BOOLOPMORE      '>'

// hard-coded vars
#define GAVELSYNTAX_BOOLTRUE        "true"
#define GAVELSYNTAX_BOOLFALSE       "false"

// reserved words
#define GAVELSYNTAX_FUNCTION        "function"
#define GAVELSYNTAX_RETURN          "return"
#define GAVELSYNTAX_WHILE           "while"

// switched to 32bit instructions!
#define INSTRUCTION int

#define STACK_MAX 256
// this is for reserved stack space (for error info for objections)
#define STACKPADDING 1 

/* 
    Instructions & bitwise operations to get registers

        64 possible opcodes due to it being 6 bits. 13 currently used. Originally used positional arguments in the instructions for stack related operations, however that limited the stack size &
    that made the GavelCompiler needlessly complicated :(. This is an exeperimental project and i 100% expect it to crash randomly. please don't use this until i release a stable version haha. This was
    also a project I made for a blog post about creating a scripting language. This has become overly-compilcated so I'll either have to break the post into a bunch of parts, or just showcase it and 
    maybe highlight some key features. IDK, I'll figure it out.

    Instructions are 32bit integers with everything encoded in it, currently there are 2 types of intructions:
        - i
            - 'Opcode' : 5 bits
            - 'Reserved space' : 27 bits 
        - iAx
            - 'Opcode' : 5 bits
            - 'Ax' : 27 bits (1 bit for neg/pos)
*/
#define SIZE_OP		    5
#define SIZE_Ax		    27

#define POS_OP		    0
#define POS_A		    (POS_OP + SIZE_OP)

// creates a mask with `n' 1 bits
#define MASK(n)	            (~((~(INSTRUCTION)0)<<n))

#define GET_OPCODE(i)	    (((OPCODE)((i)>>POS_OP)) & MASK(SIZE_OP))
#define GETARG_Ax(i)	    (signed int)(((i)>>POS_A) & MASK(SIZE_Ax))

/* These will create bytecode instructions. (Look at OPCODE enum right below this)
    o: OpCode, eg. OP_POP
    a: Ax, eg. 1
*/
#define CREATE_i(o)	        (((INSTRUCTION)(o))<<POS_OP)
#define CREATE_iAx(o,a)	    ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A))

// ===========================================================================[[ VIRTUAL MACHINE ]]===========================================================================

typedef enum { // [MAX : 32] [CURRENT : 13]
    //              ===============================[[STACK MANIPULATION]]===============================
    OP_PUSHVALUE, //    iAx - pushes consts[Ax] onto the stack
    OP_POP, //          iAx - pops Ax values off the stack
    OP_GETVAR, //       i   - pushes vars[(string)stack[top]] onto the stack
    OP_SETVAR, //       i   - sets vars[stack[top - 1]] to stack[top] & calls OP_POP[2]
    OP_CALL, //         iAx - Ax is the number of arguments to be passed to stack[top - Ax - 1] (arguments + chunk is popped off of stack when returning!)
    OP_JMP, //          iAx - Ax is ammount of instructions to jump forwards by.
    OP_JMPBACK, //      iAx - Ax is ammount of insturctions to jump backwards by.
    //                  OP_JMP IS A DANGEROUS INSTRUCTION! (could technically jump into the const or local table and start executing data)

    OP_FUNCPROLOG, //   iAx - Ax is amount of identifiers on stack to set to vars. 
    OP_RETURN, //       i - marks a return, climbs chunk hierarchy until returnable chunk is found. 

    /*              ==============================[[TABLES && METATABLES]]==============================
    OP_INDEX, //        iAx
    OP_NEWINDEX,        iAx
    // */

    //              ==================================[[CONDITIONALS]]==================================
    OP_BOOLOP, //       iAx - tests stack[top] with stack[top - 1] then pushes result as a boolean onto the stack. Ax is the type of comparison to be made (BOOLOP)
    OP_TEST, //         iAx   - if stack[top] is true, continues execution, otherwise pc is jumped by Ax
    //                  OP_TEST IS A DANGEROUS INSTRUCTION!

    //              ===================================[[ARITHMETIC]]===================================
    OP_ARITH, //        iAx - does arithmatic with stack[top] to stack[top - 1]. Ax is the type of arithmatic to do (OPARITH). Result is pushed onto the stack

    //              ================================[[MISC INSTRUCTIONS]]===============================
    OP_END //           i   - marks the end of chunk
} OPCODE;

// used for Ax in the OP_ARITH instruction. this lets me squeeze all of the arith instructions into one opcode.
typedef enum {
    OPARITH_NONE,
    OPARITH_ADD,
    OPARITH_SUB,
    OPARITH_DIV,
    OPARITH_MUL,
    OPARITH_POW,
    OPARITH_NOT
} OPARITH;

typedef enum {
    BOOLOP_EQUALS,
    BOOLOP_LESS,
    BOOLOP_MORE,
    BOOLOP_LESSEQUALS,
    BOOLOP_MOREEQUALS
} BOOLOP;

typedef enum {
    GAVELSTATE_RESUME,
    GAVELSTATE_YIELD,
    GAVELSTATE_END,
    GAVELSTATE_PANIC, // state when an objection occurs
    GAVELSTATE_RETURNING // climbs chunk hierarchy until current chunk is returnable
} GAVELSTATE;

// type info for everything thats allowed on the stack
typedef enum {
    GAVEL_TNULL,
    GAVEL_TCHUNK, // information for a gavel chunk.
    GAVEL_TCFUNC, // lets us assign stuff like "print" in the VM to c++ functions
    GAVEL_TSTRING,
    GAVEL_TUSERVAR, // just a pointer honestly. TODO: let them define custom actions for operators. like equals, less than, more than, toString, etc.
    GAVEL_TDOUBLE, // double
    GAVEL_TBOOLEAN // bool
} GAVEL_TYPE;

// pre-defining stuff for compiler
class GState;
struct GValue;
struct _uservar;
union _gvalue;

// this will store debug info per-line! yay!
struct lineInfo {
    int endInst;
    int lineNum;
    lineInfo(int s, int l) { endInst = s; lineNum = l; }
};


typedef GValue* (*GAVELCFUNC)(GState*, int);
typedef bool (*GAVELUSERVAROP)(_uservar* t, _uservar* o); // t = this, o = other
typedef std::string (*GAVELUSERVARTOSTR)(_uservar* t);

struct _uservar {
    void* ptr;
    GAVELUSERVAROP equals;
    GAVELUSERVAROP lessthan;
    GAVELUSERVAROP morethan;
    GAVELUSERVARTOSTR tostring;
    // might add arithmetic operators in the future,,,,, but rn it'll just be the boolean operators that are supported

    _uservar(void* thing, GAVELUSERVAROP e, GAVELUSERVAROP l, GAVELUSERVAROP m, GAVELUSERVARTOSTR ts) {
        ptr = thing;
        equals = e;
        lessthan = l;
        morethan = m;
        tostring = ts;
    }
};

#define CREATECONST_NULL()      (GValue*)new GValueNull()
#define CREATECONST_BOOL(n)     (GValue*)new GValueBoolean(n)
#define CREATECONST_DOUBLE(n)   (GValue*)new GValueDouble(n)
#define CREATECONST_STRING(n)   (GValue*)new GValueString(n)
#define CREATECONST_CHUNK(n)    (GValue*)new GValueChunk(n)
#define CREATECONST_CFUNC(n)    (GValue*)new GValueCFunction(n)

#define READGAVELVALUE(x, type) reinterpret_cast<type>(x)->val

#define READGVALUEBOOL(x) READGAVELVALUE(x, GValueBoolean*)
#define READGVALUEDOUBLE(x) READGAVELVALUE(x, GValueDouble*)
#define READGVALUESTRING(x) READGAVELVALUE(x, GValueString*)
#define READGVALUECHUNK(x) READGAVELVALUE(x, GValueChunk*)
#define READGVALUECFUNC(x) READGAVELVALUE(x, GValueCFunction*)

// defines a chunk. each chunk has locals
class GChunk {
public:
    std::vector<INSTRUCTION> chunk;
    std::vector<lineInfo> debugInfo; 
    std::vector<GValue*> consts;
    bool returnable = false;
    bool scoped = true; // if this is false, when setVar is called, it will set the var in the chunk hierarchy. when false, it'll set it to the state.
    char* name;

    // these are not serialized! they are setup by the compiler/serializer
    std::map<std::string, GValue*> locals; // these are completely ignored
    GChunk* parent = NULL;

    // now with 2873648273275% less memory leaks ...
    GChunk(char* n, std::vector<INSTRUCTION> c, std::vector<lineInfo> d, std::vector<GValue*> cons): 
        name(n), chunk(c), debugInfo(d), consts(cons) {}

    GChunk(char* n, std::vector<INSTRUCTION> c, std::vector<lineInfo> d, std::vector<GValue*> cons, bool r): 
        name(n), chunk(c), debugInfo(d), consts(cons), returnable(r) {}

    GChunk(char* n, std::vector<INSTRUCTION> c, std::vector<lineInfo> d, std::vector<GValue*> cons, bool r, bool s): 
        name(n), chunk(c), debugInfo(d), consts(cons), returnable(r), scoped(s) {}


    // we'll have to define these later, because they reference GState && GValue and stuff
    ~GChunk();

    void setLocal(const char* key, GValue* var);
    void setVar(const char* key, GValue* var, GState* state = NULL);
    GValue* getVar(char* key, GState* state = NULL);
};

/* GValue
    This class is a baseclass for all GValue objects.
*/
class GValue {
public:
    BYTE type; // type info
    GValue() {}

    virtual bool equals(GValue&) { return false; };
    virtual bool lessthan(GValue&) { return false; };
    virtual bool morethan(GValue&) { return false; };
    virtual std::string toString() { return ""; };
    virtual std::string toStringDataType() { return ""; };
    virtual GValue* clone() {return new GValue(); };

    // so we can easily compare GValues
    bool operator==(GValue& other)
    {
        return this->equals(other);
    }

    bool operator<(GValue& other) {
        return this->lessthan(other);
    }

    bool operator>(GValue& other) {
        return this->morethan(other);
    }

    bool operator<=(GValue& other) {
        return this->lessthan(other) || equals(other);
    }

    bool operator>=(GValue& other) {
        return this->morethan(other) || equals(other);
    }

    GValue* operator=(GValue& other)
    {
        return this->clone();
    }
};

// TODO: remove this, use baseclass with GAVEL_TNULL type
class GValueNull : public GValue {
public:
    GValueNull() {}

    bool equals(GValue& other) {
        return other.type == type;
    }

    bool lessthan(GValue& other) {
        return false;
    }

    bool morethan(GValue& other) {
        return false;
    }

    std::string toString() {
        return "Null";
    }

    std::string toStringDataType() {
        return "[NULL]";
    }

    GValue* clone() {
        return CREATECONST_NULL();
    }
};

class GValueBoolean : public GValue {
public:
    bool val;

    GValueBoolean(bool b): val(b) {
        type = GAVEL_TBOOLEAN;
    }

    bool equals(GValue& other) {
        if (other.type == type) {
            return reinterpret_cast<GValueBoolean*>(&other)->val == val;
        }
        return false;
    }

    bool lessthan(GValue& other) {
        return false;
    }

    bool morethan(GValue& other) {
        return false;
    }

    std::string toString() {
        return val ? "true" : "false";
    }

    std::string toStringDataType() {
        return "[BOOLEAN]";
    }

    GValue* clone() {
        return CREATECONST_BOOL(val);
    }
};

class GValueDouble : public GValue {
public:
    double val;

    GValueDouble(double d): val(d) {
        type = GAVEL_TDOUBLE;
    }

    bool equals(GValue& other) {
        if (other.type == type) {
            return reinterpret_cast<GValueDouble*>(&other)->val == val;
        }
        return false;
    }

    bool lessthan(GValue& other) {
        if (other.type == type) {
            return val < reinterpret_cast<GValueDouble*>(&other)->val;
        }
        return false;
    }

    bool morethan(GValue& other) {
        if (other.type == type) {
            return val > reinterpret_cast<GValueDouble*>(&other)->val;
        }
        return false;
    }

    std::string toString() {
        std::stringstream stream;
        stream << val;
        return stream.str();
    }

    std::string toStringDataType() {
        return "[DOUBLE]";
    }

    GValue* clone() {
        return CREATECONST_DOUBLE(val);
    }
};

class GValueString : public GValue {
public:
    std::string val;

    GValueString(std::string b): val(b) {
        type = GAVEL_TSTRING;
    }

    bool equals(GValue& other) {
        if (other.type == type) {
            return reinterpret_cast<GValueString*>(&other)->val.compare(val) == 0;
        }
        return false;
    }

    bool lessthan(GValue& other) {
        return false;
    }

    bool morethan(GValue& other) {
        return false;
    }

    std::string toString() {
        return val;
    }

    std::string toStringDataType() {
        return "[STRING]";
    }

    GValue* clone() {
        return CREATECONST_STRING(val);
    }
};

class GValueChunk: public GValue {
public:
    GChunk* val;

    GValueChunk(GChunk* c): val(c) {
        type = GAVEL_TCHUNK;
    }

    bool equals(GValue& other) {
        if (other.type == type) {
            return reinterpret_cast<GValueChunk*>(&other)->val == val;
        }
        return false;
    }

    bool lessthan(GValue& other) {
        return false;
    }

    bool morethan(GValue& other) {
        return false;
    }

    std::string toString() {
        std::stringstream stream;
        stream << val->name;
        return stream.str();
    }

    std::string toStringDataType() {
        return "[CHUNK]";
    }

    GValue* clone() {
        return CREATECONST_CHUNK(val);
    }
};

class GValueCFunction : public GValue {
public:
    GAVELCFUNC val;

    GValueCFunction(GAVELCFUNC c): val(c) {
        type = GAVEL_TCFUNC;
    }

    bool equals(GValue& other) {
        if (other.type == type) {
            return reinterpret_cast<GValueCFunction*>(&other)->val == val;
        }
        return false;
    }

    bool lessthan(GValue& other) {
        return false;
    }

    bool morethan(GValue& other) {
        return false;
    }

    std::string toString() {
        std::stringstream stream;
        stream << val;
        return stream.str();
    }

    std::string toStringDataType() {
        return "[C FUNCTION]";
    }

    GValue* clone() {
        return CREATECONST_CFUNC(val);
    }
};

/* GStack
    Stack for GState. I would've just used std::stack, but it annoyingly hides the container from us in it's protected members :/
*/
class GStack {
private:
    GValue** container;
    std::vector<GValue*> garbage; // hold values we popped the code can still reference them unitl flush()
    int size;
    int top;

    bool isEmpty() {
        return top == -1;
    }

    bool isFull() {
        return top > (size-1 - STACKPADDING); // STACKPADDING is for error info
    }

public:
    
    GStack(int s = STACK_MAX) {
        container = new GValue*[s];
        size = s;
        top = -1;
    }

    ~GStack() {
        clearStack(); // garbage collect all GValues
        delete[] container;
    }

    /* flush()
        This will garbage collect all of the popped values. Only use this once you are done refrencing the popped values.
    */
    void flush() {
        for (GValue* val : garbage) { // goes through garbage, and frees all of the GValues
            delete val;
        }
        garbage.clear(); // garbage is now empty
    }

    GValue* pop(int times = 1) {
        if (isEmpty()) {
            return NULL;
        }

        // push GValue*(s) to garbage, and set to NULL
        for (int i = 0; i < times; i++) {
            GValue* temp = container[top];
            container[top--] = NULL;
            garbage.push_back(temp);
        }
        
        return container[(top >= 0) ? top : 0];
    }

    GValue* popAndFlush(int times = 1) {
        if (isEmpty()) {
            return NULL;
        }

        // clean the stack
        for (int i = 0; i < times; i++) {
            delete container[top];
            container[top--] = NULL;
        }

        // flush other values
        flush();
        
        return container[(top >= 0) ? top : 0];
    }

    void clearStack() {
        pop(top+1);
        flush();
        DEBUGLOG(std::cout << "cleared stack" << std::endl);
    }

    // pushes whatever datatype that was supplied to the stack as a GValue. If datatype is not supported, NULL is pushed onto stack.
    // returns new size of stack
    // if forcePush is true, it will ignore the isFull() result, and force the push onto the stack (DANGEROUS, ONLY USED FOR ERROR STRING)
    template <typename T>
    int push(T x, bool forcePush = false) {
        if constexpr (std::is_same<T, GAVELCFUNC>())
            return push(CREATECONST_CFUNC(x), forcePush);
        else if constexpr(std::is_same<T, GChunk*>())
            return push(CREATECONST_CHUNK(x), forcePush);
        else if constexpr (std::is_same<T, double>())
            return push(CREATECONST_DOUBLE(x), forcePush);
        else if constexpr (std::is_same<T, bool>())
            return push(CREATECONST_BOOL(x), forcePush);
        else if constexpr (std::is_same<T, char*>() || std::is_same<T, const char*>()) // we have to copy the string into a buffer.
            return push(CREATECONST_STRING(x), forcePush);
        else if constexpr (std::is_same<T, GValue>())
            return push(x.clone(), forcePush);
        
        return push(CREATECONST_NULL(), forcePush);
    }

    // pushes GValue onto stack.
    // returns new size of stack
    int push(GValue* t, bool forcePush = false) {
        if (isFull() && !forcePush) {
            return -1;
        }

        container[++top] = t->clone();
        return top; // returns the new stack size
    }

    GValue* getTop(int offset = 0) {
        if (isEmpty()) {
            // TODO: OBJECTION
            return NULL;
        }

        if (top - offset < 0) {
            return NULL;
        }
        return container[top - offset];
    }

    bool setTop(GValue* g, int offset = 0) {
         if (isEmpty()) {
            // TODO: OBJECTION
            return false;
        }

        if (top - offset < 0) {
            return false;
        }

        container[top - offset] = g;
        return true;
    }

    int getSize() {
        return top;
    }

    // DEBUG function to print the stack and label everything !! 
    void printStack() {
        std::cout << "\n=======================[[Stack Dump]]=======================" << std::endl;
        for (int i = 0; i <= top; ++i) {
            std::cout << std::setw(4) << std::to_string(i) + " - " << std::setw(20) << container[i]->toStringDataType() << std::setw(20) << container[i]->toString() << std::endl; 
        }
        std::cout << "\n============================================================" << std::endl;
    }
};

// defines stuff for GState
namespace Gavel {
    void executeChunk(GState* state, GChunk* chunk, int passedArguments = 0);
}

/* GState 
    This holds the stack, pc and debug info.
*/
class GState {
public:
    std::map<std::string, GValue*> globals; // when a _main is being executed, all locals are set to here.
    GStack stack;
    GAVELSTATE state;
    GChunk* debugChunk;
    INSTRUCTION* pc;

    GState() {
        state = GAVELSTATE_RESUME;
    }

    ~GState() {
        // clean up globals
        for (auto var : globals) {
            delete var.second;
        }
    }

    GValue* getTop(int i = 0) {
        return stack.getTop(i);
    }

    bool setTop(GValue* g, int i = 0) {
        return stack.setTop(g, i);
    }

    double toDouble(int i = 0) {
        GValue* t = getTop(i);
        if (t->type == GAVEL_TDOUBLE) {
            return READGVALUEDOUBLE(t);
        }
        else {
            // TODO: OBJECTION
            return 0;
        }
    }

    bool toBool(int i = 0) {
        GValue* t = getTop(i);
        if (t->type == GAVEL_TBOOLEAN) {
            return READGVALUEBOOL(t);
        }
        else {
            // TODO: OBJECTION
            return false;
        }
    }

    std::string toString(int i = 0) {
        GValue* t = getTop(i);
        if (t->type == GAVEL_TSTRING) {
            return READGVALUESTRING(t);
        }
        else {
            // TODO: OBJECTION
            return "";
        }
    }

    void setGlobal(const char* key, GValue* var) {
        DEBUGLOG(std::cout << "GLOBAL CALLED" << std::endl; std::cout << "setting " << key << " to " << var->toString() << std::endl);
        
        // if it's in locals, clean it up
        if (globals.find(key) != globals.end()) {
            DEBUGLOG(std::cout << "CLEANING UP " << globals[key]->toString() << std::endl);
            delete globals[key];
        }

        globals[key] = var->clone();
    }

    void throwObjection(std::string error) {
        std::stringstream os;
        int lNum = 0;
        // if there is no debug info, default to line 1.
        if (debugChunk->debugInfo.size() == 0) {
            lNum = 1;
        } else {
            // gets line info
            lineInfo* line = &debugChunk->debugInfo[0];
            int instrIndex = (pc - &debugChunk->chunk[0]) - 1;
            int i = 0;

            // increases lNum until it reaches the index where the instruction is in range. the last member of debuginfo has INT32MAX for both endInst and lineNum
            DEBUGLOG(std::cout << "calculated instruction index is " << instrIndex << std::endl);
            lNum = line->lineNum;
            for (int i = 1; i < debugChunk->debugInfo.size() && instrIndex >= debugChunk->debugInfo[i].endInst; i++) {
                line = &debugChunk->debugInfo[i];
                lNum = line->lineNum;
                DEBUGLOG(std::cout << "current line: " << lNum << " current endInst: " << line->endInst << std::endl);
            }
        }
        os << "[*] OBJECTION! in [" << debugChunk->name << "] (line " << lNum << ") \n\t" << error;

#ifdef _GAVEL_DUMP_STACK_OBJ
        // dump stack 
        stack.printStack();
#endif
#ifdef _GAVEL_OUTPUT_OBJ
        // output to console
        std::cout << os.str() << std::endl;
#endif

        // we now push the objection onto the stack
        state = GAVELSTATE_PANIC;
        stack.push(os.str().c_str(), true); // err, even if a stackoverflow is detected, there's always 1 extra GValue padding reserved
    }

    std::string getObjection() {
        if (state == GAVELSTATE_PANIC) {
            // error string is on the stacks
            return getTop()->toString();
        }
        return std::string("err. missing error info");
    }

    /* START(chunk)
        This will run a _main chunk and report any errors.
    */
    bool start(GChunk* main) {
        Gavel::executeChunk(this, main);
        if (state == GAVELSTATE_PANIC)
        {
            return false;
        }

        // our chunk is finished, we don't need this stack anymore
        stack.clearStack();
        return true;
    }
};

// Finish GChunk now that GState is defined lol

GChunk::~GChunk() {
    for (auto con: locals) {
        delete con.second;
    }

    // free constants (free child chunks aswell)
    for (GValue* con: consts) {
        switch (con->type) {
            case GAVEL_TCHUNK: { // free the child chunk
                delete READGVALUECHUNK(con);
                delete con;
                break; 
            }
            default:
                delete con;
        }
    }
}

void GChunk::setLocal(const char* key, GValue* var) {
    // if it's in locals, clean it up
    if (locals.find(key) != locals.end()) {
        DEBUGLOG(std::cout << "CLEANING UP " << locals[key]->toString() << std::endl);
        delete locals[key];
    }

    locals[key] = var->clone();
}

void GChunk::setVar(const char* key, GValue* var, GState* state) {
    if (locals.find(key) != locals.end()) { // if local exists in this chunk
        return setLocal(key, var);
    }

    if (parent != NULL) { // check if we have a parent. 
        // call parent setVar
        return parent->setVar(key, var, state);
    }
    // so we don't have a parent... real batman irl moment.

    // is this chunk scoped or is the var a global var? if so, check if we have a state.
    if ((!scoped && state != NULL) || (state != NULL && state->globals.find(key) != state->globals.end())) {
        state->setGlobal(key, var);
    } else { // var doesn't exist yet, so make it.
        setLocal(key, var);
    }

    return;
}

GValue* GChunk::getVar(char* key, GState* state) {
    if (locals.find(key) != locals.end()) { // check if var is in our locals
        return locals[key];
    }

    if (parent != NULL) { // parents???
        return parent->getVar(key, state);
    }

    // no???

    if (state != NULL && state->globals.find(key) != state->globals.end()) {
        return state->globals[key];
    }

    return CREATECONST_NULL();
}

/* VM Macros
    These help make the interpreter more modular. 
*/

#define ARITH_ADD(a,b) a + b
#define ARITH_SUB(a,b) a - b
#define ARITH_MUL(a,b) a * b
#define ARITH_DIV(a,b) a / b
#define ARITH_POW(a,b) a ^ b

#define iAx_ARITH(inst, op) GValue* _t = state->getTop(); \
    GValue* _t2 = state->getTop(1); \
    DEBUGLOG(std::cout << #op << "ing top 2 values on stack" << std::endl); \
    DEBUGLOG(state->stack.printStack()); \
    if (_t->type == _t2->type) { \
        switch (_t->type) { \
            case GAVEL_TDOUBLE: \
                state->stack.pop(2); \
                state->stack.push(ARITH_ ##op(READGVALUEDOUBLE(_t2), READGVALUEDOUBLE(_t))); \
                break; \
            default: \
                state->throwObjection("These datatypes cannot be " #op "'d together!"); \
                break; \
        } \
    } \
    else { \
        state->throwObjection("Attempt to perform arithmetic on two different datatypes! " + _t2->toStringDataType() + " with " + _t->toStringDataType()); \
    } \
    state->stack.flush(); // this will garbage collect our popped values

// Main interpreter
namespace Gavel {
    /* newGValue(<t> value)
        - value : value to turn into a GValue
        returns : GValue
    */
    template <typename T>
    GValue* newGValue(T x) {
        if constexpr (std::is_same<T, GAVELCFUNC>())
            return CREATECONST_CFUNC(x);
        else if constexpr(std::is_same<T, GChunk*>())
            return CREATECONST_CHUNK(x);
        else if constexpr (std::is_same<T, double>())
            return CREATECONST_DOUBLE(x);
        else if constexpr (std::is_same<T, bool>())
            return CREATECONST_BOOL(x);
        else if constexpr (std::is_same<T, char*>() || std::is_same<T, const char*>()) // we have to copy the string into a buffer.
            return CREATECONST_STRING(x);
        
        return CREATECONST_NULL();
    }

    /* print(a, ...)
        - a : prints this value. Can be any datatype!
        - ... : args passed can be endless (or just 128 args :/)
        returns : NULL
    */
    GValue* lib_print(GState* state, int args) {
        // for number of arguments, print
        for (int i = args; i >= 0; i--) {
            GValue* _t = state->getTop(i);
            switch (_t->type) {
                case GAVEL_TDOUBLE:
                    printf("%f", READGVALUEDOUBLE(_t)); // faster than using std::cout??
                    break;
                default:
                    std::cout << _t->toString();
                    break;
            }
        }
        printf("\n");

        // returns nothing, so return a null so the VM knows,
        return CREATECONST_NULL();
    }

    /* type(a)
        - a : any GValue type
        returns : a string representng the datatype of the GValue passed
    */
    GValue* lib_getType(GState* state, int args) { // -1 means no args, 0 means 1 argument, and so on.
        if (args != 0) 
        {
            state->throwObjection("Expected 1 argment, " + std::to_string(args+1) + " given!");
        }

        GValue* _t = state->getTop();

        // our VM will take care of popping all of the args and preserving the return value.
        return Gavel::newGValue(_t->toStringDataType().c_str());
    }

    /* stackdump()
        DESC: dumps the current stack.
        returns : null
    */
   GValue* lib_stackdump(GState* state, int args) {
       state->stack.printStack();
       return CREATECONST_NULL();
   }

    const char* getVersionString() {
        return "GavelScript " GAVEL_MAJOR "." GAVEL_MINOR;
    }

    void lib_loadLibrary(GState* state) {
        state->setGlobal("print", CREATECONST_CFUNC(lib_print));
        state->setGlobal("type", CREATECONST_CFUNC(lib_getType));
        state->setGlobal("stackdump", CREATECONST_CFUNC(lib_stackdump));
        state->setGlobal("__VERSION", CREATECONST_STRING(getVersionString()));
    }

    void executeChunk(GState* state, GChunk* chunk, int passedArguments) {
        state->state = GAVELSTATE_RESUME;
        state->debugChunk = chunk;
        state->pc = &chunk->chunk[0];
        bool chunkEnd = false;
        while((state->state == GAVELSTATE_RESUME || (state->state == GAVELSTATE_RETURNING && chunk->returnable)) && !chunkEnd) 
        {
            if (state->state == GAVELSTATE_RETURNING) { // it's returning and this chunk is returnable
                // at this point, gvalue that its returning is on the stack.
                DEBUGLOG(std::cout << "this chunk is returnable, continuing execution..." << std::endl);
                state->state = GAVELSTATE_RESUME;
                return;
            }
            INSTRUCTION inst = *(state->pc)++;
            DEBUGLOG(std::cout << "OP: " << GET_OPCODE(inst) << std::endl);
            switch(GET_OPCODE(inst))
            {
                case OP_PUSHVALUE: { // iAx
                    DEBUGLOG(std::cout << "pushing const[" << GETARG_Ax(inst) << "] to stack" << std::endl);
                    int ret = state->stack.push(chunk->consts[GETARG_Ax(inst)]);
                    if (ret == -1) { // stack is full!!! oh no!
                        state->throwObjection("Stack overflow!");
                    }
                    break;
                }
                case OP_POP: { // iAx
                    // for Ax times, pop stuff off the stack
                    int times = GETARG_Ax(inst);
                    state->stack.popAndFlush(times);
                    break;
                }
                case OP_GETVAR: { // i
                    GValue* top = state->getTop();
                    state->stack.pop(); // pops
                    if (top->type == GAVEL_TSTRING) {
                        DEBUGLOG(std::cout << "pushing " << top->toString() << " to stack" << std::endl); 
                        state->stack.push(chunk->getVar((char*)READGVALUESTRING(top).c_str(), state));
                    } else { // not a valid identifier
                        DEBUGLOG(std::cout << "NOT AN IDENTIFIER! "  << top->toStringDataType() << " : " << top->toString() << std::endl);
                        state->stack.push(CREATECONST_NULL());
                    }
                    state->stack.flush();
                    break;
                }
                case OP_SETVAR: { // i -- sets vars[stack[top-1]] to stack[top]
                    GValue* top = state->getTop(1);
                    GValue* var = state->getTop();
                    if (top->type == GAVEL_TSTRING && var != NULL) {
                        DEBUGLOG(std::cout << "setting " << var->toString() << " to var :" << top->toString() << std::endl); 
                        chunk->setVar((char*)READGVALUESTRING(top).c_str(), var, state); // a copy of the const is set to the var

                        // pops var + identifier
                        state->stack.popAndFlush(2);
                    } else { // not a valid identifier
                        state->throwObjection("Illegal identifier! String expected!"); // if this error occurs, PLEASE OPEN A ISSUE!!
                    }
                    break;
                }
                case OP_BOOLOP: { // i checks top & top - 1
                    BOOLOP  bop = (BOOLOP)GETARG_Ax(inst);
                    GValue* _t = state->getTop(1);
                    GValue* _t2 = state->getTop();
                    state->stack.pop(2); // pop the 2 vars
                    bool t = false;
                    DEBUGLOG(std::cout << _t->toString() << " BOOLOP[" << bop << "] " << _t2->toString() << std::endl);
                    switch (bop) {
                        case BOOLOP_EQUALS: 
                            t = *_t == *_t2;
                            break;
                        case BOOLOP_LESS:
                            t = *_t < *_t2;
                            break;
                        case BOOLOP_MORE:
                            t = *_t > *_t2;
                            break;
                        case BOOLOP_LESSEQUALS:
                            t = *_t <= *_t2;
                            break;
                        case BOOLOP_MOREEQUALS:
                            t = *_t >= *_t2;
                            break;
                        default:
                            break;
                    }
                    DEBUGLOG(std::cout << "result : " << (t ? "TRUE" : "FALSE") << std::endl);
                    state->stack.flush(); // garbage collect our popped values
                    state->stack.push(t); // this will use our already defined == operator in the GValue struct
                    break;
                }
                case OP_TEST: { // i
                    int offset = GETARG_Ax(inst);
                    if (!state->toBool(0)) {
                       // if false, skip next 2 instructions
                       DEBUGLOG(std::cout << "false! skipping chunk!" << std::endl);
                       state->pc += offset;
                    }
                    state->stack.popAndFlush(); // pop bool value
                    break;
                }
                case OP_JMP: { // iAx
                    int offset = GETARG_Ax(inst);
                    DEBUGLOG(std::cout << "jumping by " << offset << " instructions" << std::endl);
                    state->pc += offset; // jumps by Ax
                    break;
                }
                case OP_JMPBACK: { // iAx
                    int offset = GETARG_Ax(inst);
                    DEBUGLOG(std::cout << "jumping by " << -offset << " instructions" << std::endl);
                    state->pc -= offset; // jumps back by Ax
                    break;
                }
                case OP_FUNCPROLOG: {
                    DEBUGLOG(std::cout << "assiging function parameters STACK SIZE: " << state->stack.getSize() << " PASSED ARGS : " << passedArguments << std::endl);
                    int expectedArgs = GETARG_Ax(inst);

                    if (passedArguments != expectedArgs) {
                        state->debugChunk = chunk->parent;
                        state->throwObjection("Incorrect number of arguments were passed while trying to call " + std::string(chunk->name) + "! Expected " + std::to_string(expectedArgs) + ", got " + std::to_string(passedArguments) + ".");
                    }

                    GValue* ident; // expected to be a string. yes this is me being lazy and will probably cause 187326487126438 bugs later. oops
                    GValue* var;
                    for (int i = 0; i < expectedArgs; i++) { 
                        ident = state->getTop(i); // gets the identifier first
                        var = state->getTop(expectedArgs+i); // then the variable
                        if (ident->type == GAVEL_TSTRING && var != NULL) {
                            DEBUGLOG(std::cout << "setting " << var->toString() << " to var:" << ident->toString() << std::endl); 
                            chunk->setVar((char*)READGVALUESTRING(ident).c_str(), var); // setting a copy of the const to the var
                        } else { // not a valid identifier!!!! 
                            state->throwObjection("Illegal identifier! String expected!"); // almost 100% the parsers fault.... unless someone is crafting custom bytecode lol 
                        }
                    }

                    state->stack.popAndFlush((expectedArgs*2) + 1); // should pop everything :)
                    break;
                }
                case OP_CALL: { // iAx
                    int totalArgs = GETARG_Ax(inst);
                    DEBUGLOG(std::cout << "calling chunk at stack[" << totalArgs << "]" << std::endl);
                    GValue* top = state->getTop(totalArgs); // gets chunk off stack hopefully lol

                    switch (top->type)
                    {
                        case GAVEL_TCHUNK: { // it's a gavel chunk, so call gavel::executeChunk with chunk

                            /* basic idea:
                            - save pc to a local value here
                            - call executeChunk with state & GChunk of GValue
                            - reset pc and debugChunk
                            */
                            INSTRUCTION* savedPc = state->pc;
                            executeChunk(state, READGVALUECHUNK(top), totalArgs); // chunks are in charge of popping stuff (because they will also return a value)
                            state->pc = savedPc;
                            state->debugChunk = chunk;
                            break;
                        }
                        case GAVEL_TCFUNC: { // it's a c functions, so call the c function
                            GValue* ret = READGVALUECFUNC(top)(state, totalArgs-1); // call the c function with our state & number of parameters, value returned is the return value (if any)
                            state->stack.popAndFlush(totalArgs + 1); // pop args & chunk
                            state->stack.push(ret); // push return value
                            break;
                        }
                        default:
                            state->throwObjection("GValue is not a callable object! : " + top->toStringDataType());
                            break;
                    }
                    break;
                }
                case OP_ARITH: { // iAx
                    OPARITH aop = (OPARITH)GETARG_Ax(inst); // gets the type of arithmetic to do
                    switch (aop) // basically an opcode in an instruction
                    {
                        case OPARITH_ADD: { // this can also be used for strings.
                            if (state->getTop(1)->type == GAVEL_TSTRING || state->getTop()->type == GAVEL_TSTRING) {
                                GValue* _t = state->getTop(1);
                                GValue* _t2 = state->getTop();
                                std::string buf = _t->toString() + _t2->toString();
                                state->stack.popAndFlush(2); // pop the 2 values
                                state->stack.push(buf.c_str()); // push the string to the stack
                            } else {
                                iAx_ARITH(inst, ADD);
                            }
                            break;
                        }
                        case OPARITH_SUB: {
                            iAx_ARITH(inst, SUB);
                            break;
                        }
                        case OPARITH_DIV: {
                            iAx_ARITH(inst, DIV);
                            break;
                        }
                        case OPARITH_MUL: {
                            iAx_ARITH(inst, MUL);
                            break;
                        }
                        /*case OPARITH_POW:
                            iAB_ARITH(inst, POW);
                            break;*/
                        default:
                            state->throwObjection("OPCODE failure!");
                            break;
                    }
                    break;
                }
                case OP_RETURN: {
                    GValue* top = state->getTop();
                    state->stack.pop(passedArguments); // pops chunk, args, and return value
                    if (!chunk->returnable) { // pop args again
                        state->stack.pop();
                    }
                    state->stack.push(top); // pushes our return value!

                    DEBUGLOG(std::cout << "returning !" << std::endl);
                    
                    state->state = GAVELSTATE_RETURNING;
                    state->stack.flush();
                    break;
                }
                case OP_END: { // i
                    DEBUGLOG(std::cout << "END" << std::endl);
                    state->stack.push(CREATECONST_NULL()); // pushes the NULL return value, EVERY chunk has to return something (even unnamed chunks :pensive:)
                    chunkEnd = true;
                    break;
                }
                default:
                    state->throwObjection("VM PANIC");
                    break;
            }
        }
    }
}

// ===========================================================================[[ COMPILER/LEXER ]]===========================================================================

typedef enum {
    TOKEN_ASSIGNMENT,
    TOKEN_CONSTANT,
    TOKEN_VAR,
    TOKEN_ARITH,
    TOKEN_OPENCALL,
    TOKEN_ENDCALL,
    TOKEN_OPENSCOPE,
    TOKEN_ENDSCOPE,
    TOKEN_SEPARATOR,
    TOKEN_IFCASE,
    TOKEN_ELSECASE,
    TOKEN_FUNCTION,
    TOKEN_RETURN,
    TOKEN_WHILE,
    TOKEN_BOOLOP,
    TOKEN_ENDOFLINE, // marks ';'
    TOKEN_ENDOFFILE
} TOKENTYPE;

class GavelToken {
public:
    TOKENTYPE type;
    GavelToken() {}
    GavelToken(TOKENTYPE t) {
        type = t;
    }

    virtual ~GavelToken() { }
};

class GavelToken_Variable : public GavelToken {
public:
    std::string text;
    GavelToken_Variable() {}
    GavelToken_Variable(std::string s) {
        type = TOKEN_VAR;
        text = s;
    }
};

class GavelToken_Constant : public GavelToken {
public:
    GValue* cons;
    GavelToken_Constant() {}
    GavelToken_Constant(GValue* c) {
        type = TOKEN_CONSTANT;
        cons = c;
    }
};

class GavelToken_Arith : public GavelToken {
public:
    OPARITH op;
    GavelToken_Arith(OPARITH o) {
        type = TOKEN_ARITH;
        op = o;
    }
};

class GavelToken_BoolOp : public GavelToken {
public:
    BOOLOP op;
    GavelToken_BoolOp(BOOLOP o) {
        type = TOKEN_BOOLOP;
        op = o;
    }
};

#define CREATELEXERTOKEN_CONSTANT(x)    GavelToken_Constant(x)
#define CREATELEXERTOKEN_VAR(x)         GavelToken_Variable(x)
#define CREATELEXERTOKEN_ARITH(x)       GavelToken_Arith(x)
#define CREATELEXERTOKEN_BOOLOP(x)      GavelToken_BoolOp(x)
#define CREATELEXERTOKEN_ASSIGNMENT()   GavelToken(TOKEN_ASSIGNMENT)
#define CREATELEXERTOKEN_OPENCALL()     GavelToken(TOKEN_OPENCALL)
#define CREATELEXERTOKEN_ENDCALL()      GavelToken(TOKEN_ENDCALL)
#define CREATELEXERTOKEN_OPENSCOPE()    GavelToken(TOKEN_OPENSCOPE)
#define CREATELEXERTOKEN_ENDSCOPE()     GavelToken(TOKEN_ENDSCOPE)
#define CREATELEXERTOKEN_IFCASE()       GavelToken(TOKEN_IFCASE)
#define CREATELEXERTOKEN_ELSECASE()     GavelToken(TOKEN_ELSECASE)
#define CREATELEXERTOKEN_FUNCTION()     GavelToken(TOKEN_FUNCTION) 
#define CREATELEXERTOKEN_WHILE()        GavelToken(TOKEN_WHILE)
#define CREATELEXERTOKEN_RETURN()       GavelToken(TOKEN_RETURN)
#define CREATELEXERTOKEN_SEPARATOR()    GavelToken(TOKEN_SEPARATOR)
#define CREATELEXERTOKEN_EOL()          GavelToken(TOKEN_ENDOFLINE)
#define CREATELEXERTOKEN_EOF()          GavelToken(TOKEN_ENDOFFILE)

// this will build a GChunk from a scope!
class GavelScopeParser {
private:
    std::vector<int>* tokenLineInfo;
    std::vector<INSTRUCTION> insts;
    std::vector<GValue*> consts;
    std::vector<lineInfo> debugInfo;
    std::vector<GavelToken*>* tokenList;
    std::vector<GChunk*> childChunks;

    std::stringstream errStream;

    int args = 0;
    int* currentLine;
    char* name;
    bool returnable = false;
    bool objectionOccurred = false;

public:
    GavelScopeParser(std::vector<GavelToken*>* tl, std::vector<int>* tli, int* cl) {
        tokenList = tl;
        tokenLineInfo = tli;
        currentLine = cl;
        name = "unnamed chunk";
    }

    GavelScopeParser(std::vector<GavelToken*>* tl, std::vector<int>* tli, int* cl, char* n) {
        tokenList = tl;
        tokenLineInfo = tli;
        currentLine = cl;
        name = n;
    }

    template<typename T>
    int addConstant(T c) {
        if constexpr (std::is_same<T, GAVELCFUNC>())
            return addConstant(CREATECONST_CFUNC(c));
        else if constexpr(std::is_same<T, GChunk*>())
            return addConstant(CREATECONST_CHUNK(c));
        else if constexpr (std::is_same<T, double>())
            return addConstant(CREATECONST_DOUBLE(c));
        else if constexpr (std::is_same<T, bool>())
            return addConstant(CREATECONST_BOOL(c));
        else if constexpr (std::is_same<T, char*>() || std::is_same<T, const char*>())
            return addConstant(CREATECONST_STRING(c));
        
        return addConstant(CREATECONST_NULL());
    }

    // returns index of constant
    int addConstant(GValue *c) {
        for (int i = 0; i < consts.size(); i++)
            if (&consts[i] == &c) 
                return i;

        consts.push_back(c);
        
        DEBUGLOG(std::cout << "adding const[" << (consts.size() - 1) << "] : " << c->toString() << std::endl);
        return consts.size() - 1;
    }

    void addInstruction(INSTRUCTION i) {
        insts.push_back(i);
    }

    void setArgs(int a) {
        args = a;
    }

    GavelToken* peekNextToken(int i) {
        if (i >= tokenList->size()-1 || i < 0) {
            DEBUGLOG(std::cout << "end of token list! : " << i << std::endl);
            return new CREATELEXERTOKEN_EOF();
        }
        return (*tokenList)[i];
    }

    std::string getObjection() {
        return errStream.str();
    }

#ifdef _GAVEL_OUTPUT_OBJ 
    #define GAVELPARSEROBJECTION(err) \
        if (!objectionOccurred) { \
            errStream << "[*] OBJECTION! While parsing line " << (*currentLine)+1 << "\n\t" << err; \
            objectionOccurred = true; \
            std::cout << errStream.str() << std::endl; \
        }
#else
    #define GAVELPARSEROBJECTION(err) \
        if (!objectionOccurred) { \
            errStream << "[*] OBJECTION! While parsing line " << (*currentLine)+1 << "\n\t" << err; \
            objectionOccurred = true; \
        }
#endif 

    bool checkEOL(GavelToken* token) {
        return token->type == TOKEN_ENDOFFILE || token->type == TOKEN_ENDOFLINE;
    }

    // checks the end of a scope to make sure previous line was ended correctly
    bool checkEOS(int* indx) {
        switch(peekNextToken(*indx)->type) {
            case TOKEN_ENDOFLINE:
            case TOKEN_ENDOFFILE:
            case TOKEN_ENDSCOPE:
                return true;
            default:
                GAVELPARSEROBJECTION("Illegal syntax!");
                return false;
        }
    }

    void writeDebugInfo(int i) {
        if (tokenLineInfo->size() < *currentLine || tokenLineInfo->size() == 0)
            return;
        int markedToken = 0;
        do {
            if (*currentLine > 0)
                markedToken = (*tokenLineInfo)[*currentLine - 1];
            if (i > markedToken || *currentLine == 0)
            {
                (*currentLine)++;
                DEBUGLOG(std::cout << "writing line " << *currentLine << " at " << insts.size() << std::endl);
                debugInfo.push_back(lineInfo(insts.size(), *currentLine));
            } else {
                break;
            }
        } while(true);
    }

    void parseElse(int* indx) {
        // parse next line or scope
        if (peekNextToken(++(*indx))->type != TOKEN_OPENSCOPE) { // one liners are executed on the same chunk
            // parse next line
            parseLine(indx);
            checkEOS(indx);
        } else {
            (*indx)++; // skips OPEN_SCOPE token "{"
            GavelScopeParser scopeParser(tokenList, tokenLineInfo, currentLine);
            scopeParser.addInstruction(CREATE_iAx(OP_POP, 1));
            GChunk* scope = scopeParser.parseScopeChunk(indx);
            if (scope == NULL) {
                errStream << scopeParser.getObjection();
                objectionOccurred = true;
                return;
            }
            childChunks.push_back(scope);
            int chunkIndx = addConstant(scope);

            insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));
            insts.push_back(CREATE_iAx(OP_CALL, 0)); // calls a chunk with 0 arguments.
            insts.push_back(CREATE_iAx(OP_POP, 1)); // pops useless return value (NULL)*/
        }
    }

    // parse mini-scopes, like the right of =, or anything in ()
    GavelToken* parseContext(int* indx, bool arith = false) { // if it's doing arith, some tokens are forbidden
        do {
            GavelToken* token = peekNextToken(*indx);
            DEBUGLOG(std::cout << "CONTEXT TOKEN: " << token->type << std::endl);
            switch(token->type) {
                case TOKEN_CONSTANT: {
                    int constIndx = addConstant(dynamic_cast<GavelToken_Constant*>(token)->cons);
                    // pushes const onto stack
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, constIndx));
                    break;
                }
                case TOKEN_VAR: {
                    int varIndx = addConstant((char*)dynamic_cast<GavelToken_Variable*>(token)->text.c_str());
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, varIndx)); // pushes identifier onto stack
                    insts.push_back(CREATE_i(OP_GETVAR)); // pushes value of the identifier onto stack & pops identifier
                    break;
                }
                case TOKEN_ARITH: {
                    OPARITH op = dynamic_cast<GavelToken_Arith*>(token)->op;
                    (*indx)++;
                    
                    DEBUGLOG(
                    std::cout << "doing arith : ";
                    switch(op) {
                        case OPARITH_ADD:
                            std::cout << "add" << std::endl; 
                            break;
                        case OPARITH_SUB:
                            std::cout << "sub" << std::endl; 
                            break;
                        case OPARITH_MUL:
                            std::cout << "multiply" << std::endl; 
                            break;
                        case OPARITH_DIV:
                            std::cout << "divide" << std::endl; 
                            break;
                        default:
                            break;
                    });
                    
                    // get other tokens lol
                    GavelToken* retn = parseContext(indx, true);
                    insts.push_back(CREATE_iAx(OP_ARITH, op));

                    return retn;
                }
                case TOKEN_BOOLOP: {
                    if (arith) // if it's doing arithmetic, return
                    {
                        (*indx)--;
                        return token;
                    }
                    // parse everything to the right of the boolean operator. order of operations to come soon???
                    GavelToken* nxt = parseContext(&(++(*indx)));

                    BOOLOP op = dynamic_cast<GavelToken_BoolOp*>(token)->op;
                    insts.push_back(CREATE_iAx(OP_BOOLOP, op));
                    return nxt;
                }
                case TOKEN_OPENCALL: {
                    DEBUGLOG(std::cout << "calling .." << std::endl);
                    // check if very next token is ENDCALL, that means there were no args passed to the function
                    if (peekNextToken(++(*indx))->type == TOKEN_ENDCALL) {
                        insts.push_back(CREATE_iAx(OP_CALL, 0));
                        break;
                    }

                    // parse until ENDCALL
                    GavelToken* nxt;
                    int numArgs = 0; // keeps track of the arguments :)!
                    while(nxt = parseContext(indx)) {
                        // only 2 tokens are allowed, ENDCALL & SEPARATOR
                        if (nxt->type == TOKEN_ENDCALL) {
                            break; // stop parsing
                        } else if (nxt->type != TOKEN_SEPARATOR) {
                            GAVELPARSEROBJECTION("Illegal syntax! \"" "," "\" or \"" ")" "\" expected!");
                            return NULL;
                        }
                        numArgs++;
                        (*indx)++;
                    }
                    insts.push_back(CREATE_iAx(OP_CALL, numArgs+1));
                    break;
                }
                default:
                    // doesn't recognize token, return to caller
                    return token;
                    break;
            }
            (*indx)++;
            writeDebugInfo(*indx);
        } while(!objectionOccurred);

        return NULL;
    }

    // parse whole lines, like everything until a ;
    void parseLine(int* indx) {
        do {
            GavelToken* token = peekNextToken(*indx);
            DEBUGLOG(std::cout << "token[" << *indx << "] : " << token->type << std::endl);
            switch (token->type) {
                case TOKEN_VAR: {
                    GavelToken* peekNext = peekNextToken((*indx) + 1);
                    DEBUGLOG(std::cout << "peek'd token type : " << peekNext->type << std::endl);
                    int varIndx = addConstant((char*)dynamic_cast<GavelToken_Variable*>(token)->text.c_str()); // this looks ugly lol
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, varIndx)); // pushes identifier onto stack
                    
                    if (peekNext->type == TOKEN_ASSIGNMENT) {
                        (*indx)+=2;
                        DEBUGLOG(std::cout << "indx : " << *indx << std::endl);
                        GavelToken* nxt;
                        if (!checkEOL(parseContext(indx)))
                        {
                            GAVELPARSEROBJECTION("Illegal syntax!");
                            return;
                        }
                        insts.push_back(CREATE_i(OP_SETVAR));
                        return;
                    } else {
                        insts.push_back(CREATE_i(OP_GETVAR)); // pushes value of the identifier onto stack & pops identifier
                        break;
                    }
                }
                case TOKEN_OPENCALL: {
                    DEBUGLOG(std::cout << "calling .." << std::endl);
                    // check if very next token is ENDCALL, that means there were no args passed to the function
                    if (peekNextToken(++(*indx))->type == TOKEN_ENDCALL) {
                        insts.push_back(CREATE_iAx(OP_CALL, 0));
                        break;
                    }

                    // parse until ENDCALL
                    GavelToken* nxt;
                    int numArgs = 0; // keeps track of the arguments :)!
                    while(nxt = parseContext(indx)) {
                        // only 2 tokens are allowed, ENDCALL & SEPARATOR
                        if (nxt->type == TOKEN_ENDCALL) {
                            break; // stop parsing
                        } else if (nxt->type != TOKEN_SEPARATOR) {
                            GAVELPARSEROBJECTION("Illegal syntax! \"" "," "\" or \"" ")" "\" expected!");
                            return;
                        }
                        numArgs++;
                        (*indx)++;
                    }
                    insts.push_back(CREATE_iAx(OP_CALL, numArgs+1));
                    insts.push_back(CREATE_iAx(OP_POP, 1)); // we aren't using the returned value, so pop it off. by default every function returns NULL, unless specified by 'return'
                    break;
                }
                case TOKEN_IFCASE: {
                    DEBUGLOG(std::cout << " if case " << std::endl);

                    if (peekNextToken(++(*indx))->type != TOKEN_OPENCALL) {
                        GAVELPARSEROBJECTION("Illegal syntax! \"" "(" "\" expected after \"" GAVELSYNTAX_IFCASE "\"");
                        return;
                    }

                    GavelToken* nxt;
                    do {
                        (*indx)++;
                        nxt = parseContext(indx);

                        if (nxt->type == TOKEN_ENDCALL) {
                            break;
                        } else if (nxt->type != TOKEN_BOOLOP) {
                            GAVELPARSEROBJECTION("Illegal syntax! \"" ")" "\" expected!");
                            return;
                        }
                    } while(true);


                    // parse next line or scope
                    if (peekNextToken(++(*indx))->type != TOKEN_OPENSCOPE) { // one liners are executed on the same chunk
                        int savedInstrIndx = insts.size();
                        insts.push_back(0); // filler! this will be replaced with the correct offset after the next line is parsed.
                        
                        // parse next line
                        parseLine(indx);
                        checkEOS(indx);

                        if (peekNextToken(*indx + 1)->type == TOKEN_ELSECASE) { // has an else, handle it
                            DEBUGLOG(std::cout << " ELSE CASE!" << std::endl);
                            (*indx)++;
                            int savedElseInstrIndx = insts.size();
                            insts.push_back(0); // another filler for the else haha

                            // sets up the OP_TEST
                            insts[savedInstrIndx] = CREATE_iAx(OP_TEST, (insts.size() - 1) - savedInstrIndx);

                            // parse the else
                            parseElse(indx);
                            insts[savedElseInstrIndx] = CREATE_iAx(OP_JMP, (insts.size() - 1) - savedElseInstrIndx);
                        } else {
                            // creates OP_TEST with the offset of our last line!
                            insts[savedInstrIndx] = CREATE_iAx(OP_TEST, (insts.size() - 1) - savedInstrIndx);
                        }
                    } else {
                        // should this use chunks? scoped vars are BROKEN due to this. 
                        (*indx)++; // skips OPEN_SCOPE token "{"
                        GavelScopeParser scopeParser(tokenList, tokenLineInfo, currentLine);
                        scopeParser.addInstruction(CREATE_iAx(OP_POP, 1));
                        GChunk* scope = scopeParser.parseScopeChunk(indx);
                        if (scope == NULL) {
                            errStream << scopeParser.getObjection();
                            objectionOccurred = true;
                            return;
                        }
                        childChunks.push_back(scope);
                        int chunkIndx = addConstant(scope);
                        if (peekNextToken((*indx) + 1)->type == TOKEN_ELSECASE) { // has an else, handle it
                            DEBUGLOG(std::cout << " ELSE CASE!" << std::endl);
                            (*indx)++;
                            insts.push_back(CREATE_iAx(OP_TEST, 4)); // if it's false, skip the chunk call
                            insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));
                            insts.push_back(CREATE_iAx(OP_CALL, 0)); // calls a chunk with 0 arguments.
                            insts.push_back(CREATE_iAx(OP_POP, 1)); // pops useless return value (NULL)*/
                            
                            int savedInstrIndx = insts.size();
                            insts.push_back(0); // filler! this will be replaced with the correct offset after
                            parseElse(indx); // parse else 
                            
                            // jmps the else if the chunk was called
                            insts[savedInstrIndx] = CREATE_iAx(OP_JMP, (insts.size() - 1) - savedInstrIndx);
                        } else { // no else statement
                            insts.push_back(CREATE_iAx(OP_TEST, 3)); // if it's false, skip the chunk call
                            insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));
                            insts.push_back(CREATE_iAx(OP_CALL, 0)); // calls a chunk with 0 arguments.
                            insts.push_back(CREATE_iAx(OP_POP, 1)); // pops useless return value (NULL)*/
                        }
                    }

                    break; // lets us contine
                }
                case TOKEN_FUNCTION: {
                    DEBUGLOG(std::cout << "function" << std::endl);
                    GavelToken* nxt = peekNextToken(++(*indx));
                    char* functionName = (char*)dynamic_cast<GavelToken_Variable*>(nxt)->text.c_str();

                    if (nxt->type != TOKEN_VAR) {
                        GAVELPARSEROBJECTION("Illegal syntax! Identifier expected before \"(\"!");
                        return;
                    }

                    int varIndx = addConstant(functionName);
                    GavelScopeParser functionChunk(tokenList, tokenLineInfo, currentLine, (char*)dynamic_cast<GavelToken_Variable*>(nxt)->text.c_str());

                    nxt = peekNextToken(++(*indx));
                    if (nxt->type != TOKEN_OPENCALL) {
                        GAVELPARSEROBJECTION("Illegal syntax! \"" "(" "\" expected after identifier!");
                        return;
                    }

                    // get parameters it uses
                    int params = 0;
                    while(nxt = peekNextToken(++(*indx))) {
                        if (nxt->type == TOKEN_VAR) {
                            params++; // increment params lol
                            functionChunk.addInstruction(CREATE_iAx(OP_PUSHVALUE, functionChunk.addConstant((char*)dynamic_cast<GavelToken_Variable*>(nxt)->text.c_str())));
                            if (peekNextToken(++(*indx))->type == TOKEN_ENDCALL) {
                                break;
                            } else if (peekNextToken(*indx)->type != TOKEN_SEPARATOR) {
                                GAVELPARSEROBJECTION("Illegal syntax! Expected \",\" after identifier!");
                                return;
                            }
                        } else if (nxt->type == TOKEN_ENDCALL) {
                            break;
                        } else {
                            GAVELPARSEROBJECTION("Illegal syntax! Unexpected symbol in function definition!");
                            return;
                        }
                    }
                    // create function prolog 
                    functionChunk.addInstruction(CREATE_iAx(OP_FUNCPROLOG, params));

                    nxt = peekNextToken(++(*indx));
                    if (nxt->type == TOKEN_OPENSCOPE) {
                        (*indx)++;
                        GChunk* scope = functionChunk.parseFunctionScope(indx);
                        if (scope == NULL) {
                            errStream << functionChunk.getObjection();
                            objectionOccurred = true;
                            return;
                        }
                        scope->name = new char[strlen(functionName)]; // TODO: fix this awful memleak
                        strcpy(scope->name, functionName);
                        childChunks.push_back(scope);
                        int chunkIndx = addConstant(scope);
                        insts.push_back(CREATE_iAx(OP_PUSHVALUE, varIndx));
                        insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));
                        insts.push_back(CREATE_i(OP_SETVAR));
                    } else {
                        GAVELPARSEROBJECTION("Illegal syntax! Expected \"{\" after function definition!");
                        return;
                    }
                    break;
                }
                case TOKEN_RETURN: {
                    (*indx)++;

                    //insts.push_back(CREATE_iAx(OP_POP, 1)); // pops chunk
                    if (peekNextToken(*indx)->type == TOKEN_ENDOFLINE) {
                        insts.push_back(CREATE_iAx(OP_PUSHVALUE, addConstant(CREATECONST_NULL()))); // returns null
                        insts.push_back(CREATE_i(OP_RETURN));
                        break;
                    }
                    if (!checkEOL(parseContext(indx))) {
                        GAVELPARSEROBJECTION("Illegal syntax! Expected \";\"!");
                        return;
                    }

                    insts.push_back(CREATE_i(OP_RETURN));
                    return;
                }
                case TOKEN_WHILE: {
                    GavelToken* nxt = peekNextToken(++(*indx));
                    if (nxt->type != TOKEN_OPENCALL) {
                        GAVELPARSEROBJECTION("Illegal syntax! \"" "(" "\" expected after while!");
                        return;
                    }

                    int startPc = insts.size();

                    do { // parse boolean operator(s)
                        (*indx)++;
                        nxt = parseContext(indx);

                        if (nxt->type == TOKEN_ENDCALL) {
                            break;
                        } else {
                            GAVELPARSEROBJECTION("Illegal syntax! \"" ")" "\" expected!");
                            return;
                        }
                    } while(true);

                    // parse scope
                    if (peekNextToken(++(*indx))->type == TOKEN_OPENSCOPE) {
                        (*indx)++;
                        GavelScopeParser scopeParser(tokenList, tokenLineInfo, currentLine);
                        scopeParser.addInstruction(CREATE_iAx(OP_POP, 1)); // pops chunk
                        GChunk* scope = scopeParser.parseScopeChunk(indx);
                        if (scope == NULL) {
                            errStream << scopeParser.getObjection();
                            objectionOccurred = true;
                            return;
                        }
                        childChunks.push_back(scope);
                        int chunkIndx = addConstant(scope);

                        insts.push_back(CREATE_iAx(OP_TEST, 4)); // tests with offset of 5, so if it's false it'll skip 3 instructions 
                        insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));
                        insts.push_back(CREATE_iAx(OP_CALL, 0)); // calls a chunk with 0 arguments.
                        insts.push_back(CREATE_iAx(OP_POP, 1)); // pops useless return value (NULL)
                        insts.push_back(CREATE_iAx(OP_JMPBACK, (insts.size()-startPc + 1))); // 3rd instruction to skip
                    } else {
                        GAVELPARSEROBJECTION("Illegal syntax! \"" "{" "\" expected!");
                        return;
                    }
                    DEBUGLOG(std::cout << "loop end! continuing.." << std::endl);
                    break;
                }
                case TOKEN_OPENSCOPE: {
                    (*indx)++;
                    GavelScopeParser scopeParser(tokenList, tokenLineInfo, currentLine);
                    GChunk* scope = scopeParser.parseScopeChunk(indx);
                    if (scope == NULL) {
                        errStream << scopeParser.getObjection();
                        objectionOccurred = true;
                        return;
                    }
                    childChunks.push_back(scope);
                    int chunkIndx = addConstant(scope);
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));

                    return;
                }
                case TOKEN_ENDSCOPE:
                case TOKEN_ENDOFLINE: 
                case TOKEN_ENDOFFILE:
                    //(*indx)++;
                    return;
                default:
                    DEBUGLOG(std::cout << "unknown token! " << token->type << std::endl);
                    GAVELPARSEROBJECTION("Illegal syntax!");
                    return;
            }
            (*indx)++;
            writeDebugInfo(*indx);
        } while(!objectionOccurred);
    }

    void parseScope(int* indx) {
        DEBUGLOG(std::cout << "parsing new scope" << std::endl);
        do {
            parseLine(indx);
            checkEOS(indx);
        } while(!objectionOccurred && peekNextToken(*indx)->type != TOKEN_ENDSCOPE && peekNextToken((*indx)++)->type != TOKEN_ENDOFFILE);
        
        DEBUGLOG(std::cout << "exiting scope.." << std::endl);
    }

    GChunk* singleLineChunk(int* indx) {
        DEBUGLOG(std::cout << "parsing single line" << std::endl);

        parseLine(indx);
        if (!objectionOccurred)
            checkEOS(indx);
        
        insts.push_back(CREATE_i(OP_END));
        DEBUGLOG(std::cout << "exiting single line" << std::endl);
        if (!objectionOccurred)
            return new GChunk(name, insts, debugInfo, consts);
        else
            return NULL;
    }

    // this will parse a WHOLE scope, aka { /* code */ }
    GChunk* parseScopeChunk(int* indx) {
        parseScope(indx);

        insts.push_back(CREATE_i(OP_END));

        GChunk* c = NULL;
        if (!objectionOccurred)
        {
            c = new GChunk(name, insts, debugInfo, consts);

            // set all child parents to this chunk 
            for (GChunk* child : childChunks) {
                child->parent = c;
            }
        }

        return c;
    }

    GChunk* parseFunctionScope(int* indx) {
        GChunk* temp = parseScopeChunk(indx);
        if (temp != NULL)
            temp->returnable = true;
        return temp;
    }
};

class GavelCompiler {
private:
    std::vector<GavelToken*> tokenList;
    std::vector<INSTRUCTION> insts;
    std::vector<GValue> consts;
    std::vector<int> tokenLineInfo;
    const char* code;
    const char* currentChar;
    size_t len;

    std::string objection;

public:
    GavelCompiler(const char* c) {
        code = c;
        currentChar = c;
        len = strlen(c);
    }

    static bool isEmpty(char c) {
        switch (c)
        {
            case '\n':
            case ' ':
                return true;
            default:
                return false;
        }
    }

    static bool isNumeric(char c) {
        switch(c)
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.': // decimal numbers
                return true;
            default: // not a numeric
                return false;
        }
    }

    static OPARITH isOp(char c) {
        switch (c)
        {
            case '+':
                return OPARITH_ADD;
            case '-':
                return OPARITH_SUB;
            case '*':
                return OPARITH_MUL;
            case '/':
                return OPARITH_DIV;
            case '^':
                return OPARITH_POW;
            case '!':
                return OPARITH_NOT;
            default:
                return OPARITH_NONE;   
        }
    }

    char peekNext() {
        return *(currentChar + 1);
    }

    int nextLine() {
        int i = 0;
        while (*currentChar++ != '\n') { 
            i++; // idk i might use this result later /shrug
        } 
        return i;
    }

    GValue* readNumber(double mul = 1) {
        std::string num;

        while (isNumeric(*currentChar)) {
            num += *currentChar++; // adds char to string
        }

        *currentChar--;

        return CREATECONST_DOUBLE(std::stod(num.c_str()) * mul);
    }

    std::string* readString() {
        std::string* str = new std::string();

        while (*currentChar++) {
            if (*currentChar == GAVELSYNTAX_STRING)
            {
                break;
            }
            *str += *currentChar;
        }

        DEBUGLOG(std::cout << " string : " << *str << std::endl);

        return str;
    }

    std::string readIdentifier() {
        std::string name;

        // strings can be alpha-numeric + _
        while (isalpha(*currentChar) || isNumeric(*currentChar) || *currentChar == '_') {
            name += *currentChar++;
        }

        *currentChar--;

        return name;
    }

    GavelToken* previousToken() {
        if (tokenList.size() > 0)
            return tokenList[tokenList.size() - 1];
        return NULL;
    }

    GavelToken* nextToken() {
        GavelToken* token = NULL;
        while (true) {
            switch (*currentChar) {
                case GAVELSYNTAX_COMMENTSTART:
                    if (peekNext() == GAVELSYNTAX_COMMENTSTART) {
                        *currentChar++;
                        // it's a comment, so skip to next line
                        int length = nextLine();
                        DEBUGLOG(std::cout << " NEWLINE " << std::endl);
                        tokenLineInfo.push_back(tokenList.size()); // push number of tokens to mark end of line!
                    } else { // it's just / by itself, so it's trying to divide!!!!!
                        token = new CREATELEXERTOKEN_ARITH(OPARITH_DIV);
                    }
                    break;
                case GAVELSYNTAX_ASSIGNMENT: {
                    if (peekNext() == '=') {
                        // equals bool op
                        DEBUGLOG(std::cout << " == " << std::endl);
                        *currentChar++;
                        token = new CREATELEXERTOKEN_BOOLOP(BOOLOP_EQUALS);
                        break;
                    }
                    DEBUGLOG(std::cout << " = " << std::endl);
                    token = new CREATELEXERTOKEN_ASSIGNMENT();
                    break;
                }
                case GAVELSYNTAX_STRING: {
                    // read string, create const, and return token
                    token =  new CREATELEXERTOKEN_CONSTANT(CREATECONST_STRING(*readString()));
                    break;
                }
                case GAVELSYNTAX_OPENCALL: {
                    DEBUGLOG(std::cout << " ( " << std::endl);
                    token = new CREATELEXERTOKEN_OPENCALL();
                    break;
                }
                case GAVELSYNTAX_ENDCALL: {
                    DEBUGLOG(std::cout << " ) " << std::endl);
                    token = new CREATELEXERTOKEN_ENDCALL();
                    break;
                }
                case GAVELSYNTAX_OPENSCOPE: {
                    DEBUGLOG(std::cout << " { " << std::endl);
                    token = new CREATELEXERTOKEN_OPENSCOPE();
                    break;
                }
                case GAVELSYNTAX_ENDSCOPE: {
                    DEBUGLOG(std::cout << " } " << std::endl);
                    token = new CREATELEXERTOKEN_ENDSCOPE();
                    break;
                }
                case GAVELSYNTAX_SEPARATOR: {
                    DEBUGLOG(std::cout << " , " << std::endl);
                    token = new CREATELEXERTOKEN_SEPARATOR();
                    break;
                }
                case GAVELSYNTAX_ENDOFLINE: {
                    DEBUGLOG(std::cout << " EOL\n " << std::endl);
                    token = new CREATELEXERTOKEN_EOL();
                    break; 
                }
                case GAVELSYNTAX_BOOLOPLESS: {
                    if (peekNext() == '=') {
                        *currentChar++;
                        token = new CREATELEXERTOKEN_BOOLOP(BOOLOP_LESSEQUALS);
                    } else {
                        token = new CREATELEXERTOKEN_BOOLOP(BOOLOP_LESS);
                    }
                    break;
                }
                case GAVELSYNTAX_BOOLOPMORE: {
                    if (peekNext() == '=') {
                        *currentChar++;
                        token = new CREATELEXERTOKEN_BOOLOP(BOOLOP_MOREEQUALS);
                    } else {
                        token = new CREATELEXERTOKEN_BOOLOP(BOOLOP_MORE);
                    }
                    break;
                }
                case '\0': {
                    DEBUGLOG(std::cout << " EOF " << std::endl);
                    token = new CREATELEXERTOKEN_EOF();
                    break;
                }
                case '\n': {
                    DEBUGLOG(std::cout << " NEWLINE " << std::endl);
                    tokenLineInfo.push_back(tokenList.size()); // push number of tokens to mark end of line!
                    break;
                }
                default:
                    if (isEmpty(*currentChar)) {
                        // we don't care about whitespace. this is not a structured langauge...... lookin' at you python....
                        break;
                    }

                    // check for constants or identifier
                    if (isNumeric(*currentChar)) {
                        DEBUGLOG(std::cout << " number " << std::endl);
                        token = new CREATELEXERTOKEN_CONSTANT(readNumber());
                    } else if (isalpha(*currentChar) || *currentChar == '_') { // identifier
                        //======= [[ reserved words lol ]] =======
                        std::string ident = readIdentifier();

                        if (ident == GAVELSYNTAX_IFCASE) {
                            DEBUGLOG(std::cout << " IF " << std::endl);
                            token = new CREATELEXERTOKEN_IFCASE();
                        } else if (ident == GAVELSYNTAX_ELSECASE) {
                            DEBUGLOG(std::cout << " ELSE " << std::endl);
                            token = new CREATELEXERTOKEN_ELSECASE();
                        } else if (ident == GAVELSYNTAX_BOOLFALSE) {
                            DEBUGLOG(std::cout << " FALSE " << std::endl);
                            token = new CREATELEXERTOKEN_CONSTANT(CREATECONST_BOOL(false));
                        } else if (ident == GAVELSYNTAX_BOOLTRUE) {
                            DEBUGLOG(std::cout << " TRUE " << std::endl);
                            token = new CREATELEXERTOKEN_CONSTANT(CREATECONST_BOOL(true));
                        } else if (ident == GAVELSYNTAX_FUNCTION) {
                            DEBUGLOG(std::cout << " FUNCTION " << std::endl);
                            token = new CREATELEXERTOKEN_FUNCTION();
                        } else if (ident == GAVELSYNTAX_RETURN) {
                            DEBUGLOG(std::cout << " RETURN " << std::endl);
                            token = new CREATELEXERTOKEN_RETURN();
                        } else if (ident == GAVELSYNTAX_WHILE) {
                            DEBUGLOG(std::cout << " WHILE " << std::endl);
                            token = new CREATELEXERTOKEN_WHILE();
                        } else { // it's a variable
                            DEBUGLOG(std::cout << " var: " << ident << std::endl);
                            token = new CREATELEXERTOKEN_VAR(ident);
                        }
                    } else { 
                        OPARITH op = isOp(*currentChar);
                        GavelToken* prev = previousToken();
                        if (op == OPARITH_SUB && prev != NULL && (prev->type != TOKEN_CONSTANT && prev->type != TOKEN_VAR)) {
                            *currentChar++;
                            if (isNumeric(*currentChar)) {
                                DEBUGLOG(std::cout << " NEGATIVE CONSTANT " << std::endl);
                                token = new CREATELEXERTOKEN_CONSTANT(readNumber(-1));
                            } else if(isalpha(*currentChar)) { // turns -var into -1 * var (kinda hacky but it works, rip performance though.)
                                DEBUGLOG(std::cout << " NEGATIVE VAR " << std::endl);
                                token = new CREATELEXERTOKEN_VAR(readIdentifier());
                                tokenList.push_back(new CREATELEXERTOKEN_CONSTANT(CREATECONST_DOUBLE(-1)));
                                tokenList.push_back(new CREATELEXERTOKEN_ARITH(OPARITH_MUL));
                            }
                        } else if (op != OPARITH_NONE) {
                            DEBUGLOG(std::cout << " OPERATOR " << std::endl);
                            token = new CREATELEXERTOKEN_ARITH(op);
                        }
                    }
                    break;
            }
            *currentChar++;
            if (token != NULL) {
                return token;
            }
        }
    }

    void buildTokenList() {        
        while (currentChar < code+len) {
            GavelToken* t = nextToken();
            tokenList.push_back(t);
        }
    }

    void freeTokenList() {
        for (GavelToken* token : tokenList) {
            delete token; // free the our tokens
        }
        tokenList = {};
    }

    std::string getObjection() {
        return objection;
    }

    GChunk* compile() {
        DEBUGLOG(std::cout << "[*] COMPILING SCRIPT.." << std::endl);
        GChunk* mainChunk = NULL;

        // generate tokens
        buildTokenList();

        if (tokenList.size() > 0) {
            int i = 0;
            int lineNum = 0;
            GavelScopeParser scopeParser(&tokenList, &tokenLineInfo, &lineNum);
            mainChunk = scopeParser.parseScopeChunk(&i);
            if (mainChunk != NULL) {
                mainChunk->name = "_main";
                mainChunk->scoped = false;
            } else {
                objection = scopeParser.getObjection();
            }
        }

        // free memory used by tokens
        freeTokenList();

        DEBUGLOG(std::cout << "[*] DONE!" << std::endl);

        // build instruction & consts table
        return mainChunk;
    }
};

#undef GAVELPARSEROBJECTION

/* ===========================================================================[[ SERIALIZER ]]=========================================================================== 
    This will transform a Gavel Chunk (& all of it's children !) into a binary blob. This will have the following format:

    [HEADER]
        [6 bytes] - GAVEL[VERSION BYTE]
    
    Anatomy of a serialized chunk
        [1 byte] - Chunk datatype id
        [n bytes] - Chunk name [STRING]
            [4 bytes] - size of string
            [n bytes] - n characters
        [1 byte] - Bool datatype id
        [1 byte] - returnable flag
        [1 byte] - scoped flag
        [4 bytes] - number of constants
        [n bytes] - constant list
            [1 byte] - datatype id
                [n bytes] - [STRING]
                    [4 bytes] - size of string
                    [ n bytes] - n characters
                [8 bytes] - [DOUBLE]
                [1 byte] - [BOOL]
                [n bytes] - [CHUNK]
        [4 bytes] - number of instructions = i
        [2*i bytes] - instruction list
        [4 bytes] - size of debug info
        [n bytes] - debug info list
            [4 bytes] - endInstr
            [4 bytes] - line num
*/

// serial bytecode release version

#define GAVEL_VERSION_BYTE '\x02'

class GavelSerializer {
private:
    std::ostringstream data;

public:
    GavelSerializer() {}

    GavelSerializer(GChunk* c) {
        serialize(c);
    }

    void writeByte(BYTE b) {
        data.write(reinterpret_cast<const char*>(&b), sizeof(BYTE));
    }

    void writeSizeT(int s) {
        data.write(reinterpret_cast<const char*>(&s), sizeof(int));
    }

    void writeBool(bool b) {
        writeByte(GAVEL_TBOOLEAN);
        writeByte(b); // theres so much wasted space here ...
    }

    void writeDouble(double d) {
        writeByte(GAVEL_TDOUBLE);
        data.write(reinterpret_cast<const char*>(&d), sizeof(double));
    }

    void writeString(char* str) {
        int strSize = strlen(str);
        writeByte(GAVEL_TSTRING);
        writeSizeT(strSize); // writes size of string
        data.write(reinterpret_cast<const char*>(str), strSize); // writes string to stream!
    }

    void writeInstruction(INSTRUCTION inst) {
        data.write(reinterpret_cast<const char*>(&inst), sizeof(INSTRUCTION));
    }

    void writeConstants(std::vector<GValue*> consts) {
        writeSizeT(consts.size()); // number of constants!
        for (GValue* c : consts) {
            DEBUGLOG(std::cout << c->toStringDataType() << " : " << c->toString() << std::endl);
            switch(c->type) {
                case GAVEL_TBOOLEAN:
                    writeBool(READGVALUEBOOL(c));
                    break;
                case GAVEL_TDOUBLE:
                    writeDouble(READGVALUEDOUBLE(c));
                    break;
                case GAVEL_TSTRING:
                    writeString((char*)READGVALUESTRING(c).c_str());
                    break;
                case GAVEL_TCHUNK:
                    writeChunk(READGVALUECHUNK(c));
                    break;
                default: // TODO: be nicer about serializer errors
                    std::cout << "OBJECTION! In serializer. GValue [" << c->toStringDataType() << "] isn't supported!" << std::endl;
                    exit(0);
                    break;
            }
        }
    }

    void writeInstructions(std::vector<INSTRUCTION> insts) {
        writeSizeT(insts.size()); // ammount of instructions
        for (INSTRUCTION i : insts) {
            writeInstruction(i);
        }
    }

    void writeDebugInfo(std::vector<lineInfo> di) { // TODO: this is needlessly kinda fat. Maybe compress or ad option to remove debug info?
        writeSizeT(di.size()); // write size of data!
        for (lineInfo line : di) {
            writeSizeT(line.endInst); // writes endInst
            writeSizeT(line.lineNum); // then lineNum!
        }
    }

    void writeChunk(GChunk* chunk) {
        writeByte(GAVEL_TCHUNK);
        writeString(chunk->name);
        writeBool(chunk->returnable);
        writeBool(chunk->scoped);
        // first, write the constants
        writeConstants(chunk->consts);
        // then, write the instructions
        writeInstructions(chunk->chunk);
        // and finally, write the debug info!
        writeDebugInfo(chunk->debugInfo);
    }

    std::vector<BYTE> serialize(GChunk* chunk) {
        // write the header
        data.write("GAVEL", 5);
        data.put(GAVEL_VERSION_BYTE);
        
        writeChunk(chunk);

        std::string ssdata = data.str();
        std::vector<BYTE> rawBytes(ssdata.c_str(), ssdata.c_str() + ssdata.length());

        return rawBytes;
    }
    
    std::vector<BYTE> getData() {
        std::string ssdata = data.str();
        std::vector<BYTE> rawBytes(ssdata.c_str(), ssdata.c_str() + ssdata.length());

        return rawBytes;
    }
};

class GavelDeserializer {
private:
    std::vector<BYTE> data;
    int offset;
public:
    GavelDeserializer() {}
    GavelDeserializer(std::vector<BYTE> b): data(b) {
        offset = 0;
    }

    void read(char* buffer, int sz) {
        if (offset + sz > data.size()) { // overflow :(
            throwObjection("Serialized data is incomplete/malformed!");
            return;
        }

        memcpy(buffer, &data[0] + offset, sz);
        offset += sz;
    }

    BYTE getByte() {
        BYTE b = 0;
        read(reinterpret_cast<char*>(&b), sizeof(BYTE));
        return b;
    }

    int getSizeT() {
        int sz = 0;
        read(reinterpret_cast<char*>(&sz), sizeof(int));
        return sz;
    }

    bool getBool() {
        BYTE b = 0;
        read(reinterpret_cast<char*>(&b), sizeof(BYTE));
        return (bool)b;
    }

    double getDouble() {
        double db = 0.0; // :)
        read(reinterpret_cast<char*>(&db), sizeof(double));
        return db;
    }

    char* getString() {
        int sz = getSizeT();
        char* buf = new char[sz];
        read(buf, sz);
        buf[sz] = '\0';
        return buf; 
    }

    INSTRUCTION getInstruction() {
        INSTRUCTION inst = OP_END;
        read(reinterpret_cast<char*>(&inst), sizeof(INSTRUCTION));
        return inst;
    }

    std::vector<INSTRUCTION> getInstructions() {
        std::vector<INSTRUCTION> insts;
        int insts_size = getSizeT();
        for (int i = 0; i < insts_size; i++) {
            insts.push_back(getInstruction());
        }
        return insts;
    }

    std::vector<GValue*> getConstants(std::vector<GChunk*>& childChunks) {
        std::vector<GValue*> consts;
        int consts_size = getSizeT();
        for (int i = 0; i < consts_size; i++) {
            BYTE type = getByte();
            switch(type) {
                case GAVEL_TBOOLEAN: {
                    bool tmp = getBool();
                    DEBUGLOG(std::cout << "[BOOLEAN] : " << (tmp ? "TRUE" : "FALSE") << std::endl);
                    consts.push_back(CREATECONST_BOOL(tmp));
                    break;
                }
                case GAVEL_TDOUBLE: {
                    double tmp = getDouble();
                    DEBUGLOG(std::cout << "[DOUBLE] : " << tmp << std::endl);
                    consts.push_back(CREATECONST_DOUBLE(tmp));
                    break;
                }
                case GAVEL_TSTRING: {
                    char* buf = getString();
                    DEBUGLOG(std::cout << "[STRING] : " << buf << std::endl);
                    consts.push_back(CREATECONST_STRING(buf));
                    break;
                }
                case GAVEL_TCHUNK: {
                    GChunk* chk = getChunk();
                    childChunks.push_back(chk);
                    consts.push_back(CREATECONST_CHUNK(chk));
                    break;
                }
                default:
                    throwObjection("fail to deserialize type : " + std::to_string(type));
                    break;
            }
        }
        return consts;
    }

    std::vector<lineInfo> getDebugInfo() {
        std::vector<lineInfo> di;
        int num = getSizeT();
        for (int i = 0; i < num; i++) {
            int endInst = getSizeT();
            int lineNum = getSizeT();
            di.push_back(lineInfo(endInst, lineNum));
        }
        return di;
    }

    GChunk* getChunk() {
        std::vector<GChunk*> childChunks;

        if (getByte() != GAVEL_TSTRING)
            return NULL;
        char* name = getString();
        DEBUGLOG(std::cout << "CHUNK NAME: " << name << std::endl);

        if (getByte() != GAVEL_TBOOLEAN)
            return NULL;
        bool returnable = getBool();

        if (getByte() != GAVEL_TBOOLEAN)
            return NULL;
        bool scoped = getBool();

        std::vector<GValue*> consts = getConstants(childChunks);
        DEBUGLOG(std::cout << consts.size() << " Constants" << std::endl);
        std::vector<INSTRUCTION> insts = getInstructions();
        DEBUGLOG(std::cout << insts.size() << " Instructions" << std::endl);
        std::vector<lineInfo> debugInfo = getDebugInfo();

        GChunk* chunk = new GChunk(name, insts, debugInfo, consts, returnable, scoped);

        // parent the child chunks
        for (GChunk* chk : childChunks) {
            chk->parent = chunk;
        }

        return chunk;
    }

    void throwObjection(std::string err) {
        std::cout << "OBJECTION: " << err << std::endl;
    }

    GChunk* deserialize() {
        char header[] = {'G', 'A', 'V', 'E', 'L', GAVEL_VERSION_BYTE};
        if (memcmp(&data[0], header, 6) == 0) {
            offset+=6;
            DEBUGLOG(std::cout << "HEADER PASSED!" << std::endl);
            if (getByte() != GAVEL_TCHUNK)
                return NULL;
            return getChunk();
        }

        // header check failed!
        if (memcmp(&data[0], header, 5) == 0) {
            // invalid version!
            throwObjection("Invalid version!");
            return NULL;
        }

        std::cout << "HEADER CHECK FAILED!" << std::endl;
        return NULL;
    }

};

#endif // hi there :)