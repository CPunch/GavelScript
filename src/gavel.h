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
    - Stack-based VM with max 64 instructions (16 currently used)
    - dynamically-typed
    - basic control-flow
    - basic loops (while)
    - easily embeddable (hooray plugins!)
    - custom lexer & parser
    - user-definable c functions
    - serialization support
    - and of course, free and open source!

    Just a little preface here, while creating your own chunks yourself is *possible* there are very
    little checks in the actual VM, so expect crashes. A lot of crashes. However if you are determined
    to write your own bytecode and set up the chunks & constants yourself, take a look at CREATE_i() and 
    CREATE_iAx(). These macros will make crafting your own instructions easier.
*/

#ifndef _GSTK_HPP
#define _GSTK_HPP

#include <iostream>
#include <iomanip>
#include <type_traits>
#include <vector>
#include <map>
#include <unordered_map>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <math.h>

// add x to show debug info
#define DEBUGLOG(x) 

// if this is defined, it will dump the stack after an objection is thrown
// #define _GAVEL_DUMP_STACK_OBJ

// if this is defined, all objections will be printed to the console
// #define _GAVEL_OUTPUT_OBJ

#ifndef BYTE // now we have BYTE for both VC++ & the G++ compiler
    #define BYTE unsigned char
#endif

// version info
#define GAVEL_MAJOR "0"
#define GAVEL_MINOR "3"

// basic syntax rules
#define GAVELSYNTAX_COMMENTSTART    '/'
#define GAVELSYNTAX_ASSIGNMENT      '='
#define GAVELSYNTAX_OPENSCOPE       "do"
#define GAVELSYNTAX_ENDSCOPE        "end"
#define GAVELSYNTAX_OPENCALL        '('
#define GAVELSYNTAX_ENDCALL         ')'
#define GAVELSYNTAX_STRING          '\"'
#define GAVELSYNTAX_SEPARATOR       ','
#define GAVELSYNTAX_MINIINDEX       '.'
#define GAVELSYNTAX_ENDOFLINE       ';'

// tables
#define GAVELSYNTAX_OPENINDEX       '['
#define GAVELSYNTAX_ENDINDEX        ']'
#define GAVELSYNTAX_OPENTABLE       '{'
#define GAVELSYNTAX_CLOSETABLE      '}'

// control-flow
#define GAVELSYNTAX_STARTCONDITIONAL "if"
#define GAVELSYNTAX_ENDCONDITIONAL  "then"

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

        64 possible opcodes due to it being 6 bits. 16 currently used. Originally used positional arguments in the instructions for stack related operations, however that limited the stack size &
    that made the GavelCompiler needlessly complicated :(. Instruction encoding changes on the daily vro.

    Instructions are 32bit integers with everything encoded in it, currently there are 2 types of intructions:
        - i
            - 'Opcode' : 6 bits
            - 'Reserved space' : 26 bits 
        - iAx
            - 'Opcode' : 6 bits
            - 'Ax' : 26 bits [MAX: 67108864]
        - iAB
            - 'Opcode' : 6 bits
            - 'A' : 13 bits [MAX: 8192]
            - 'B' : 13 bits [MAX: 8192]
*/
#define SIZE_OP		    6
#define SIZE_Ax		    26
#define SIZE_A		    13
#define SIZE_B		    13

#define MAXREG_Ax       pow(2, SIZE_Ax)
#define MAXREG_A        pow(2, SIZE_A)
#define MAXREG_B        pow(2, SIZE_B)

#define POS_OP		    0
#define POS_A		    (POS_OP + SIZE_OP)
#define POS_B		    (POS_A + SIZE_A)

// creates a mask with `n' 1 bits
#define MASK(n)	            (~((~(INSTRUCTION)0)<<n))

#define GET_OPCODE(i)	    (((OPCODE)((i)>>POS_OP)) & MASK(SIZE_OP))
#define GETARG_Ax(i)	    (signed int)(((i)>>POS_A) & MASK(SIZE_Ax))
#define GETARG_A(i)	        (signed int)(((i)>>POS_A) & MASK(SIZE_A))
#define GETARG_B(i)	        (signed int)(((i)>>POS_B) & MASK(SIZE_B))

/* These will create bytecode instructions. (Look at OPCODE enum right below this)
    o: OpCode, eg. OP_POP
    a: Ax, eg. 1
*/
#define CREATE_i(o)	        (((INSTRUCTION)(o))<<POS_OP)
#define CREATE_iAx(o,a)	    ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A))
#define CREATE_iAB(o,a,b)   ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A) | (((INSTRUCTION)(b))<<POS_B))


// ===========================================================================[[ VIRTUAL MACHINE ]]===========================================================================

typedef enum { // [MAX : 64] [CURRENT : 16]
    //              ===============================[[STACK MANIPULATION]]===============================
    OP_PUSHVALUE, //    iAx - pushes consts[Ax] onto the stack
    OP_POP, //          iAx - pops Ax values off the stack
    OP_GETVAR, //       iAx - pushes vars[Ax] onto the stack
    OP_SETVAR, //       iAx - sets vars[Ax] to stack[top] 
    OP_CALL, //         iAx - Ax is the number of arguments to be passed to stack[top - Ax - 1] (arguments + chunk is popped off of stack when returning!)
    OP_JMP, //          iAx - Ax is ammount of instructions to jump forwards by.
    OP_JMPBACK, //      iAx - Ax is ammount of insturctions to jump backwards by.
    //                  OP_JMP IS A DANGEROUS INSTRUCTION! (could technically jump into the const or local table and start executing data)

    OP_FUNCPROLOG, //   iAx - Ax is amount of identifiers on stack to set to vars. 
    OP_RETURN, //       i   - marks a return, climbs chunk hierarchy until returnable chunk is found. 

    //              ==============================[[TABLES && METATABLES]]==============================
    OP_INDEX, //        i   - Gets stack[top] index of GAVEL_TTABLE stack[top-1]
    OP_NEWINDEX, //     i   - Sets stack[top-1] key of stack[top-2] to stack[top]
    OP_CREATETABLE, //  iAx - Creates a new table with stack[top-Ax] values predefined in it

    //              ==================================[[CONDITIONALS]]==================================
    OP_BOOLOP, //       iAx - tests stack[top] with stack[top - 1] then pushes result as a boolean onto the stack. Ax is the type of comparison to be made (BOOLOP)
    OP_TEST, //         iAx - if stack[top] is true, continues execution, otherwise pc is jumped by Ax
    //                  OP_TEST IS A DANGEROUS INSTRUCTION!

    //              ===================================[[ARITHMETIC]]===================================
    OP_ARITH, //        iAB- does arithmatic with stack[top] to stack[top - 1]. A is the type of arithmatic to do (OPARITH), B is an optional argument to the arithmatic operator. Result is pushed onto the stack

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

    // UNUSED
    OPARITH_POW,
    OPARITH_NOT,

    OPARITH_INC, // this will increment an identifier  by stack[top] and set the var all in one instruction.
    OPARITH_DEC, // this will decrement an identifier at stack[top-1] by stack[top] and set the var all in one instruction.
    // [TODO]: add support for MUL and DIV 
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
    GAVEL_TBOOLEAN, // bool
    GAVEL_TDOUBLE, // double
    GAVEL_TSTRING,
    GAVEL_TCHUNK, // information for a gavel chunk.
    GAVEL_TCFUNC, // lets us assign stuff like "print" in the VM to c++ functions
    GAVEL_TTABLE, // basically arrays && maps but it's the same datatype
    GAVEL_TOBJECTION
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
#define CREATECONST_OBJECTION(n)(GValue*)new GValueObjection(n)
#define CREATECONST_TABLE()     (GValue*)new GValueTable()

#define READGAVELVALUE(x, type) reinterpret_cast<type>(x)->val

#define READGVALUEBOOL(x) READGAVELVALUE(x, GValueBoolean*)
#define READGVALUEDOUBLE(x) READGAVELVALUE(x, GValueDouble*)
#define READGVALUESTRING(x) READGAVELVALUE(x, GValueString*)
#define READGVALUECHUNK(x) READGAVELVALUE(x, GValueChunk*)
#define READGVALUECFUNC(x) READGAVELVALUE(x, GValueCFunction*)
#define READGVALUETABLE(x) reinterpret_cast<GValueTable*>(x)
#define READGVALUEOBJECTION(x) READGAVELVALUE(x, GValueObjection*)

// defines a chunk. each chunk has locals
class GChunk {
public:
    std::vector<INSTRUCTION> chunk;
    std::vector<lineInfo> debugInfo; 
    std::vector<GValue*> consts;
    std::vector<std::string> identifiers;
    std::string name;

    bool returnable = false;
    bool scoped = true; // if this is false, when setVar is called, it will set the var in the chunk hierarchy. when false, it'll set it to the state.

    // these are not serialized! they are setup by the compiler/serializer
    std::map<std::string, GValue*> locals; // these are completely ignored
    GChunk* parent = NULL;

    // now with 2873648273275% less memory leaks ...
    GChunk(char* n, std::vector<INSTRUCTION> c, std::vector<lineInfo> d, std::vector<GValue*> cons, std::vector<std::string> i): 
        name(n), chunk(c), debugInfo(d), consts(cons), identifiers(i) {}

    GChunk(std::string n, std::vector<INSTRUCTION> c, std::vector<lineInfo> d, std::vector<GValue*> cons, std::vector<std::string> i): 
        name(n), chunk(c), debugInfo(d), consts(cons), identifiers(i) {}

    GChunk(char* n, std::vector<INSTRUCTION> c, std::vector<lineInfo> d, std::vector<GValue*> cons, std::vector<std::string> i, bool r): 
        name(n), chunk(c), debugInfo(d), consts(cons), identifiers(i), returnable(r) {}

    GChunk(std::string n, std::vector<INSTRUCTION> c, std::vector<lineInfo> d, std::vector<GValue*> cons, std::vector<std::string> i, bool r): 
        name(n), chunk(c), debugInfo(d), consts(cons), identifiers(i), returnable(r) {}

    GChunk(char* n, std::vector<INSTRUCTION> c, std::vector<lineInfo> d, std::vector<GValue*> cons, std::vector<std::string> i, bool r, bool s): 
        name(n), chunk(c), debugInfo(d), consts(cons), identifiers(i), returnable(r), scoped(s) {}


    // we'll have to define these later, because they reference GState && GValue and stuff
    ~GChunk();

    void setLocal(const char* key, GValue* var);
    void clearLocals();
    void restoreLocals(std::map<std::string, GValue*>);
    std::map<std::string, GValue*> saveLocals();
    void setVar(const char* key, GValue* var, GState* state = NULL);
    GValue* getVar(const char* key, GState* state = NULL);
};

/* GavelObjection 
    This class replaces the terrible idependent solutions for both the parser and the VM.
*/
class GavelObjection {
private:
    std::string chunkName;
    std::string msg;
    int line;
    GChunk* chunk;
public:
    GavelObjection() {}

    GavelObjection(std::string m, int l, GChunk* c):
        msg(m), line(l), chunk(c) {
            chunkName = chunk->name;
        }
    
    GavelObjection(std::string m, int l):
        msg(m), line(l) {
            chunk = NULL;
            chunkName = "_parser";
        }

    std::string getRawString() {
        return msg;
    }

    std::string getFormatedString() {
        if (chunk != NULL)
            return "[*] OBJECTION! in [" + chunkName + "] (line " + std::to_string(line) + "):\n\t" + msg;
        else
            return "[*] OBJECTION! (line " + std::to_string(line) + "):\n\t" + msg;
    }

    int getLine() {
        return line;
    }

    GChunk* getChunk() {
        return chunk;
    }
};

/* GValue
    This class is a baseclass for all GValue objects.
*/
class GValue {
public:
    BYTE type; // type info
    bool isReference = false; // this is set for values like GAVEL_TTABLE, which cannot be memory collected until the state is being cleaned.
    GValue() {}
    virtual ~GValue() {};

    virtual bool equals(GValue*) { return false; };
    virtual bool lessthan(GValue*) { return false; };
    virtual bool morethan(GValue*) { return false; };
    virtual std::string toString() { return ""; };
    virtual std::string toStringDataType() { return ""; };
    virtual GValue* clone() {return new GValue(); };
    virtual int getHash() {return std::hash<BYTE>()(type); };
    static void free(GValue* v) {delete v; };

    // so we can easily compare GValues
    bool operator==(GValue& other)
    {
        return this->equals(&other);
    }

    bool operator<(GValue& other) {
        return this->lessthan(&other);
    }

    bool operator>(GValue& other) {
        return this->morethan(&other);
    }

    bool operator<=(GValue& other) {
        return this->lessthan(&other) || equals(&other);
    }

    bool operator>=(GValue& other) {
        return this->morethan(&other) || equals(&other);
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
    virtual ~GValueNull() {};

    bool equals(GValue* other) {
        return other->type == type;
    }

    bool lessthan(GValue* other) {
        return false;
    }

    bool morethan(GValue* other) {
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

class GValueObjection : public GValue {
public:
    GavelObjection val;

    //GValueObjection() {}
    GValueObjection(GavelObjection o): val(o) {
        type = GAVEL_TOBJECTION;
    }

    virtual ~GValueObjection() {};

    bool equals(GValue* other) {
        return false;
    }

    bool lessthan(GValue* other) {
        return false;
    }

    bool morethan(GValue* other) {
        return false;
    }

    std::string toString() {
        return val.getRawString();
    }

    std::string toStringDataType() {
        return "[OBJECTION]";
    }

    GValue* clone() {
        return CREATECONST_OBJECTION(val);
    }
};

class GValueBoolean : public GValue {
public:
    bool val;

    GValueBoolean(bool b): val(b) {
        type = GAVEL_TBOOLEAN;
    }

    virtual ~GValueBoolean() {};

    bool equals(GValue* other) {
        if (other->type == type) {
            return reinterpret_cast<GValueBoolean*>(other)->val == val;
        }
        return false;
    }

    bool lessthan(GValue* other) {
        return false;
    }

    bool morethan(GValue* other) {
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

    int getHash() {
        return std::hash<BYTE>()(type)^std::hash<bool>()(val);
    }
};

class GValueDouble : public GValue {
public:
    double val;

    GValueDouble(double d): val(d) {
        type = GAVEL_TDOUBLE;
    }

    virtual ~GValueDouble() {};

    bool equals(GValue* other) {
        if (other->type == type) {
            return reinterpret_cast<GValueDouble*>(other)->val == val;
        }
        return false;
    }

    bool lessthan(GValue* other) {
        if (other->type == type) {
            return val < reinterpret_cast<GValueDouble*>(other)->val;
        }
        return false;
    }

    bool morethan(GValue* other) {
        if (other->type == type) {
            return val > reinterpret_cast<GValueDouble*>(other)->val;
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

    int getHash() {
        return std::hash<BYTE>()(type)^std::hash<double>()(val);
    }
};

class GValueString : public GValue {
public:
    std::string val;

    GValueString(std::string b): val(b) {
        type = GAVEL_TSTRING;
    }

    virtual ~GValueString() {};

    bool equals(GValue* other) {
        if (other->type == type) {
            return reinterpret_cast<GValueString*>(other)->val.compare(val) == 0;
        }
        return false;
    }

    bool lessthan(GValue* other) {
        return false;
    }

    bool morethan(GValue* other) {
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

    virtual ~GValueChunk() {};

    bool equals(GValue* other) {
        if (other->type == type) {
            return reinterpret_cast<GValueChunk*>(other)->val == val;
        }
        return false;
    }

    bool lessthan(GValue* other) {
        return false;
    }

    bool morethan(GValue* other) {
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

    virtual ~GValueCFunction() {};

    bool equals(GValue* other) {
        if (other->type == type) {
            return reinterpret_cast<GValueCFunction*>(other)->val == val;
        }
        return false;
    }

    bool lessthan(GValue* other) {
        return false;
    }

    bool morethan(GValue* other) {
        return false;
    }

    std::string toString() {
        std::stringstream stream;
        stream << &val;
        return stream.str();
    }

    std::string toStringDataType() {
        return "[C FUNCTION]";
    }

    GValue* clone() {
        return CREATECONST_CFUNC(val);
    }
};

namespace Gavel {
    template <typename T>
    GValue* newGValue(T x);

    GValue* safeClone(GValue*);
    void safeFree(GValue*);
}

/* GValueTable
        This is basically a std::map wrapper for GValue objects.
*/
class GValueTable : public GValue {
private: 
    // getting std::unordered_maps to work was hell. wtf, why won't they just let us use a standard bool cmp function. this hashtable stuff is hell. 

    struct map_key {
        GValue* val;
        map_key(GValue* v): val(v) {}
        
        bool operator == (const map_key &other) const {
            return val->equals(other.val);
        }
    };

    // specialized hash function for unordered_map keys
    struct hash_fn
    {
        std::size_t operator() (map_key v) const
        {
            return v.val->getHash();
        }
    };

    // this is so i don't dump the heap full of useless GValueNULLS
    GValueNull staticNULL;
public:
    std::unordered_map<map_key, GValue*, hash_fn> val;

    // TODO: add basic methods, like .length(), .exists(), etc.
    GValueTable(std::unordered_map<map_key, GValue*, hash_fn> v): val(v) {
        type = GAVEL_TTABLE;
        isReference = true;
    }

    virtual ~GValueTable() {
        for(auto pair : val) {
            delete pair.first.val;
            delete pair.second;
        }
    }

    GValueTable() {
        type = GAVEL_TTABLE;
        isReference = true;
    }

    bool equals(GValue* other) { // todo
        return false;
    }

    bool lessthan(GValue* other) {
        return false;
    }

    bool morethan(GValue* other) {
        return false;
    }

    std::string toString() {
        std::stringstream stream;
        stream << "Table " << &val;
        return stream.str();
    }

    std::string toStringDataType() {
        return "[TABLE]";
    }

    GValue* clone() {
        std::unordered_map<map_key, GValue*, hash_fn> v;

        for (auto pair : val) { // clones table
            v[map_key(pair.first.val->clone())] = pair.second->clone();
        }
        return (GValue*)new GValueTable(v);
    }

    // GValueTable methods

    // there are some types that WILL NOT work as keys. eg. other tables, Chunks, cfunctions, etc.
    static const bool checkValidKey(GValue* k) {
        switch(k->type) {
            case GAVEL_TTABLE:
            case GAVEL_TCFUNC:
            case GAVEL_TCHUNK:
                return false;
            default:
                return true;
        }
    }

    template<typename T, typename T2>
    void newIndex(T k, T2 v) {
        GValue* key = Gavel::newGValue(k);
        GValue* value = Gavel::newGValue(v);

        newIndex(key, value);

        delete key;
        delete value;
    }

    bool newIndex(GValue* key, GValue* value) {
        if (!checkValidKey(key)) 
            return false;

        auto k = val.find(key);
        if (k != val.end()) {
            delete k->first.val;
            delete k->second; // delete value
            val.erase(key); // remove from map
        }

        // set to map
        DEBUGLOG(std::cout << "setting indx " << key->toString() << " to " << value->toString() << std::endl);
        val[map_key(key->clone())] = value->clone();
        return true;
    }

    template<typename T>
    GValue* index(T k) {
        GValue* key = Gavel::newGValue(k);
        GValue* res = index(key);
        delete key;
        return res;
    }

    GValue* index(GValue* key) {
        if (!checkValidKey(key))
            return reinterpret_cast<GValue*>(&staticNULL);

        DEBUGLOG(std::cout << "looking for indx " << key->toString() << std::endl);

        if (val.find(map_key(key)) != val.end()) {
            DEBUGLOG(std::cout << "getting indx " << key->toString() << " which is " << val[key]->toString() << std::endl);
            return val[map_key(key)];
        }

        return reinterpret_cast<GValue*>(&staticNULL);
    }

    int getSize() {
        return val.size();
    }
};

/* GStack
    Stack for GState. I would've just used std::stack, but it annoyingly hides the container from us in it's protected members :/
*/
class GStack {
private:
    struct StackItem {
        bool reference;
        GValue* val;
        StackItem() { reference = true; } // so we don't delete a NULL pointer
        StackItem(GValue* v, bool r): reference(r), val(v) {}
    };

    StackItem* container;
    std::vector<GValue*> garbage;
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
        container = new StackItem[s];
        size = s;
        top = -1;
    }

    ~GStack() {
        clearStack(); // garbage collect all GValues
        delete[] container;
    }

    StackItem pop(int times = 1) {
        if (isEmpty()) {
            return StackItem();
        }

        // push GValue*(s) to garbage, and set to NULL
        for (int i = 0; i < times; i++) {
            StackItem si = container[top--];
            if (!si.reference)
                garbage.push_back(si.val);
        }
        
        return container[(top >= 0) ? top : 0];
    }

    /* flush()
        This will garbage collect arbituary GValues popped off the stack
    */
    void flush() {
        for (GValue* si : garbage) {
            delete si;
        }
        garbage.clear();
    }

    StackItem popAndFlush(int times = 1) {
        StackItem rtn = pop(times);
        flush();
        return rtn;
    }

    void clearStack() { // super expensive!
        pop(top+1);
        flush();
        DEBUGLOG(std::cout << "cleared stack" << std::endl);
    }

    // pushes whatever datatype that was supplied to the stack as a GValue. If datatype is not supported, NULL is pushed onto stack.
    // returns new size of stack
    // if forcePush is true, it will ignore the isFull() result, and force the push onto the stack (DANGEROUS, ONLY USED FOR ERROR STRING)
    template <typename T>
    int push(T x, bool forcePush = false) {
        GValue* v = Gavel::newGValue(x);
        return push(v, forcePush);
    }

    // pushes GValue onto stack.
    // returns new size of stack
    int push(GValue* t, bool forcePush = false) {
        if (isFull() && !forcePush) {
            return -1;
        }

        container[++top] = StackItem(t, false);
        return top; // returns the new stack size
    }

    // this is for pushing variables onto the stack. (they wont be free'd by the stack when they're no longer needed)
    int pushReference(GValue* t) {
        if (isFull())
            return -1;

        container[++top] = StackItem(t, true);
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
        return container[top - offset].val;
    }

    bool setTop(GValue* g, int offset = 0, bool ref = false) {
         if (isEmpty()) {
            // TODO: OBJECTION
            return false;
        }

        if (top - offset < 0) {
            return false;
        }

        if (!container[top - offset].reference)
            garbage.push_back(container[top - offset].val);

        container[top - offset] = StackItem(g, ref);
        return true;
    }

    int getSize() {
        return top+1;
    }

    // DEBUG function to print the stack and label everything !! 
    void printStack() {
        std::cout << "\n=======================[[Stack Dump]]=======================" << std::endl;
        for (int i = 0; i <= top; ++i) {
            std::cout << std::setw(4) << std::to_string(i) + " - " << std::setw(20) << container[i].val->toStringDataType() << std::setw(20) << container[i].val->toString() << std::endl; 
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

    inline bool globalExists(const char* key) {
        return globals.find(key) != globals.end();
    }

    template<typename T>
    GValue* setGlobal(const char* key, T v) {
        GValue* var = Gavel::newGValue(v);
        GValue* res = setGlobal(key, var);
        delete var;
        return res;
    }

    GValue* setGlobal(const char* key, GValue* var) {
        DEBUGLOG(std::cout << "GLOBAL CALLED" << std::endl; std::cout << "setting " << key << " to " << var->toString() << std::endl);
        GValue* cln;
        // if it's in locals, clean it up
        if (globalExists(key)) {
            DEBUGLOG(std::cout << "CLEANING UP " << globals[key]->toString() << std::endl);
            delete globals[key];
        }

        cln = var->clone();
        globals[key] = cln;
        return cln;
    }

    GValue* getGlobal(const char* key) {
        if (globalExists(key))
            return globals[key];
        return CREATECONST_NULL();
    }

    void throwObjection(std::string error) {
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

        GavelObjection err(error, lNum, debugChunk);
        GValueObjection objValue(err);
#ifdef _GAVEL_OUTPUT_OBJ
        // output to console
        std::cout << err.getFormatedString() << std::endl;
#endif
        // we now push the objection onto the stack
        state = GAVELSTATE_PANIC;
        stack.push(reinterpret_cast<GValue*>(&objValue)->clone(), true); // pushed error to stack
    }

    GavelObjection getObjection() {
        GValue* top = getTop();
        if (state == GAVELSTATE_PANIC && top->type == GAVEL_TOBJECTION) {
            // objection is on the stack
            return READGVALUEOBJECTION(top);
        }
        DEBUGLOG(std::cout << "no objection found!" << std::endl);
        return GavelObjection();
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

    /* callFunction(chunk, Args...)
        This is a wrapper function that will setup the stack and call a function and give you it's return value (THAT YOU ARE RESPONSIBLE FOR CLEANING UP!)
    
   template<typename... Args>
   GValue* callFunction(GChunk* ch, Args&&... args) {
       // pushes values to the stack :eyes: (thank GOD for c++ fold expressions)
       (stack.push(Gavel::newGValue(args)), ...);

   }*/
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

void GChunk::clearLocals() {
    locals.clear();
}

void GChunk::restoreLocals(std::map<std::string, GValue*> newLocals) {
    for (auto pair : locals) {
        delete pair.second;
    }
    locals = newLocals;
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
    if ((!scoped && state != NULL) || (state != NULL && state->globalExists(key))) {
        state->setGlobal(key, var);
    } else { // var doesn't exist yet, so make it.
        setLocal(key, var);
    }

    return;
}

GValue* GChunk::getVar(const char* key, GState* state) {
    if (locals.find(key) != locals.end()) { // check if var is in our locals
        return locals[key];
    }

    if (parent != NULL) { // parents???
        return parent->getVar(key, state);
    }

    // no???
    if (state != NULL && state->globalExists(key)) {
        return state->globals[key];
    }

    return CREATECONST_NULL();
}

std::map<std::string, GValue*> GChunk::saveLocals() {
    /*std::map<std::string, GValue*> newlocals;
    for (auto pair : locals) {
        newlocals[pair.first] = pair.second->clone();
    }
    return newlocals;*/
    return locals;
}

/* VM Macros
    These help make the interpreter more modular. 
*/
#define ARITH_ADD(a,b) a + b
#define ARITH_SUB(a,b) a - b
#define ARITH_MUL(a,b) a * b
#define ARITH_DIV(a,b) a / b
#define ARITH_POW(a,b) pow(a, b)

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
    state->stack.flush();

// Main interpreter
namespace Gavel {
    /* newGValue(<t> value) - Helpfull function to auto-turn some basic datatypes into a GValue for ease of embeddability
        - value : value to turn into a GValue
        returns : GValue
    */
    template <typename T>
    inline GValue* newGValue(T x) {
        if constexpr (std::is_same<T, GAVELCFUNC>())
            return CREATECONST_CFUNC(x);
        else if constexpr(std::is_same<T, GChunk*>())
            return CREATECONST_CHUNK(x);
        else if constexpr (std::is_same<T, double>() || std::is_same<T, int>())
            return CREATECONST_DOUBLE(x);
        else if constexpr (std::is_same<T, bool>())
            return CREATECONST_BOOL(x);
        else if constexpr (std::is_same<T, char*>() || std::is_same<T, const char*>() || std::is_same<T, std::string>())
            return CREATECONST_STRING(x);
        else if constexpr (std::is_same<T, GAVELCFUNC>())
            return CREATECONST_CFUNC(x);
        else if constexpr (std::is_same<T, GValue*>())
            return x->clone();

        return CREATECONST_NULL();
    }

    /* safeClone(GValue*)

    */
   GValue* safeClone(GValue* v) {
        if (!v->isReference)
            return v->clone();
        return v;
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
                    //printf("%f", READGVALUEDOUBLE(_t)); // faster than using std::cout??
                    //break;
                default:
                    printf("%s", _t->toString().c_str());
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
        state->setGlobal("print", lib_print);
        state->setGlobal("type", lib_getType);
        state->setGlobal("stackdump", lib_stackdump);
        state->setGlobal("__VERSION", getVersionString());
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
                    int ret = state->stack.pushReference(chunk->consts[GETARG_Ax(inst)]);
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
                case OP_GETVAR: { // iAx -- pushes vars[Ax] to the stack
                    int a = GETARG_Ax(inst);
                    state->stack.pushReference(chunk->getVar(chunk->identifiers[a].c_str(), state));
                    break;
                }
                case OP_SETVAR: { // iAx -- sets vars[Ax] to stack[top]
                    int a = GETARG_Ax(inst);
                    GValue* top = state->getTop();
                    chunk->setVar(chunk->identifiers[a].c_str(), top, state);
                    state->stack.popAndFlush();
                    break;
                }
                case OP_INDEX: { // i -- indexes a GAVEL_TTABLE, leaves value on stack
                    GValue* top = state->getTop(1); // table
                    GValue* indx = state->getTop();
                    if (top->type != GAVEL_TTABLE) {
                        state->throwObjection("Attempt to index non-table GValue.");
                        break;
                    }
                    state->stack.pop(2);

                    if (GValueTable::checkValidKey(indx)) {
                        GValueTable* table = READGVALUETABLE(top);
                        state->stack.push(safeClone(table->index(indx)));
                    } else {
                        state->throwObjection("Can't index a table with a value type of " + indx->toStringDataType());
                        return;
                    }
                    state->stack.flush();
                    break;
                }
                case OP_NEWINDEX: { // i -- sets index of table to new value. leaves table on stack
                    GValue* tbl = state->getTop(2);
                    GValue* indx = state->getTop(1);
                    GValue* newVal = state->getTop();

                    if (tbl->type != GAVEL_TTABLE) {
                        state->throwObjection("Attemp to index non-table GValue.");
                        break;
                    }

                    GValueTable* table = READGVALUETABLE(tbl);
                    
                    if (!table->newIndex(indx, newVal)) {
                        state->throwObjection("Can't index a table with a value type of " + indx->toStringDataType());
                        return;
                    }                

                    state->stack.popAndFlush(3); // pops index, newVal, and table
                    break;
                }
                case OP_CREATETABLE: { // iAx
                    int size = GETARG_Ax(inst);
                    if (state->stack.getSize() < size) {
                        state->throwObjection("Stack size does not match expected results! : " + std::to_string(size));
                        break;
                    }

                    DEBUGLOG(std::cout << "Building table with size " << size << std::endl);
                    // setup tables with default indexes, (0 thru size-1) take that lua!
                    GValueTable* tbl = new GValueTable();
                    GValue* indx = CREATECONST_DOUBLE(0);
                    for (int i = 0; i < size; i++) {
                        READGVALUEDOUBLE(indx) = (size-1) - i; // creates index
                        tbl->newIndex(indx, state->getTop(i));
                    }

                    DEBUGLOG(std::cout << "Cleaning stack, and pushing table" << std::endl);

                    delete indx; // we don't need this anymore (GValueTable clones the keys && values!)
                    if (size >= 1)
                        state->stack.popAndFlush(size); // clean the stack!
                    state->stack.push((GValue*)tbl); // push our newly crafted table onto the stack!
                    DEBUGLOG(state->stack.printStack());
                    break;
                }
                case OP_BOOLOP: { // i checks stack[top] to stack[top - 1] with BOOLOP[Ax]
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
                    state->stack.flush();
                    state->stack.push(t); // this will use our already defined == operator in the GValue struct
                    break;
                }
                case OP_TEST: { // i
                    int offset = GETARG_Ax(inst);
                    GValue* top = state->getTop();
                    if (!top->type == GAVEL_TBOOLEAN || !READGVALUEBOOL(top)) {
                       // if false, skip next 2 instructions
                       DEBUGLOG(std::cout << "false! jumping by " << offset << std::endl);
                       state->pc += offset;
                    }
                    state->stack.popAndFlush(1); // pop bool value
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

                    std::string ident; // expected to be a string. yes this is me being lazy and will probably cause 187326487126438 bugs later. oops
                    GValue* var;
                    for (int i = 0; i < expectedArgs; i++) { 
                        ident = chunk->identifiers[i]; // gets the identifier first
                        var = state->getTop(i); // then the variable
                    
                        DEBUGLOG(std::cout << "setting " << var->toString() << " to var:" << ident << std::endl); 
                        chunk->setLocal((char*)ident.c_str(), var); // setting a copy of the const to the var
                    }

                    state->stack.pop((expectedArgs) + 1); // should pop everything :)
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
                            std::map<std::string, GValue*> savedLocals = chunk->saveLocals();
                            chunk->clearLocals();
                            executeChunk(state, READGVALUECHUNK(top), totalArgs); // chunks are in charge of popping stuff (because they will also return a value)
                            state->pc = savedPc;
                            state->debugChunk = chunk;
                            chunk->restoreLocals(savedLocals);
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
                    OPARITH aop = (OPARITH)GETARG_A(inst); // gets the type of arithmetic to do
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
                        case OPARITH_INC: {
                            int id = GETARG_B(inst);
                            GValue* amount = state->getTop(); // amount to increment by
                            GValue* value = chunk->getVar((char*)chunk->identifiers[id].c_str(), state);
                             if (value->type == GAVEL_TDOUBLE && amount->type == GAVEL_TDOUBLE) {
                                READGVALUEDOUBLE(value) = READGVALUEDOUBLE(value) + READGVALUEDOUBLE(amount); // it's no longer a copy, just set the value directly
                                state->stack.pop(); // pops the amount
                                state->stack.pushReference(value); // pushes reference to the stack
                            } else {
                                state->throwObjection("[DOUBLE] expected, got " + value->toString() + " instead");
                            }
                            break;
                        }
                        default:
                            state->throwObjection("OPCODE failure!");
                            break;
                    }
                    break;
                }
                case OP_RETURN: {
                    GValue* top = state->getTop()->clone();
                    state->stack.pop(passedArguments); // pops chunk, and return value
                    if (!chunk->returnable) { // pop args again
                        state->stack.pop();
                    }
                    state->stack.flush();
                    state->stack.push(top); // pushes our return value!

                    DEBUGLOG(std::cout << "returning !" << std::endl);
                    
                    state->state = GAVELSTATE_RETURNING;
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
    TOKEN_STARTCONDITIONAL,
    TOKEN_ENDCONDITIONAL,
    TOKEN_OPENINDEX,
    TOKEN_ENDINDEX,
    TOKEN_OPENTABLE,
    TOKEN_CLOSETABLE,
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
#define CREATELEXERTOKEN_STARTCONDITIONAL()       GavelToken(TOKEN_STARTCONDITIONAL)
#define CREATELEXERTOKEN_ENDCONDITIONAL()       GavelToken(TOKEN_ENDCONDITIONAL)
#define CREATELEXERTOKEN_OPENINDEX()    GavelToken(TOKEN_OPENINDEX)
#define CREATELEXERTOKEN_ENDINDEX()     GavelToken(TOKEN_ENDINDEX)
#define CREATELEXERTOKEN_OPENTABLE()    GavelToken(TOKEN_OPENTABLE)
#define CREATELEXERTOKEN_CLOSETABLE()   GavelToken(TOKEN_CLOSETABLE)
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
    std::vector<std::string> idents;

    GavelObjection err;
    std::string name;

    int args = 0;
    int* currentLine;
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
        return addConstant(Gavel::newGValue(c));
    }

    int addIdentifier(std::string id) {
        for (int i = 0; i < idents.size(); i++)
            if (idents[i].compare(id) == 0)
                return i; // identifier is already in our list! return it!
        
        idents.push_back(id);

        DEBUGLOG(std::cout << "adding identifier[" << (idents.size() - 1) << "] : " << id << std::endl);
        return idents.size() - 1;
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
        if (i >= tokenList->size()-1 || i < 0 || tokenList->size() == 0) {
            DEBUGLOG(std::cout << "end of token list! : " << i << std::endl);
            tokenList->push_back(new CREATELEXERTOKEN_EOF());
            return (*tokenList)[tokenList->size() - 1];
        }
        return (*tokenList)[i];
    }

    GavelObjection getObjection() {
        return err;
    }

#ifdef _GAVEL_OUTPUT_OBJ 
    #define GAVELPARSEROBJECTION(msg) \
        if (!objectionOccurred) { \
            std::stringstream eStream; \
            eStream << msg; \
            err = GavelObjection(eStream.str(), (*currentLine)+1); \
            objectionOccurred = true; \
            std::cout << err.getFormatedString() << std::endl; \
        }
#else
    #define GAVELPARSEROBJECTION(msg) \
        if (!objectionOccurred) { \
            std::stringstream eStream; \
            eStream << msg; \
            err = GavelObjection(eStream.str(), (*currentLine)+1); \
            objectionOccurred = true; \
        }
#endif 

    bool checkEOL(GavelToken* token) {
        return token->type == TOKEN_ENDOFFILE || token->type == TOKEN_ENDOFLINE;
    }

    // checks the end of a scope to make sure previous line was ended correctly
    bool checkEOS(int* indx, bool ignore = false) {
        switch(peekNextToken(*indx)->type) {
            case TOKEN_ENDOFLINE:
            case TOKEN_ENDOFFILE:
            case TOKEN_ENDSCOPE:
                return true;
            default:
                if (!ignore) {
                    GAVELPARSEROBJECTION("Illegal syntax! Statement not ended correctly!");
                }
                return false;
        }
    }

    void writeDebugInfo(int i) {
        if (tokenLineInfo->size() < *currentLine || tokenLineInfo->size() == 0)
            return;
        int markedToken = 0;
        do {
            if (*currentLine > tokenLineInfo->size() - 1)
                break;
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
        // parse scope
        do {
            parseLine(indx);
            checkEOS(indx);
        } while(!objectionOccurred && peekNextToken(*indx)->type != TOKEN_ENDSCOPE && peekNextToken((*indx)++)->type != TOKEN_ENDOFFILE);
    }

    // parse mini-scopes, like the right of =, or anything in ()
    GavelToken* parseContext(int* indx, bool arith = false) { // if it's doing arith, some tokens are forbidden
        do {
            GavelToken* token = peekNextToken(*indx);
            DEBUGLOG(std::cout << "CONTEXT TOKEN: " << token->type << std::endl);
            switch(token->type) {
                case TOKEN_CONSTANT: {
                    if (peekNextToken((*indx)-1)->type == TOKEN_CONSTANT || peekNextToken((*indx)-1)->type == TOKEN_VAR) {
                        GAVELPARSEROBJECTION("Illegal syntax!");
                        return peekNextToken(*indx);
                    }
                    int constIndx = addConstant(dynamic_cast<GavelToken_Constant*>(token)->cons);
                    // pushes const onto stack
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, constIndx));
                    break;
                }
                case TOKEN_VAR: {
                    if (peekNextToken((*indx)-1)->type == TOKEN_CONSTANT || peekNextToken((*indx)-1)->type == TOKEN_VAR) {
                        GAVELPARSEROBJECTION("Illegal syntax!");
                        return peekNextToken(*indx);
                    } 

                    int idIndx = addIdentifier((char*)dynamic_cast<GavelToken_Variable*>(token)->text.c_str());

                    GavelToken* nxt = peekNextToken((*indx)+1);
                    if (nxt->type == TOKEN_ARITH && (dynamic_cast<GavelToken_Arith*>(nxt)->op == OPARITH_INC || dynamic_cast<GavelToken_Arith*>(nxt)->op == OPARITH_DEC)) {
                        // if it's a quick increment/decrement (there's probably a better way to do it)
                        (*indx)++;
                        int inc = addConstant(CREATECONST_DOUBLE((dynamic_cast<GavelToken_Arith*>(nxt)->op == OPARITH_INC) ? 1 : -1)); 
                        insts.push_back(CREATE_iAx(OP_PUSHVALUE, inc)); // pushes inc amount onto stack
                        insts.push_back(CREATE_iAB(OP_ARITH, OPARITH_INC, idIndx)); // do inc/dec
                    } else {
                        insts.push_back(CREATE_iAx(OP_GETVAR, idIndx)); // var onto stack
                    }
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
                    insts.push_back(CREATE_iAB(OP_ARITH, op, 0));

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
                case TOKEN_OPENTABLE: {
                    (*indx)++;
                    GavelToken* nxt;
                    int tblSize = 0; // keeps track of the arguments :)!
                    if (peekNextToken(*indx)->type != TOKEN_CLOSETABLE) {
                        while(true) {
                            nxt = parseContext(indx);
                            tblSize++;
                            // only 2 tokens are allowed, ENDCALL & SEPARATOR
                            if (nxt->type == TOKEN_CLOSETABLE) {
                                break; // stop parsing
                            } else if (nxt->type != TOKEN_SEPARATOR) {
                                GAVELPARSEROBJECTION("Illegal syntax! \"" << GAVELSYNTAX_SEPARATOR << "\" or \"" << GAVELSYNTAX_ENDCALL << "\" expected!");
                                return peekNextToken(*indx);
                            }
                            (*indx)++;
                        }
                    }
                    insts.push_back(CREATE_iAx(OP_CREATETABLE, tblSize));
                    break;
                }
                case TOKEN_OPENINDEX: {
                    (*indx)++;
                    if (parseContext(indx)->type != TOKEN_ENDINDEX) {
                        GAVELPARSEROBJECTION("Illegal syntax! Expected \"" << GAVELSYNTAX_ENDINDEX << "\"!")
                    }

                    insts.push_back(CREATE_i(OP_INDEX));
                    break;
                }
                case TOKEN_OPENCALL: { // check if we are calling a var, or if it's just for context.
                    if (peekNextToken((*indx) - 1)->type == TOKEN_VAR) { // check last token for var. if it's a var this is a call.
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
                                GAVELPARSEROBJECTION("Illegal syntax! \"" << GAVELSYNTAX_SEPARATOR << "\" or \"" << GAVELSYNTAX_ENDCALL << "\" expected!");
                                return NULL;
                            }
                            numArgs++;
                            (*indx)++;
                        }
                        insts.push_back(CREATE_iAx(OP_CALL, numArgs+1));
                    } else { // okay so they just to group stuff together.
                        (*indx)++;
                        GavelToken* nxt = parseContext(indx);
                        while (!(nxt->type == TOKEN_ENDCALL)) {
                            if (checkEOL(nxt) || nxt->type == TOKEN_ENDSCOPE) {
                                GAVELPARSEROBJECTION("Expected \"" << GAVELSYNTAX_ENDCALL << "\" before end of statement");
                            }
                        }
                    }
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

        return peekNextToken(*indx);
    }

    // parse whole lines, like everything until a ;
    void parseLine(int* indx, bool ifcase = false) {
        do {
            GavelToken* token = peekNextToken(*indx);
            DEBUGLOG(std::cout << "token[" << *indx << "] : " << token->type << std::endl);
            switch (token->type) {
                case TOKEN_VAR: {
                    GavelToken* peekNext = peekNextToken((*indx) + 1);
                    DEBUGLOG(std::cout << "peek'd token type : " << peekNext->type << std::endl);
                    int idIndx = addIdentifier((char*)dynamic_cast<GavelToken_Variable*>(token)->text.c_str());

                    if (peekNext->type == TOKEN_ASSIGNMENT) {
                        (*indx)+=2;
                        GavelToken* nxt;
                        if (!checkEOL(parseContext(indx)))
                        {
                            GAVELPARSEROBJECTION("Illegal syntax!");
                            return;
                        }
                        insts.push_back(CREATE_iAx(OP_SETVAR, idIndx));
                        return;
                    } else {
                        GavelToken* nxt = peekNextToken((*indx)+1);
                        if (nxt->type == TOKEN_ARITH && (dynamic_cast<GavelToken_Arith*>(nxt)->op == OPARITH_INC || dynamic_cast<GavelToken_Arith*>(nxt)->op == OPARITH_DEC)) {
                            // if it's a quick increment/decrement (there's probably a better way to do it)
                            (*indx)++;
                            int inc = addConstant(CREATECONST_DOUBLE((dynamic_cast<GavelToken_Arith*>(nxt)->op == OPARITH_INC) ? 1 : -1)); 
                            insts.push_back(CREATE_iAx(OP_PUSHVALUE, inc)); // pushes inc amount onto stack
                            insts.push_back(CREATE_iAB(OP_ARITH, OPARITH_INC, idIndx)); // do inc/dec
                            // prevent stack leak :)
                            insts.push_back(CREATE_iAx(OP_POP, 1));
                        } else {
                            insts.push_back(CREATE_iAx(OP_GETVAR, idIndx)); // pushes var onto stack
                        }
                        break;
                    }
                }
                case TOKEN_OPENINDEX: {
                    (*indx)++;

                    // if there's not an index, wtf is going on??
                    if (peekNextToken(*indx)->type == TOKEN_ENDINDEX) {
                        GAVELPARSEROBJECTION("Illegal syntax! Expected index!");
                        return;
                    }

                    if (parseContext(indx)->type != TOKEN_ENDINDEX) { // gets index onto stack
                        GAVELPARSEROBJECTION("Illegal syntax! Expected \"" << GAVELSYNTAX_ENDINDEX << "\"!");
                        return;
                    }

                    if (peekNextToken((*indx) + 1)->type == TOKEN_ASSIGNMENT) {
                        (*indx)+=2;
                        GavelToken* nxt;
                        if (!checkEOL(parseContext(indx))) // last stuff on stack.
                        {
                            GAVELPARSEROBJECTION("Illegal syntax!");
                            return;
                        }
                        insts.push_back(CREATE_i(OP_NEWINDEX)); // top-2 = [TABLE], top-1 = INDX, top = newval
                        return;
                    } else {
                        insts.push_back(CREATE_i(OP_INDEX));
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
                case TOKEN_STARTCONDITIONAL: {
                    DEBUGLOG(std::cout << " if case " << std::endl);
                    if (peekNextToken((*indx) + 1)->type == TOKEN_ENDCONDITIONAL) {
                        GAVELPARSEROBJECTION("Illegal syntax! No conditional!");
                        return;
                    }

                    GavelToken* nxt;
                    do {
                        (*indx)++;
                        nxt = parseContext(indx);

                        if (nxt->type == TOKEN_ENDCONDITIONAL) {
                            break;
                        } else if (nxt->type != TOKEN_BOOLOP) {
                            GAVELPARSEROBJECTION("Illegal syntax! \"" "then" "\" expected!");
                            return;
                        }
                    } while(true);

                    // parse scope
                    int savedInstrIndx = insts.size();
                    insts.push_back(0); // filler! this will be replaced with the correct offset after the next line is parsed.
                    (*indx)++;

                    do {
                        parseLine(indx, true);
                        if (!checkEOS(indx, true) && peekNextToken(*indx)->type != TOKEN_ELSECASE)
                        {
                            GAVELPARSEROBJECTION("Illegal syntax! Statement not ended correctly!")
                        }
                    } while(!objectionOccurred && peekNextToken(*indx)->type != TOKEN_ENDSCOPE  && peekNextToken(*indx)->type != TOKEN_ELSECASE && peekNextToken((*indx)++)->type != TOKEN_ENDOFFILE);

                    if (objectionOccurred)
                        return;

                    if (peekNextToken(*indx)->type == TOKEN_ELSECASE) { // has an else, handle it
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

                    break; // lets us contine
                }
                case TOKEN_FUNCTION: { // defining a function
                    DEBUGLOG(std::cout << "function" << std::endl);
                    GavelToken* nxt = peekNextToken(++(*indx));

                    if (nxt->type != TOKEN_VAR) {
                        GAVELPARSEROBJECTION("Illegal syntax! Identifier expected before \"(\"!");
                        return;
                    }

                    char* functionName = (char*)dynamic_cast<GavelToken_Variable*>(nxt)->text.c_str();

                    int idIndx = addIdentifier(functionName);
                    GavelScopeParser functionChunk(tokenList, tokenLineInfo, currentLine, (char*)dynamic_cast<GavelToken_Variable*>(nxt)->text.c_str());

                    nxt = peekNextToken(++(*indx));
                    if (nxt->type != TOKEN_OPENCALL) {
                        GAVELPARSEROBJECTION("Illegal syntax! \"" "(" "\" expected after identifier!");
                        return;
                    }

                    // get parameters it uses
                    // OP_FUNCPROLOG expects the identifiers to be assigned to the passed values to be the first identifiers. set those.
                    int params = 0;
                    while(nxt = peekNextToken(++(*indx))) {
                        if (nxt->type == TOKEN_VAR) {
                            params++; // increment params lol
                            functionChunk.addIdentifier((char*)dynamic_cast<GavelToken_Variable*>(nxt)->text.c_str());
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

                    (*indx)++;
                    GChunk* scope = functionChunk.parseFunctionScope(indx);
                    if (scope == NULL) {
                        err = functionChunk.getObjection();
                        objectionOccurred = true;
                        return;
                    }
                    
                    scope->name = functionName;

                    childChunks.push_back(scope);
                    int chunkIndx = addConstant(scope);
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));
                    insts.push_back(CREATE_iAx(OP_SETVAR, idIndx));
                    
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
                    if (peekNextToken((*indx) + 1)->type == TOKEN_ENDCONDITIONAL) {
                        GAVELPARSEROBJECTION("Illegal syntax! No conditional!");
                        return;
                    }
                    
                    int startPc = insts.size();

                    GavelToken* nxt;
                    do {
                        (*indx)++;
                        nxt = parseContext(indx);

                        if (nxt->type == TOKEN_OPENSCOPE) {
                            break;
                        } else if (nxt->type != TOKEN_BOOLOP) {
                            GAVELPARSEROBJECTION("Illegal syntax! \"" GAVELSYNTAX_OPENSCOPE "\" expected!");
                            return;
                        }
                    } while(true);

                    int testPc = insts.size();
                    insts.push_back(CREATE_iAx(OP_TEST, 0)); // filler until we get full instructions
                    // parse scope
                    (*indx)++; // skips TOKEN_OPENSCOPE
                    parseScope(indx);

                    insts.push_back(CREATE_iAx(OP_JMPBACK, (insts.size() - startPc) + 1)); // 3rd instruction to skip
                    insts[testPc] = CREATE_iAx(OP_TEST, (insts.size() - testPc) - 1);
                   
                    DEBUGLOG(std::cout << "loop end! continuing.." << std::endl);
                    break;
                }
                case TOKEN_OPENSCOPE: {
                    (*indx)++;
                    GavelScopeParser scopeParser(tokenList, tokenLineInfo, currentLine);
                    GChunk* scope = scopeParser.parseScopeChunk(indx);
                    if (scope == NULL) {
                        err = scopeParser.getObjection();
                        objectionOccurred = true;
                        return;
                    }
                    childChunks.push_back(scope);
                    int chunkIndx = addConstant(scope);
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));

                    return;
                }
                case TOKEN_ELSECASE:
                    if (!ifcase)
                        GAVELPARSEROBJECTION("Illegal syntax!");
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
            return new GChunk(name, insts, debugInfo, consts, idents);
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
            c = new GChunk(name, insts, debugInfo, consts, idents);

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

    GavelObjection objection;

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

    std::string readString() {
        std::string str;

        while (*currentChar++) {
            if (*currentChar == GAVELSYNTAX_STRING)
            {
                break;
            }
            str += *currentChar;
        }

        DEBUGLOG(std::cout << " string : " << str << std::endl);

        return str;
    }

    std::string readIdentifier() {
        std::string name;

        // strings can be alpha-numeric + _
        while ((isalpha(*currentChar) || isNumeric(*currentChar) || *currentChar == '_') && *currentChar != '.') {
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

    void buildTokenList() {        
        while (currentChar < code+len) {
            switch (*currentChar) {
                case GAVELSYNTAX_COMMENTSTART:
                    if (peekNext() == GAVELSYNTAX_COMMENTSTART) {
                        *currentChar++;
                        // it's a comment, so skip to next line
                        int length = nextLine();
                        DEBUGLOG(std::cout << " NEWLINE " << std::endl);
                        tokenLineInfo.push_back(tokenList.size()); // push number of tokens to mark end of line!
                    } else { // it's just / by itself, so it's trying to divide!!!!!
                        tokenList.push_back(new CREATELEXERTOKEN_ARITH(OPARITH_DIV));
                    }
                    break;
                case GAVELSYNTAX_ASSIGNMENT: {
                    if (peekNext() == '=') {
                        // equals bool op
                        DEBUGLOG(std::cout << " == " << std::endl);
                        *currentChar++;
                        tokenList.push_back(new CREATELEXERTOKEN_BOOLOP(BOOLOP_EQUALS));
                        break;
                    }
                    DEBUGLOG(std::cout << " = " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_ASSIGNMENT());
                    break;
                }
                case GAVELSYNTAX_STRING: {
                    // read string, create const, and return token
                    tokenList.push_back(new CREATELEXERTOKEN_CONSTANT(CREATECONST_STRING(readString())));
                    break;
                }
                case GAVELSYNTAX_OPENCALL: {
                    DEBUGLOG(std::cout << " ( " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_OPENCALL());
                    break;
                }
                case GAVELSYNTAX_ENDCALL: {
                    DEBUGLOG(std::cout << " ) " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_ENDCALL());
                    break;
                }
                case GAVELSYNTAX_OPENINDEX: {
                    DEBUGLOG(std::cout << " [ " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_OPENINDEX());
                    break;
                }
                case GAVELSYNTAX_ENDINDEX: {
                    DEBUGLOG(std::cout << " ] " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_ENDINDEX());
                    break;
                }
                case GAVELSYNTAX_MINIINDEX: {
                    DEBUGLOG(std::cout << " . " << std::endl);
                    *currentChar++;
                    tokenList.push_back(new CREATELEXERTOKEN_OPENINDEX());
                    std::string ident = readIdentifier();
                    if (ident.length() > 0) { // if the length is 0 (nothing was given), this will make the parser very mad later lol
                        DEBUGLOG(std::cout << " " << ident << std::endl);
                        tokenList.push_back(new CREATELEXERTOKEN_CONSTANT(CREATECONST_STRING(ident)));
                    }
                    tokenList.push_back(new CREATELEXERTOKEN_ENDINDEX());
                    break;
                }
                case GAVELSYNTAX_OPENTABLE: {
                    DEBUGLOG(std::cout << " { " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_OPENTABLE());
                    break;
                }
                case GAVELSYNTAX_CLOSETABLE: {
                    DEBUGLOG(std::cout << " } " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_CLOSETABLE());
                    break;
                }
                case GAVELSYNTAX_SEPARATOR: {
                    DEBUGLOG(std::cout << " , " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_SEPARATOR());
                    break;
                }
                case GAVELSYNTAX_ENDOFLINE: {
                    DEBUGLOG(std::cout << " EOL\n " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_EOL());
                    break; 
                }
                case GAVELSYNTAX_BOOLOPLESS: {
                    if (peekNext() == '=') {
                        *currentChar++;
                        tokenList.push_back(new CREATELEXERTOKEN_BOOLOP(BOOLOP_LESSEQUALS));
                    } else {
                        tokenList.push_back(new CREATELEXERTOKEN_BOOLOP(BOOLOP_LESS));
                    }
                    break;
                }
                case GAVELSYNTAX_BOOLOPMORE: {
                    if (peekNext() == '=') {
                        *currentChar++;
                        tokenList.push_back(new CREATELEXERTOKEN_BOOLOP(BOOLOP_MOREEQUALS));
                    } else {
                        tokenList.push_back(new CREATELEXERTOKEN_BOOLOP(BOOLOP_MORE));
                    }
                    break;
                }
                case '\0': {
                    DEBUGLOG(std::cout << " EOF " << std::endl);
                    tokenList.push_back(new CREATELEXERTOKEN_EOF());
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
                        tokenList.push_back(new CREATELEXERTOKEN_CONSTANT(readNumber()));
                    } else if (isalpha(*currentChar) || *currentChar == '_') { // identifier
                        //======= [[ reserved words lol ]] =======
                        std::string ident = readIdentifier();

                        if (ident == GAVELSYNTAX_STARTCONDITIONAL) {
                            DEBUGLOG(std::cout << " IF " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_STARTCONDITIONAL());
                        } else if (ident == GAVELSYNTAX_ENDCONDITIONAL) {
                            DEBUGLOG(std::cout << " THEN " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_ENDCONDITIONAL());
                        } else if (ident == GAVELSYNTAX_ELSECASE) {
                            DEBUGLOG(std::cout << " ELSE " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_ELSECASE());
                        } else if (ident == GAVELSYNTAX_BOOLFALSE) {
                            DEBUGLOG(std::cout << " FALSE " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_CONSTANT(CREATECONST_BOOL(false)));
                        } else if (ident == GAVELSYNTAX_BOOLTRUE) {
                            DEBUGLOG(std::cout << " TRUE " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_CONSTANT(CREATECONST_BOOL(true)));
                        } else if (ident == GAVELSYNTAX_FUNCTION) {
                            DEBUGLOG(std::cout << " FUNCTION " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_FUNCTION());
                        } else if (ident == GAVELSYNTAX_RETURN) {
                            DEBUGLOG(std::cout << " RETURN " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_RETURN());
                        } else if (ident == GAVELSYNTAX_WHILE) {
                            DEBUGLOG(std::cout << " WHILE " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_WHILE());
                        } else if (ident == GAVELSYNTAX_OPENSCOPE) {
                            DEBUGLOG(std::cout << " { " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_OPENSCOPE());
                        } else if (ident == GAVELSYNTAX_ENDSCOPE) {
                            DEBUGLOG(std::cout << " } " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_ENDSCOPE());
                        } else { // it's a variable
                            DEBUGLOG(std::cout << " var: " << ident << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_VAR(ident));
                        }
                    } else { 
                        OPARITH op = isOp(*currentChar);
                        GavelToken* prev = previousToken();

                        // handles negative op
                        if (op == OPARITH_SUB && prev != NULL && (prev->type != TOKEN_CONSTANT && prev->type != TOKEN_VAR)) {
                            *currentChar++;
                            if (isNumeric(*currentChar)) {
                                DEBUGLOG(std::cout << " NEGATIVE CONSTANT " << std::endl);
                                tokenList.push_back(new CREATELEXERTOKEN_CONSTANT(readNumber(-1)));
                            } else if(isalpha(*currentChar)) { // turns -var into -1 * var (kinda hacky but it works, rip performance though.)
                                DEBUGLOG(std::cout << " NEGATIVE VAR " << std::endl);
                                tokenList.push_back(new CREATELEXERTOKEN_VAR(readIdentifier()));
                                tokenList.push_back(new CREATELEXERTOKEN_ARITH(OPARITH_MUL));
                                tokenList.push_back(new CREATELEXERTOKEN_CONSTANT(CREATECONST_DOUBLE(-1)));
                            }
                        } else if (op == OPARITH_ADD && isOp(*(currentChar + 1)) == OPARITH_ADD) {
                            DEBUGLOG(std::cout << " OPERATOR - INC " << std::endl);
                            // increment operator
                            tokenList.push_back(new CREATELEXERTOKEN_ARITH(OPARITH_INC));
                            *currentChar++;
                        } else if (op == OPARITH_SUB && isOp(*(currentChar + 1)) == OPARITH_SUB) {
                            DEBUGLOG(std::cout << " OPERATOR - DEC " << std::endl);
                            // increment operator
                            tokenList.push_back(new CREATELEXERTOKEN_ARITH(OPARITH_DEC));
                            *currentChar++;
                        } else if (op != OPARITH_NONE) {
                            DEBUGLOG(std::cout << " OPERATOR " << std::endl);
                            tokenList.push_back(new CREATELEXERTOKEN_ARITH(op));
                        }
                    }
                    break;
            }
            *currentChar++;
        }
    }

    void freeTokenList() {
        for (GavelToken* token : tokenList) {
            delete token; // free the our tokens
        }
        tokenList = {};
    }

    GavelObjection getObjection() {
        return objection;
    }

    GChunk* compile() {
        DEBUGLOG(std::cout << "[*] COMPILING SCRIPT.." << std::endl);
        GChunk* mainChunk = NULL;

        // generate tokens
        buildTokenList();

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
        [4*i bytes] - instruction list
        [4 bytes] - size of debug info
        [n bytes] - debug info list
            [4 bytes] - endInstr
            [4 bytes] - line num
*/

// serial bytecode release version

#define GAVEL_VERSION_BYTE '\x03'

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

    void writeRawString(char* str) {
        int strSize = strlen(str);
        writeSizeT(strSize); // writes size of string
        data.write(reinterpret_cast<const char*>(str), strSize); // writes string to stream!
    }

    void writeString(char* str) {
        writeByte(GAVEL_TSTRING);
        writeRawString(str);
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

    void writeIdentifiers(std::vector<std::string> ids) { // TODO: this is needlessly kinda fat. Maybe compress or ad option to remove debug info?
        writeSizeT(ids.size()); // write size of data!
        for (std::string id : ids) {
            writeRawString((char*)id.c_str());    
        }
    }

    void writeChunk(GChunk* chunk) {
        writeByte(GAVEL_TCHUNK);
        writeString((char*)chunk->name.c_str());
        writeBool(chunk->returnable);
        writeBool(chunk->scoped);
        // first, write the constants
        writeConstants(chunk->consts);
        // then, write the instructions
        writeInstructions(chunk->chunk);
        // write the debug info,,,
        writeDebugInfo(chunk->debugInfo);
        // and finally write the identifiers list!
        writeIdentifiers(chunk->identifiers);
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

    char* getRawString() {
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
                    char* buf = getRawString();
                    DEBUGLOG(std::cout << "[STRING] : " << buf << std::endl);
                    consts.push_back(CREATECONST_STRING(buf));
                    delete buf;
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

    std::vector<std::string> getIdentifiers() {
        std::vector<std::string> ids;
        int num = getSizeT(); // number of unique identifiers
        for (int i = 0; i < num; i++) {
            char* buf = getRawString();
            std::string str(buf);
            ids.push_back(str);
            delete buf; // clean up
        }
        return ids;
    }

    GChunk* getChunk() {
        std::vector<GChunk*> childChunks;

        if (getByte() != GAVEL_TSTRING)
            return NULL;
        char* name = getRawString();
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
        std::vector<std::string> idents = getIdentifiers();

        GChunk* chunk = new GChunk(name, insts, debugInfo, consts, idents, returnable, scoped);

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