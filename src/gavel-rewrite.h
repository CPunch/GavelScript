/* 

     ██████╗  █████╗ ██╗   ██╗███████╗██╗     ███████╗ ██████╗██████╗ ██╗██████╗ ████████╗
    ██╔════╝ ██╔══██╗██║   ██║██╔════╝██║     ██╔════╝██╔════╝██╔══██╗██║██╔══██╗╚══██╔══╝
    ██║  ███╗███████║██║   ██║█████╗  ██║     ███████╗██║     ██████╔╝██║██████╔╝   ██║   
    ██║   ██║██╔══██║╚██╗ ██╔╝██╔══╝  ██║     ╚════██║██║     ██╔══██╗██║██╔═══╝    ██║   
    ╚██████╔╝██║  ██║ ╚████╔╝ ███████╗███████╗███████║╚██████╗██║  ██║██║██║        ██║   
     ╚═════╝ ╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚══════╝╚══════╝ ╚═════╝╚═╝  ╚═╝╚═╝╚═╝        ╚═╝                                                                 
                        Copyright (c) 2019-2020, Seth Stubbs
    
    Version 1.0
        - Complete rewrite
        - C++17 compliant 
        - Better parser based off of a varient of a Pratt parser 
        - Better stack management
        - Better memory management using mark & sweep garbage collector
        - Easy to use C++ API built for the lazy developer
        - Closures, better scopes, etc.
        - Platform-independent serializer

    Any version of GavelScript prior to this was a necessary evil :(, it was a learning curve okay....
*/

#ifndef _GSTK_HPP
#define _GSTK_HPP

#include <cstdio>
#include <cstring>
#include <sstream>
#include <cmath>
#include <cstdint>

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <map>
#include <unordered_map>

// add x to show debug info
#define DEBUGLOG(x) 
// logs specifically for the garbage collector
#define DEBUGGC(x) 

// version info
#define GAVEL_MAJOR "1"
#define GAVEL_MINOR "0"

// because of this, recursion is limited to 64 calls deep. (aka, FEEL FREE TO CHANGE THIS BASED ON YOUR NEEDS !!!)
#define CALLS_MAX 64
#define STACK_MAX CALLS_MAX * 8
#define MAX_LOCALS STACK_MAX - 1

// enables string interning if defined
#define GSTRING_INTERN

// excludes the compiler/lexer if defined. (this also removes compileString in the API!)
//#define EXCLUDE_COMPILER

// this only tracks memory DYNAMICALLY allocated for GObjects! the other memory is cleaned and managed by their respective classes or the user.
//  * this will dynamically change, balancing the work.
#define GC_INITALMEMORYTHRESH 1024 * 16

// defines a max string count before causing a garbage collection. will be ignored if string interning is disabled!
//  * this will dynamically change, balancing the work.
#define GC_INITIALSTRINGSTHRESH 128

// switched to 32bit instructions!
typedef uint32_t INSTRUCTION;

/* 
    Instructions & bitwise operations to get registers

        64 possible opcodes due to it being 6 bits. 16 currently used. Originally used positional arguments in the instructions for stack related operations, however that limited the stack size &
    that made the GavelCompiler needlessly complicated :(. Instruction encoding changes on the daily vro.

    Instructions are 32bit integers with everything encoded in it, currently there are 3 types of intructions:
        - i
            - 'Opcode' : 6 bits
            - 'Reserved space' : 26 bits 
        - iAx
            - 'Opcode' : 6 bits
            - 'Ax' : 26 bits [MAX: 67108864]
        - iAxs
            - 'Opcode' : 6 bits
            - 'Axs' : 26 bits [-33554432/33554432]
                    It's stored as a generic unsigned integer, however to simulate the sign bit, the raw value of the unsigned integer has it's value subtracted by the max a 25
                bit unsigned int can hold (2^25 or 33554432)
        - iAB
            - 'Opcode' : 6 bits
            - 'A' : 13 bits [MAX: 8192]
            - 'B' : 13 bits [MAX: 8192]
*/
#define SIZE_OP		        6
#define SIZE_Ax		        26
#define SIZE_Bx		        18
#define SIZE_A		        8
#define SIZE_B		        9
#define SIZE_C		        9

#define MAXREG_Ax           pow(2, SIZE_Ax)
// 1 bit is simulated for the sign bit
#define MAXREG_Axs          pow(2, SIZE_Ax-1)
#define MAXREG_Bx           pow(2, SIZE_Bx)
#define MAXREG_A            pow(2, SIZE_A)
#define MAXREG_B            pow(2, SIZE_B)
#define MAXREG_C            pow(2, SIZE_C)

#define POS_OP		        0
#define POS_A		        (POS_OP + SIZE_OP)
#define POS_B		        (POS_A + SIZE_A)
#define POS_C		        (POS_B + SIZE_B)

// creates a mask with `n' 1 bits
#define MASK(n)	            (~((~(INSTRUCTION)0)<<n))

#define GET_OPCODE(i)	    (OPCODE)(((OPCODE)((i)>>POS_OP)) & MASK(SIZE_OP))
#define GETARG_Ax(i)	    (int)(((i)>>POS_A) & MASK(SIZE_Ax))
#define GETARG_Axs(i)	    (((int)(((i)>>POS_A) & MASK(SIZE_Ax))) - MAXREG_Axs) 
#define GETARG_Bx(i)        (int)(((i)>>POS_B) & MASK(SIZE_Bx))
#define GETARG_A(i)	        (int)(((i)>>POS_A) & MASK(SIZE_A))
#define GETARG_B(i)	        (int)(((i)>>POS_B) & MASK(SIZE_B))
#define GETARG_C(i)	        (int)(((i)>>POS_C) & MASK(SIZE_C))

/* These will create bytecode instructions. (Look at OPCODE enum right below this for references)
    o: OpCode, eg. OP_POP
    a: Ax, A eg. 1
    b: B eg. 2

    TODO: use more register-based instructions, eg. OP_ADD A, B, C:
        A: slot 1
        B: slot 2
        C: slot 3 to move result too, could be slot 1 or 2 to replace the value on the stack
*/
#define CREATE_i(o)	            (((INSTRUCTION)(o))<<POS_OP)
#define CREATE_iAx(o,a)	        ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A))
#define CREATE_iABx(o,a,b)      ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A) | (((INSTRUCTION)(b))<<POS_B))
#define CREATE_iABC(o,a,b,c)    ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A) | (((INSTRUCTION)(b))<<POS_B) | (((INSTRUCTION)(c))<<POS_C))

// ===========================================================================[[ VIRTUAL MACHINE ]]===========================================================================

typedef enum {
    OPTYPE_I,
    OPTYPE_IAX,
    OPTYPE_CLOSURE
} OPTYPE;

typedef enum { // [MAX : 64] 
    //              ===============================[[STACK MANIPULATION]]===============================
    OP_LOADCONST,   // iAx - Loads chunk->const[Ax] and pushes the value onto the stack
    OP_DEFINEGLOBAL, //iAx - Sets stack[top] to global[chunk->identifiers[Ax]]
    OP_GETGLOBAL,   // iAx - Pushes global[chunk->identifiers[Ax]]
    OP_SETGLOBAL,   // iAx - sets stack[top] to global[chunk->identifiers[Ax]]
    OP_GETBASE,     // iAx - Pushes stack[base-Ax] to the stack
    OP_SETBASE,     // iAx - Sets stack[base-Ax] to stack[top] (after popping it of course)
    OP_GETUPVAL,    // iAx - Grabs upval[Ax]
    OP_SETUPVAL,    // iAx - Sets upval[Ax] with stack[top] 
    OP_CLOSURE,     // iAx - Makes a closure with FUNC at const[Ax]
    OP_CLOSE,       // iAx - Closes local at stack[base-Ax] to the heap, doesn't pop however.
    OP_POP,         // iAx - pops values from the stack Ax times
     
    //              ===================================[[CONTROL FLOW]]=================================
    OP_IFJMP,       // iAx - if stack.pop() is false, state->pc + Ax, 
    OP_CNDNOTJMP,   // iAx - if stack[top] is false, state->pc + Ax 
    OP_CNDJMP,      // iAx - if stack[top] is true, state->pc + Ax 
    OP_JMP,         // iAx - state->pc += Ax
    OP_JMPBACK,     // iAx - state->pc -= Ax
    OP_CALL,        // iAx - calls fnuction or cfunction at stack[top-Ax] with Ax args

    //              ==============================[[TABLES && METATABLES]]==============================
    OP_INDEX,       // i - indexes stack[top-1] with stack[top]
    OP_NEWINDEX,    // i - sets stack[top-2] at index stack[top-1] with stack[top]

    //              ==================================[[CONDITIONALS]]==================================
    OP_EQUAL,       // i - pushes (stack[top] == stack[top-1])
    OP_GREATER,     // i - pushes (stack[top] > stack[top-1])
    OP_LESS,        // i - pushes (stack[top] < stack[top-1])

    //              ===================================[[BITWISE OP]]===================================
    OP_NEGATE,      // i - Negates stack[top], sets to stack[base-Ax], or if Ax is negative, push result onto the stack
    OP_NOT,         // i - falsifies stack[top], sets to stack[base-Ax], or if Ax is negative, push result onto the stack
    OP_ADD,         // i - adds stack[top] to stack[top-1], sets to stack[base-Ax], or if Ax is negative, push result onto the stack
    OP_SUB,         // i - subs stack[top] from stack[top-1], sets to stack[base-Ax], or if Ax is negative, push result onto the stack
    OP_MUL,         // i - multiplies stack[top] with stack[top-1], sets to stack[base-Ax], or if Ax is negative, push result onto the stack
    OP_DIV,         // i - divides stack[top] with stack[top-1], sets to stack[base-Ax], or if Ax is negative, push result onto the stack
    
    OP_INC,         // iAx - Increments stack[top] by 1, pushes 2 results onto the stack. one to use to assign, one to use as a value. Ax == 1: pushed value is pre inc; Ax == 2: pushed value is post inc.
    OP_DEC,         // iAx - Decrements stack[top] by 1, pushes 2 results onto the stack. one to use to assign, one to use as a value. Ax == 1: pushed value is pre dec; Ax == 2: pushed value is post dec.

    //              ====================================[[LITERALS]]====================================
    OP_TRUE,
    OP_FALSE,
    OP_NIL,
    OP_NEWTABLE,    // iAx - Ax is the amount of key/value pairs on the stack to create the table out of

    //              ================================[[MISC INSTRUCTIONS]]===============================
    OP_RETURN,      // iAx - returns Ax args while popping the function off the stack and returning to the previous function
} OPCODE;

const OPTYPE GInstructionTypes[] { // [MAX : 64] 
    OPTYPE_IAX,     // OP_LOADCONST
    OPTYPE_IAX,     // OP_DEFINEGLOBAL
    OPTYPE_IAX,     // OP_GETGLOBAL
    OPTYPE_IAX,     // OP_SETGLOBAL
    OPTYPE_IAX,     // OP_GETBASE
    OPTYPE_IAX,     // OP_SETBASE
    OPTYPE_IAX,     // OP_GETUPVAL
    OPTYPE_IAX,     // OP_SETUPVAL
    OPTYPE_CLOSURE, // OP_CLOSURE
    OPTYPE_IAX,     // OP_CLOSE
    OPTYPE_IAX,     // OP_POP
     
    OPTYPE_IAX,     // OP_IFJMP
    OPTYPE_IAX,     // OP_CNDNOTJMP
    OPTYPE_IAX,     // OP_CNDJMP
    OPTYPE_IAX,     // OP_JMP
    OPTYPE_IAX,     // OP_JMPBACK
    OPTYPE_IAX,     // OP_CALL
    
    OPTYPE_I,       // OP_INDEX
    OPTYPE_I,       // OP_NEWINDEX

    OPTYPE_I,       // OP_EQUAL
    OPTYPE_I,       // OP_GREATER
    OPTYPE_I,       // OP_LESS
    
    OPTYPE_I,       // OP_NEGATE
    OPTYPE_I,       // OP_NOT
    OPTYPE_I,       // OP_ADD
    OPTYPE_I,       // OP_SUB
    OPTYPE_I,       // OP_MUL
    OPTYPE_I,       // OP_DIV

    OPTYPE_IAX,     // OP_INC
    OPTYPE_I,       // OP_DEC
    
    OPTYPE_I,       // OP_TRUE
    OPTYPE_I,       // OP_FALSE
    OPTYPE_I,       // OP_NIL
    OPTYPE_IAX,     // OP_NEWTABLE

    OPTYPE_IAX      // OP_RETURN
};

typedef enum {
    GAVELSTATE_RESUME,
    GAVELSTATE_YIELD,
    GAVELSTATE_END,
    GAVELSTATE_PANIC, // state when an objection occurs
    GAVELSTATE_RETURNING // climbs chunk hierarchy until current chunk is returnable
} GAVELSTATE;

// type info for everything thats allowed on the stack
typedef enum {
    GAVEL_TNIL,
    GAVEL_TBOOLEAN, // bool
    GAVEL_TNUMBER, // double
    GAVEL_TOBJ // references objects
} GType;

// for GObjects
typedef enum {
    GOBJECT_NULL,
    GOBJECT_STRING,
    GOBJECT_TABLE, // comparable to objects, basically a hashtable
    GOBJECT_PROTOTABLE, // wrapper type for c++ stuffs
    GOBJECT_FUNCTION,
    GOBJECT_CFUNCTION,
    GOBJECT_BOUNDCALL, // for internal vm use (connecting c functions to prototable)
    GOBJECT_CLOSURE, // for internal vm use
    GOBJECT_UPVAL, // for internal vm use
    GOBJECT_OBJECTION // holds objections, external vm use lol
} GObjType;

// so we can refernece pointers :)
struct GValue;
struct GChunk;
class GState;

// cfunction typedef (state, args)
typedef GValue (*GAVELCFUNC)(GState*, int);

/* GObjection
    Holds information on objections for both the parser and virtual machine
*/
class GObjection {
private:
    std::string err;
    std::string chunkName = "_MAIN"; // by default
    int line;

public:
    GObjection() {}
    GObjection(std::string e, int l):
        err(e), line(l) {}
    GObjection(std::string e, std::string cn, int l):
        err(e), chunkName(cn), line(l) {}
    
    std::string getFormatedString() {
        return "[line " + std::to_string(line) + "] OBJECTION! (in " + chunkName + "):\n\t" + err;
    }
    std::string getString() {
        return err;
    }
};

/* GObjects
    This class is a baseclass for all GValue objects.
*/
class GObject {
public:
    GObjType type = GOBJECT_NULL;
    bool isGray = false; // for our garbage collector
    GObject* next = NULL; // linked list for our garbage collector as well :)

    GObject() {}
    virtual ~GObject() {}

    virtual bool equals(GObject*) { return false; };
    virtual std::string toString() { return ""; };
    virtual std::string toStringDataType() { return ""; };
    virtual GObject* clone() {return new GObject(); };
    virtual int getHash() {return std::hash<GObjType>()(type); };
    virtual size_t getSize() { return sizeof(GObject); }; // return the size of a GObject in bytes

    // so we can easily compare GValues
    bool operator==(GObject& other)
    {
        return this->equals(&other);
    }
};

class GObjectString : public GObject {
public:
    std::string val;
    int hash;

    GObjectString(std::string& b):
        val(b) {
        type = GOBJECT_STRING;
        // make a hash specific for the type and the string
        hash = std::hash<GObjType>()(GOBJECT_STRING) ^ std::hash<std::string>()(val);
    }

    virtual ~GObjectString() {};

    bool equals(GObject* other) {
        if (other->type == type) {
            return reinterpret_cast<GObjectString*>(other)->val.compare(val) == 0;
        }
        return false;
    }

    std::string toString() {
        return val;
    }

    std::string toStringDataType() {
        return "[STRING]";
    }

    GObject* clone() {
        return new GObjectString(val);
    }

    int getHash() {
        return hash;
    }

    size_t getSize() { 
        return sizeof(GObjectString); 
    };
};

class GObjectCFunction : public GObject {
public:
    GAVELCFUNC val;
    int hash;

    GObjectCFunction(GAVELCFUNC b):
        val(b) {
        type = GOBJECT_CFUNCTION;
        // make a hash specific for the type and the string
        hash = std::hash<GObjType>()(GOBJECT_CFUNCTION);
    }

    virtual ~GObjectCFunction() {};

    bool equals(GObject* other) {
        if (other->type == type) {
            return reinterpret_cast<GObjectCFunction*>(other)->val == val;
        }
        return false;
    }

    std::string toString() {
        std::stringstream out;
        out << &val;
        return out.str();
    }

    std::string toStringDataType() {
        return "[C FUNCTION]";
    }

    GObject* clone() {
        return new GObjectCFunction(val);
    }

    int getHash() {
        return hash;
    }

    size_t getSize() { 
        return sizeof(GObjectCFunction); 
    };
};

class GObjectObjection : public GObject {
public:
    GObjection val;

    GObjectObjection(GObjection o):
        val(o) {
        type = GOBJECT_OBJECTION;
    }
    
    virtual ~GObjectObjection() {}

    std::string toString() {
        return val.getFormatedString();
    }

    std::string toStringDataType() {
        return "[OBJECTION]";
    }

    GObject* clone() {
        return new GObjectObjection(val);
    }

    int getHash() {
        // make a hash specific for the type and the string
        return std::hash<GObjType>()(type) ^ std::hash<std::string>()(val.getFormatedString());
    }

    size_t getSize() { 
        return sizeof(GObjectObjection); 
    };
};

// holds primitives
struct GValue {
    GType type;
    union {
        bool boolean;
        double number;
        GObject* obj;
    } val;

    GValue() {
        type = GAVEL_TNIL;
    }

    GValue(bool b) {
        type = GAVEL_TBOOLEAN;
        val.boolean = b;
    }

    GValue(double n) {
        type = GAVEL_TNUMBER;
        val.number = n;
    }

    GValue(GObject* o) {
        type = GAVEL_TOBJ;
        val.obj = o;
    }

    bool equals(GValue v) {
        if (v.type != type) 
            return false;
        
        switch (type) {
            case GAVEL_TNIL:
                return true;
            case GAVEL_TBOOLEAN:
                return v.val.boolean == val.boolean;
            case GAVEL_TNUMBER:
                return v.val.number == val.number;
            case GAVEL_TOBJ:
                return v.val.obj->equals(val.obj);
            default:
                return false;
        }
    }

    std::string toStringDataType() {
        switch (type) {
            case GAVEL_TNIL:
                return "[NIL]";
            case GAVEL_TBOOLEAN:
                return "[BOOL]";
            case GAVEL_TNUMBER:
                return "[NUMBER]";
            case GAVEL_TOBJ:
                return val.obj->toStringDataType();
            default:
                return "[ERR]";
        }
    }

    std::string toString() {
        switch (type) {
            case GAVEL_TBOOLEAN:
                return val.boolean ? "True" : "False";
            case GAVEL_TNUMBER: {
                return std::to_string(val.number);
            }
            case GAVEL_TOBJ: 
                return val.obj->toString();
            case GAVEL_TNIL:
            default:
                return "Nil";
        }
    }

    int getHash() {
        switch (type) {
            case GAVEL_TBOOLEAN:
                return std::hash<GType>()(type) ^ std::hash<bool>()(val.boolean);
            case GAVEL_TNUMBER: 
                return std::hash<GType>()(type) ^ std::hash<double>()(val.number);
            case GAVEL_TOBJ:
                return val.obj->getHash();
            case GAVEL_TNIL:
            default:
                return std::hash<GType>()(type);
        }
    }
};

#define CREATECONST_NIL()       GValue()
#define CREATECONST_BOOL(b)     GValue((bool)b)
#define CREATECONST_NUMBER(n)   GValue((double)n)
#define CREATECONST_STRING(x)   GValue((GObject*)Gavel::addString(x))
#define CREATECONST_CFUNCTION(x)GValue((GObject*)new GObjectCFunction(x))
#define CREATECONST_CLOSURE(x)  GValue((GObject*)new GObjectClosure(x))
#define CREATECONST_OBJECTION(x)GValue((GObject*)new GObjectObjection(x))
#define CREATECONST_TABLE()     GValue((GObject*)new GObjectTable())

#define READOBJECTVALUE(x, type)reinterpret_cast<type>(x)->val

#define READGVALUEBOOL(x)       x.val.boolean
#define READGVALUENUMBER(x)     x.val.number
#define READGVALUESTRING(x)     READOBJECTVALUE(x.val.obj, GObjectString*)
#define READGVALUEFUNCTION(x)   READOBJECTVALUE(x.val.obj, GObjectFunction*)
#define READGVALUECFUNCTION(x)  READOBJECTVALUE(x.val.obj, GObjectCFunction*)
#define READGVALUECLOSURE(x)    READOBJECTVALUE(x.val.obj, GObjectClosure*)
#define READGVALUEOBJECTION(x)  READOBJECTVALUE(x.val.obj, GObjectObjection*)
#define READGVALUETABLE(x)      READOBJECTVALUE(x.val.obj, GObjectTable*)
#define READGVALUEPROTOTABLE(x) READOBJECTVALUE(x.val.obj, GObjectPrototable*)

#define ISGVALUEBOOL(x)         (x.type == GAVEL_TBOOLEAN)
#define ISGVALUENUMBER(x)       (x.type == GAVEL_TNUMBER)
#define ISGVALUENIL(x)          (x.type == GAVEL_TNIL)

#define ISGVALUEOBJ(x)          (x.type == GAVEL_TOBJ)

// treat this like a macro, this is to protect against macro expansion and causing undefined behavior :eyes:
inline bool ISGVALUEOBJTYPE(GValue v, GObjType t) {
    return ISGVALUEOBJ(v) && v.val.obj->type == t;
}

#define ISGVALUESTRING(x)       ISGVALUEOBJTYPE(x, GOBJECT_STRING)
#define ISGVALUEOBJECTION(x)    ISGVALUEOBJTYPE(x, GOBJECT_OBJECTION)
#define ISGVALUEFUNCTION(x)     ISGVALUEOBJTYPE(x, GOBJECT_FUNCTION)
#define ISGVALUECFUNCTION(x)    ISGVALUEOBJTYPE(x, GOBJECT_CFUNCTION)
#define ISGVALUECLOSURE(x)      ISGVALUEOBJTYPE(x, GOBJECT_CLOSURE)
#define ISGVALUEOBJECTION(x)    ISGVALUEOBJTYPE(x, GOBJECT_OBJECTION)
#define ISGVALUETABLE(x)        ISGVALUEOBJTYPE(x, GOBJECT_TABLE)
#define ISGVALUEPROTOTABLE(x)   ISGVALUEOBJTYPE(x, GOBJECT_PROTOTABLE)
#define ISGVALUEBASETABLE(x)    (ISGVALUETABLE(x) || ISGVALUEPROTOTABLE(x))

#define FREEGVALUEOBJ(x)        delete x.val.obj

class GObjectUpvalue : public GObject {
public:
    GValue* val;
    GValue closed; // nil by default, this is for when an upval is moved from the stack to the heap
    GObjectUpvalue* nextUpval = NULL;
    int hash;

    GObjectUpvalue(GValue* b):
        val(b) {
        type = GOBJECT_UPVAL;
        // make a hash specific for the type and the string
        hash = std::hash<GObjType>()(GOBJECT_UPVAL) ^ std::hash<GValue*>()(val);
    }

    virtual ~GObjectUpvalue() {};

    bool equals(GObject* other) {
        if (other->type == type) {
            return reinterpret_cast<GObjectUpvalue*>(other)->val == val;
        }
        return false;
    }

    std::string toString() {
        return "[UPVAL] LINKED TO: " + std::to_string((intptr_t)val);
    }

    std::string toStringDataType() {
        return "[UPVAL]";
    }

    GObject* clone() {
        return new GObjectUpvalue(val);
    }

    int getHash() {
        return hash;
    }

    size_t getSize() { 
        return sizeof(GObjectUpvalue); 
    };
};

/*  GTable
        This is GavelScript's custom hashtable implementation. This is so we can use string interning, which is a lowlevel optimization where we can shorten comparison times by just comparing the 
    addresses and not comparing the actual memory contents. (can be enabled/disabled using GSTRING_INTERN)
*/
template<typename T>
class GTable {
private:
    struct Entry {
        T key; // dynamically allocated, also may point to the same string due to string interning

        Entry(T k):
            key(k) {}

        bool operator == (const Entry& other) const {
            if constexpr (std::is_same<T, GObjectString*>()) {
                #ifdef GSTRING_INTERN
                            return key == other.key;
                #else
                            return key->equals(other.key);
                #endif
            } else if constexpr (std::is_same<T, GObject*>()) {
                return key->equals(other.key);
            } else if constexpr (std::is_same<T, GValue>()) {
                return ((GValue)key).equals(other.key);
            }
        }
    };

    struct hash_fn {
        std::size_t operator() (Entry v) const {
            if constexpr (std::is_same<T, GObjectString*>() || std::is_same<T, GObject*>()) {
                return v.key->getHash();
            } else if constexpr (std::is_same<T, GValue>()) {
                return v.key.getHash();
            }
        }
    };


public:
    std::unordered_map<Entry, GValue, hash_fn> hashTable;
    GTable() {}

    T findExistingKey(T key) {
        // this is why hashes need to be *pretty* unique! however hash collisions will always exist so if the hash matches, compare the memory anyways
        for (std::pair<Entry, GValue> pair : hashTable) {
            if (pair.first.key->getHash() == key->getHash() && pair.first.key->equals(key))
                return pair.first.key;
        }

        return NULL;
    }

    bool checkValidKey(T key) {
        return hashTable.find(key) != hashTable.end();
    }
    
    GValue getIndex(T key) {
        // if key exists, return it
        auto res = hashTable.find(key);
        if (res != hashTable.end()) {
            return res->second;
        }

        // otherwise return NIL
        return CREATECONST_NIL();
    }

    void setIndex(T key, GValue value) {
        hashTable[key] = value;
    }

    // returns true if index already existed
    bool checkSetIndex(T key, GValue v) {
        auto res = hashTable.find(key);
        if (res != hashTable.end()) {
            res->second = v;
            return true;
        }

        hashTable[key] = v;
        return false;
    }

    std::vector<T> getVectorOfKeys() {
        std::vector<T> keys;
        for (std::pair<Entry, GValue> pair : hashTable) {
            keys.push_back(pair.first.key);
        }
        return keys;
    }

    auto deleteKey(T key) {
        return hashTable.erase(key);
    }

    int getSize() {
        return hashTable.size();
    }

    void printTable() {
        for (std::pair<Entry, GValue> pair : hashTable) {
            std::cout << pair.first.key->toString() << " : " << pair.second.toString() << std::endl;
        }
    }
};

namespace Gavel {
    GObjectString* addString(std::string str);
    GState* newState();
    void freeState(GState*);
    GChunk* newChunk();
    void freeChunk(GChunk* ch);
    void checkGarbage();
    void collectGarbage();
    void addGarbage(GObject* g);

    void markObject(GObject* o);
    void markValue(GValue val);

    template <typename T>
    void markTable(GTable<T>* tbl);

    template <typename T>
    inline GValue newGValue(T x);
}

class GObjectTableBase : public GObject {
public:
    GObjectTableBase() {}
    virtual ~GObjectTableBase() {}

    virtual GValue getIndex(GValue key) { return CREATECONST_NIL(); }
    virtual void setIndex(GValue key, GValue v) {}

    // template versions
    template <typename T>
    GValue getIndex(T key) {
        return this->getIndex(Gavel::newGValue(key));
    }

    template <typename T, typename T2>
    void setIndex(T key, T2 v) {
        this->setIndex(Gavel::newGValue(key), Gavel::newGValue(v));
    }
};

// Similar to closures, however this binds a c function to a prototable
class GObjectBoundCall : public GObject {
public:
    GAVELCFUNC var;
    GObjectTableBase* tbl;
    bool alive; // this will keep track if the bound table is still alive.

    GObjectBoundCall(GAVELCFUNC cfunc, GObjectTableBase* tblObj): 
        var(cfunc), tbl(tblObj) {
        type = GOBJECT_BOUNDCALL;
        alive = true;
    }
};

class GObjectTable : public GObjectTableBase {
public:
    GTable<GValue> val;
    int hash;

    GObjectTable(GTable<GValue> v = GTable<GValue>()): val(v) {
        type = GOBJECT_TABLE;
        hash = 1;
    }

    virtual ~GObjectTable() {};

    bool equals(GObject* other) {
        // todo
        return false;
    }

    std::string toString() {
        std::stringstream out;
        out << "Table " << this;
        return out.str();
    }

    std::string toStringDataType() {
        return "[TABLE]";
    }

    GObject* clone() {
        return new GObjectTable();
    }

    int getHash() {
        return hash;
    }

    size_t getSize() { 
        return sizeof(GObjectTable); 
    };

    // Methods specifically for GObjectTable
    GValue getIndex(GValue key) {
        return val.getIndex(key);
    }

    void setIndex(GValue key, GValue v) {
        val.setIndex(key, v);
    }
};

// lets you wrap a pointer to a c++ object, lets scripts interact with c++ objects easily
class GObjectPrototable : public GObjectTableBase {
private:
    int hash;

    class GProto {
    public:
        bool readOnly = false;
        GProto() {}
        virtual ~GProto() {}

        virtual void set(GValue t) {}
        virtual GValue get() { return CREATECONST_NIL(); }
        virtual void mark() {} // mark active objects
    };

    template<typename T>
    class GProtoNumber : public GProto {
    public:
        T* val;

        GProtoNumber(T* v): val(v) {}
        ~GProtoNumber() {}

        void set(GValue v) {
            if (ISGVALUENUMBER(v) && !readOnly) {
                *val = (T)READGVALUENUMBER(v);
            }
        }

        GValue get() {
            return CREATECONST_NUMBER((double)*val);
        }
    };

    class GProtoBool : public GProto {
    public:
        bool* val;

        GProtoBool(bool* v): val(v) {}
        ~GProtoBool() {}

        void set(GValue v) {
            if (ISGVALUEBOOL(v) && !readOnly) {
                *val = READGVALUEBOOL(v);
            }
        }

        GValue get() {
            return CREATECONST_BOOL(val);
        }   
    };

    class GProtoString : public GProto {
    public:
        std::string* val;

        GProtoString(std::string* v): val(v) {}
        ~GProtoString() {}

        void set(GValue v) {
            if (ISGVALUESTRING(v) && !readOnly) {
                *val = READGVALUESTRING(v);
            }
        }

        GValue get() {
            return CREATECONST_STRING(*val);
        }
    };

    class GProtoCFunction : public GProto {
    public:
        // this will need to be added to our tracked garbage objects. the user could store the bound call in a global var. :(
        GObjectBoundCall* val;

        GProtoCFunction(GAVELCFUNC v, GObjectTableBase* bse) {
            val = new GObjectBoundCall(v, bse);

            // add it to the vm garbage
            Gavel::addGarbage((GObject*)val);
        }

        ~GProtoCFunction() {
            val->alive = false;
        }

        void set(GValue v) {
            // you can't set this property!
        }

        GValue get() {
            return GValue((GObject*)val);
        }

        void mark() {
            Gavel::markObject((GObject*)val);
        }
    };

    template<typename T>
    GProto* newGProto(T v) {
        GProto* result = NULL;

        // GProtoNumber
        if constexpr (std::is_same<T, double*>()) {
            result = new GProtoNumber<double>(v);
        } else if constexpr (std::is_same<T, float*>()) {
            result = new GProtoNumber<float>(v);
        } else if constexpr (std::is_same<T, int*>()) {
            result = new GProtoNumber<int>(v);
        // GProtoBool
        } else if constexpr (std::is_same<T, bool*>()) {
            result = new GProtoBool(v);
        // GProtoString
        } else if constexpr (std::is_same<T, std::string*>()) {
            result = new GProtoString(v);
        // GProtoCFunction
        } else if constexpr (std::is_same<T, GAVELCFUNC>()) {
            result = new GProtoCFunction(v, this);
        }

        return result;
    }

    struct Entry {
        GValue key;

        Entry(GValue k):
            key(k) {}

        bool operator == (const Entry& other) const {
            return ((GValue)key).equals(other.key);
        }
    };

    struct hash_fn {
        std::size_t operator() (Entry v) const {
            return v.key.getHash();
        }
    };

public:
    std::unordered_map<Entry, GProto*, hash_fn> hashTable;
    void* val;

    GObjectPrototable(void* v = NULL): 
        val(v) {
            type = GOBJECT_PROTOTABLE;
            hash = 2;
        }

    // clean up GProtos
    ~GObjectPrototable() {
        for (auto pair : hashTable) {
            delete pair.second;
        }
    }

    bool equals(GObject* other) {
        // todo
        return false;
    }

    std::string toString() {
        std::stringstream out;
        out << "Prototable " << this << " for " << val;
        return out.str();
    }

    std::string toStringDataType() {
        return "[PROTOTABLE]";
    }

    GObject* clone() {
        return new GObjectPrototable();
    }

    int getHash() {
        return hash;
    }

    size_t getSize() { 
        return sizeof(GObjectPrototable); 
    };
    
    template<typename T, typename T2>
    void newIndex(T key, T2 pointer) {
        GProto* value = newGProto(pointer);

        if (value != NULL) { // a valid GProto
            hashTable[Gavel::newGValue(key)] = value;
        }
    }

    GValue getIndex(GValue key) {
        auto res = hashTable.find(key);
        if (res != hashTable.end())
            return res->second->get();

        return CREATECONST_NIL();
    }

    void setIndex(GValue key, GValue v) {
        auto res = hashTable.find(key);
        if (res != hashTable.end())
            res->second->set(v);
    }
};

// defines a chunk. each chunk has locals
struct GChunk {
    GChunk* next = NULL; // for gc linked list
    std::vector<INSTRUCTION> code;
    std::vector<GValue> constants;
    std::vector<GObjectString*> identifiers;
    std::vector<int> lineInfo;

    // default constructor
    GChunk() {}

    ~GChunk() {
        DEBUGGC(std::cout << "-- FREEING CHUNK " << this << std::endl);
        // frees all of the constants, since we own them
        for (GValue c : constants) {
            if (ISGVALUEOBJ(c) && !ISGVALUESTRING(c)) {
                DEBUGGC(std::cout << "freeing " << c.toStringDataType() << " : " << c.toString() << std::endl);
                FREEGVALUEOBJ(c);
            }
        }
        DEBUGGC(std::cout << "-- DONE FREEING CHUNK " << this << std::endl);
    }

    int addInstruction(INSTRUCTION i, int line) {
        // add INSTRUCTION to our instruction table
        code.push_back(i);
        lineInfo.push_back(line);
        return code.size() - 1; // returns index
    }

    void patchInstruction(int i, INSTRUCTION inst) {
        code[i] = inst;
    }

    int addIdentifier(std::string id) {
        int tmpId = findIdentifier(id);
        if (tmpId != -1)
            return tmpId;

        identifiers.push_back(Gavel::addString(id));
        return identifiers.size() - 1;
    }

    int findIdentifier(std::string id) {
        for (int i = 0; i < identifiers.size(); i++) {
            if (identifiers[i]->val.compare(id) == 0)
                return i;
        }

        return -1; // identifier doesn't exist
    }

    void markRoots() {
        // mark identifiers
        for (GObjectString* s : identifiers) {
            Gavel::markObject((GObject*)s);
        }
        
        // mark constants
        for (GValue c : constants) {
            Gavel::markValue(c);
        }

    }

    // GChunk now owns the GValue, so you don't have to worry about freeing it if it's a GObject
    int addConstant(GValue c) {
        // check if we already have an identical constant in our constant table, if so return the index
        for (int i  = 0; i < constants.size(); i++) {
            GValue oc = constants[i];
            if (oc.equals(c)) {
                if (ISGVALUEOBJ(c) && !ISGVALUESTRING(c)) { // free unused object
                    FREEGVALUEOBJ(c);
                }
                return i;
            }
        }

        // else, add it to the constant table and return the new index!!
        constants.push_back(c);
        return constants.size() - 1;
    }

    static const std::string getOpCodeName(OPCODE op) {
        switch (op) {
            case OP_LOADCONST:
                return "OP_LOADCONST";
            case OP_DEFINEGLOBAL: 
                return "OP_DEFINEGLOBAL";
            case OP_GETGLOBAL: 
                return "OP_GETGLOBAL";
            case OP_SETGLOBAL: 
                return "OP_SETGLOBAL";
            case OP_GETBASE: 
                return "OP_GETBASE";
            case OP_SETBASE: 
                return "OP_SETBASE";
            case OP_GETUPVAL: 
                return "OP_GETUPVAL";
            case OP_SETUPVAL: 
                return "OP_SETUPVAL";
            case OP_CLOSURE: 
                return "OP_CLOSURE";
            case OP_CLOSE: 
                return "OP_CLOSE";
            case OP_POP: 
                return "OP_POP";
            case OP_IFJMP: 
                return "OP_IFJMP";
            case OP_CNDNOTJMP: 
                return "OP_CNDNOTJMP";
            case OP_CNDJMP: 
                return "OP_CNDJMP";
            case OP_JMP: 
                return "OP_JMP";
            case OP_JMPBACK: 
                return "OP_BACKJMP";
            case OP_CALL: 
                return "OP_CALL";
            case OP_INDEX: 
                return "OP_INDEX";
            case OP_NEWINDEX: 
                return "OP_NEWINDEX";
            case OP_EQUAL: 
                return "OP_EQUAL";
            case OP_GREATER:
                return "OP_GREATER";
            case OP_LESS: 
                return  "OP_LESS";
            case OP_NEGATE: 
                return "OP_NEGATE";
            case OP_NOT: 
                return "OP_NOT";
            case OP_ADD: 
                return "OP_ADD";
            case OP_SUB: 
                return "OP_SUB";
            case OP_MUL: 
                return "OP_MUL";
            case OP_DIV: 
                return "OP_DIV";
            case OP_INC: 
                return "OP_INC";
            case OP_DEC: 
                return "OP_DEC";
            case OP_TRUE: 
                return "OP_TRUE";
            case OP_FALSE: 
                return "OP_FALSE";
            case OP_NIL:
                return "OP_NIL ";
            case OP_NEWTABLE:
                return "OP_NEWTABLE";
            case OP_RETURN: 
                return "OP_RETURN";
            default:
                return  "ERR. INVALID OP [" + std::to_string(op) + "]";
        }
    }

    void disassemble(int);
};

class GObjectFunction : GObject {
private:
    int expectedArgs;
    int upvalues = 0;
    std::string name;
    int hash;

public:
    GChunk* val;

    GObjectFunction(GChunk* c = Gavel::newChunk(), int a = 0, int up = 0, std::string n = "_MAIN"):
        val(c), expectedArgs(a), upvalues(up), name(n) {
        type = GOBJECT_FUNCTION;
        hash = std::hash<GObjType>()(type) ^ std::hash<GChunk*>()(val);
    }

    virtual ~GObjectFunction() {
        Gavel::freeChunk(val); // clean up chunk
    }

    std::string toString() {
        return "<Func> " + name;
    }

    std::string toStringDataType() {
        return "[FUNCTION]";
    }

    GObject* clone() {
        return new GObjectFunction(val, expectedArgs, upvalues, name);
    }

    int getHash() {
        // make a hash specific for the type and the chunk
        return hash;
    }

    int getArgs() {
        return expectedArgs;
    }

    void setArgs(int ea) {
        expectedArgs = ea;
    }

    int getUpvalueCount() {
        return upvalues;
    }

    void setUpvalueCount(int uv) {
        upvalues = uv;
    }

    std::string getName() {
        return name;
    }

    void setName(std::string n) {
        name = n;
    }
};

#define DISASSM_LEVEL '\t'

void GChunk::disassemble(int level = 0) {
    std::cout << std::string(level, DISASSM_LEVEL) << "=========[[Chunk Constants]]=========" << std::endl;
    for (int i = 0; i < constants.size(); i++) {
        GValue c = constants[i]; 
        std::cout << std::string(level, DISASSM_LEVEL) << std::setw(3) << std::left << i << std::setw(2) << "-" << std::setw(15) << c.toStringDataType() << std::setw(7) << std::left << ": " + c.toString() << std::endl;
        if (ISGVALUEFUNCTION(c)) {
            READGVALUEFUNCTION(c)->disassemble(level+1);
        }
    }
    std::cout << std::endl;

    std::cout << std::string(level, DISASSM_LEVEL) << "=========[[Chunk Disassembly]]=========" << std::endl;
    int currentLine = -1;
    for (int z  = 0; z < code.size(); z++) {
        INSTRUCTION i = code[z];
        OPCODE op = GET_OPCODE(i);

        std::cout << std::string(level, DISASSM_LEVEL) << std::setw(3) << std::left << z << std::setw(2) << "-" << std::setw(16) << getOpCodeName(op) << std::setw(7) << std::left;
        switch (GInstructionTypes[op]) {
            case OPTYPE_IAX: {
                std::cout << "Ax: " + std::to_string(GETARG_Ax(i)) << "| ";
                break;
            }
            case OPTYPE_CLOSURE: {
                int indx = GETARG_Ax(i);
                GObjectFunction* func = (GObjectFunction*)(constants[indx]).val.obj;

                std::cout << func->toString();
                // the upval types are encoded in the instruction chunk (i just reuse OP_GETBASE & OP_GETUPVAL because it's readable and they arleady exist)
                for (int x = 0; x < func->getUpvalueCount(); x++) {
                    z++;
                    i = code[z];
                    switch (GET_OPCODE(i)) {
                        case OP_GETUPVAL:
                            std::cout << std::endl << std::string(level+1, DISASSM_LEVEL) << std::setw(3) << x << "- upvalue[" << GETARG_Ax(i) << "]";
                            break;
                        case OP_GETBASE:
                            std::cout << std::endl << std::string(level+1, DISASSM_LEVEL) << std::setw(3) << x << "- local[" << GETARG_Ax(i) << "]";
                            break;
                        default:
                            break;
                    }
                }

                break;
            }
            case OPTYPE_I:
            default: 
                std::cout << " " << "| ";
                // does nothing for now
                break;
        }

        // adds some nice comments to some slight confusing opcodes :)
        switch (op) {
            // ip manipulation
            case OP_JMP:
            case OP_IFJMP:
            case OP_CNDJMP:
            case OP_CNDNOTJMP: {
                int offset = GETARG_Ax(i);
                std::cout << "Jumps to " << (offset+z+1);
                break;
            }
            case OP_JMPBACK: {
                int offset = -GETARG_Ax(i);
                std::cout << "Jumps to " << (offset+z+1);
                break;
            }
            // loads from constants
            case OP_LOADCONST: {
                int indx = GETARG_Ax(i);
                std::cout << constants[indx].toStringDataType() << ": " << constants[indx].toString();
                break;
            }
            // loads from identifiers
            case OP_DEFINEGLOBAL:
            case OP_GETGLOBAL:
            case OP_SETGLOBAL: {
                int indx = GETARG_Ax(i);
                std::cout << identifiers[indx]->toString();
            }
        }

        std::cout << std::endl;
    }
}

#undef DISASSM_LEVEL

class GObjectClosure : GObject {
private:
    int hash;
public:
    GObjectFunction* val; // function being wrapped
    GObjectUpvalue** upvalues;
    int upvalueCount;

    GObjectClosure(GObjectFunction* func):
        val(func) {
        type = GOBJECT_CLOSURE;
        hash = std::hash<GObjType>()(type) ^ std::hash<GObjectFunction*>()(val);
        upvalueCount = val->getUpvalueCount();
        upvalues = new GObjectUpvalue*[upvalueCount];

        for (int i = 0; i < upvalueCount; i++) {
            upvalues[i] = NULL;
        }
    }

    virtual ~GObjectClosure() {
        delete[] upvalues;
    }

    std::string toString() {
        return "<Closure> " + std::to_string((intptr_t)val);
    }

    std::string toStringDataType() {
        return "[CLOSURE]";
    }

    GObject* clone() {
        return new GObjectClosure(val);
    }

    int getHash() {
        // make a hash specific for the type and the GObjectFunction
        return hash;
    }
};

struct GCallFrame {
    GObjectClosure* closure; // current function we're in
    INSTRUCTION* pc; // current pc
    GValue* basePointer; // our base in the stack to offset for locals and temps
};

/* GStack
    Stack for GState. I would've just used std::stack, but it annoyingly hides the container from us in it's protected members :/
*/
class GStack {
private:
    GValue container[STACK_MAX];
    GValue* top;

    GCallFrame callStack[CALLS_MAX];
    GCallFrame* currentCall;
public:
    GStack() {
        top = container;
        currentCall = callStack;
    }

    int push(GValue v) {
        *(top++) = v;
        return top - container; // returns the top index
    }

    GValue pop(int i = 1) {
        top -= i;
        return *(top); // returns old top value
    }

    inline GCallFrame* getCallStackStart() {
        return callStack;
    }

    inline GCallFrame* getCallStackEnd() {
        return currentCall;
    }

    inline GCallFrame* getFrame() {
        return (currentCall - 1);
    }

    inline int getCallCount() {
        return currentCall - callStack;
    }

    inline GValue* getStackStart() {
        return container;
    }

    inline GValue* getStackEnd() {
        return top;
    }

    GValue getBase(int i) {
        return getFrame()->basePointer[i];
    }

    void setBase(int i, GValue val) {
        getFrame()->basePointer[i] = val;
    }

    /* pushFrame()
        This pushes a frame to our callstack, with the given function, and offset in stack for the basePointer
    */
    bool pushFrame(GObjectClosure* closure, int a) {
        if (getCallCount() >= CALLS_MAX) {
            return false;
        }

        *(currentCall++) = {closure, &closure->val->val->code[0], (top - a - 1)};
        return true;
    }

    GCallFrame popFrame() {
        GCallFrame previousCall = *(--currentCall);
        top = previousCall.basePointer; // easy way to pop
        return previousCall;
    }

    GValue getTop(int i) {
        return *(top - (i+1)); // returns offset of stack
    }

    void setTop(int i, GValue v) {
        *(top - (i+1)) = v;
    }

    void resetStack() {
        top = container;
        currentCall = callStack;
    }

    void printStack() {
        std::cout << "===[[StackDump]]==" << std::endl;
        for (GValue* slot = top-1; slot >= container; slot--) {
            std::cout << std::setw(4) << slot->toStringDataType() << std::setw(20) << slot->toString() << std::endl;
        }
    }
};

typedef enum {
    GSTATE_OK,
    GSTATE_RUNTIME_OBJECTION,
    GSTATE_COMPILER_OBJECTION
} GStateStatus;

#define BINARY_OP(op) { \
    GValue num1 = stack.pop(); \
    GValue num2 = stack.pop(); \
    if (num1.type != GAVEL_TNUMBER || num2.type != GAVEL_TNUMBER) { \
        throwObjection("Cannot perform arithmetic on " + num1.toStringDataType() + " and " + num2.toStringDataType()); \
        break; \
    } \
    stack.push(GValue(num2.val.number op num1.val.number)); \
}


/* GState 
    This holds the stack, globals, debug info, and is in charge of executing states
*/
class GState {
private:
    GTable<GObjectString*> globals;
    GObjectUpvalue* openUpvalueList = NULL; // tracks our closed upvalues
    GStateStatus status = GSTATE_OK;

    // determins falsey-ness
    static bool isFalsey(GValue v) {
        return ISGVALUENIL(v) || (ISGVALUEBOOL(v) && !READGVALUEBOOL(v));
    }

    void closeUpvalues(GValue* last) {
        // for each open Upvalue "close" it onto the heap
        while (openUpvalueList != NULL && openUpvalueList->val >= last) {
            GObjectUpvalue* upval = openUpvalueList;
            upval->closed = *upval->val; // copy upvalue to closed GValue
            upval->val = &upval->closed; // update refernce to itself
            openUpvalueList = upval->nextUpval; // update list
        }
    }
    
    GStateStatus callValueFunction(GObjectClosure* closure, int args) {
        GObjectFunction* func = closure->val;

        if (args != func->getArgs()) {
            throwObjection("Function expected " + std::to_string(func->getArgs()) + " args!");
            return GSTATE_RUNTIME_OBJECTION;
        }

        if (!stack.pushFrame(closure, args)) { // callstack Overflow !
            throwObjection("PANIC! CallStack Overflow!");
            return GSTATE_RUNTIME_OBJECTION;
        }
        
        // starts the chunk
        GStateStatus stat = run();
        DEBUGLOG(std::cout << "CHUNK RETURNED!" << std::endl);

        if (stat == GSTATE_OK) {
            DEBUGLOG(std::cout << "fixing return value, frame, and call... " << std::endl);
            GValue retResult = stack.pop();
            closeUpvalues(stack.getFrame()->basePointer); // closes parameters (if they are upvalues)
            stack.popFrame(); // pops call
            stack.push(retResult);
            DEBUGLOG(std::cout << "done" << std::endl);
        }
        
        DEBUGLOG(std::cout << "coninuing execution" << std::endl);
        return stat;
    }

public:
    GState* next = NULL; // internal gc use
    GStack stack;
    GState() {}

    void markRoots() {
        // marks values on the stack (locals and temporaries)
        GValue* stackTop = stack.getStackEnd();
        for (GValue* indx = stack.getStackStart(); indx < stackTop; indx++) {
            Gavel::markValue(*indx);
        }

        // mark closures
        GCallFrame* endCallFrame = stack.getCallStackEnd();
        for (GCallFrame* indx = stack.getCallStackStart(); indx > endCallFrame; indx++) {
            Gavel::markObject((GObject*)indx->closure);
        }

        // mark all closed upvalues (it's okay if we re-mark something, like an upvalue pointing to a stack index)
        for (GObjectUpvalue* upval = openUpvalueList; upval != NULL; upval = upval->nextUpval) {
            Gavel::markObject((GObject*)upval);
        }

        // marks globals
        Gavel::markTable<GObjectString*>(&globals);
    }

    GObjectUpvalue* captureUpvalue(GValue* v) {
        // traverse open upvalue tree
        GObjectUpvalue* prevUpval = NULL;
        GObjectUpvalue* currentUpval = openUpvalueList;

        while (currentUpval != NULL && currentUpval->val > v) {
            prevUpval = currentUpval;
            currentUpval = currentUpval->nextUpval;
        }

        if (currentUpval != NULL && currentUpval->val == v)
            return currentUpval;

        GObjectUpvalue* temp = new GObjectUpvalue(v);
        Gavel::addGarbage((GObject*)temp);
        temp->nextUpval = currentUpval;

        if (prevUpval == NULL) {
            openUpvalueList = temp;
        } else {
            prevUpval->nextUpval = temp;
        }

        return temp;
    }

    void printGlobals() {
        std::cout << "----[[GLOBALS]]----" << std::endl;
        globals.printTable();
    }

    template <typename T>
    void setGlobal(std::string id, T val) {
        GValue newVal = Gavel::newGValue(val);
        GObjectString* str = Gavel::addString(id); // lookup string
        globals.setIndex(str, newVal);
    }

    void throwObjection(std::string err) {
        GCallFrame* frame = stack.getFrame();
        GChunk* currentChunk = frame->closure->val->val; // gets our currently-executing chunk
        status = GSTATE_RUNTIME_OBJECTION;

        GValue obj = CREATECONST_OBJECTION(GObjection(err, currentChunk->lineInfo[frame->pc - &currentChunk->code[0]]));
        Gavel::addGarbage(obj.val.obj);
        
        stack.push(obj);
    }

    GObjection getObjection() {
        GValue obj = stack.getTop(0);
        if (ISGVALUEOBJ(obj) && ISGVALUEOBJTYPE(obj, GOBJECT_OBJECTION)) {
            return READGVALUEOBJECTION(obj);
        } else {
            return GObjection(); // empty gobjection
        }
    }

    GStateStatus start(GObjectFunction* main) {
        // clean stack and everything
        resetState();
        GObjectClosure* closure = new GObjectClosure(main);
        Gavel::addGarbage((GObject*)closure);
        stack.push(GValue((GObject*)closure)); // pushes closure to the stack
        return callValueFunction(closure, 0);
    }

    // resets stack, callstack and triggers a garbage collection. globals are NOT cleared
    void resetState() {
        status = GSTATE_OK;
        stack.resetStack();
    }

    /* call(args)
        Looks at stack[top-args], and if it is callable, call it.
    */
    GStateStatus call(int args) {
        GValue val = stack.getTop(args);

        if (!ISGVALUEOBJ(val)) {
            throwObjection(val.toStringDataType() + " is not a callable type!");
            return GSTATE_RUNTIME_OBJECTION;
        }
            
        switch (val.val.obj->type) {
            case GOBJECT_CLOSURE: {
                // call chunk
                if (callValueFunction(reinterpret_cast<GObjectClosure*>(val.val.obj), args) != GSTATE_OK) {
                    return GSTATE_RUNTIME_OBJECTION;
                }
                break;
            }
            case GOBJECT_BOUNDCALL: { // c function bound to a prototable!
                GObjectBoundCall* bCall = reinterpret_cast<GObjectBoundCall*>(val.val.obj);
                // the prototable the call belongs too will always be first on the stack
                stack.push(GValue((GObject*)bCall->tbl));
                args++;

                GValue rtnVal = bCall->var(this, args);

                // pop the passed arguments & function (+1)
                stack.pop(args + 1);

                // push the return value
                stack.push(rtnVal);
                break;
            }
            case GOBJECT_FUNCTION:{
                // craft a closure and then call callValueFunction
                GObjectClosure* cls = new GObjectClosure((GObjectFunction*)val.val.obj);
                Gavel::addGarbage((GObject*)cls);
                if (callValueFunction(cls, args) != GSTATE_OK) {
                    return GSTATE_RUNTIME_OBJECTION;
                }
                break;
            }
            case GOBJECT_CFUNCTION: {
                // call c function
                GAVELCFUNC func = READGVALUECFUNCTION(val);
                GValue rtnVal = func(this, args);

                // pop the passed arguments & function (+1)
                stack.pop(args + 1);

                // push the return value
                stack.push(rtnVal);
                break;
            }
            default: 
                throwObjection(val.toStringDataType() + " is not a callable type!");
                return GSTATE_RUNTIME_OBJECTION;
        }

        return GSTATE_OK;
    }

    GStateStatus run() {
        GCallFrame* frame = stack.getFrame();
        GChunk* currentChunk = frame->closure->val->val; // sets currentChunk to our currently-executing chunk
        bool chunkEnd = false;
        
        while (!chunkEnd && status == GSTATE_OK) 
        {
            INSTRUCTION inst = *(frame->pc)++; // gets current executing instruction and increment
            DEBUGLOG(std::cout << "OP: " << GET_OPCODE(inst) << std::endl);
            switch (GET_OPCODE(inst))
            {   
                case OP_LOADCONST: { // iAx -- loads chunk->consts[Ax] onto the stack
                    DEBUGLOG(std::cout << "loading constant " << currentChunk->constants[GETARG_Ax(inst)].toString() << std::endl);
                    stack.push(currentChunk->constants[GETARG_Ax(inst)]);
                    break;
                }
                case OP_DEFINEGLOBAL: {
                    GValue newVal = stack.pop();
                    GObjectString* id = currentChunk->identifiers[GETARG_Ax(inst)];
                    DEBUGLOG(std::cout << "defining '" << id->toString() << "' to " << newVal.toString() << std::endl);
                    if (globals.checkSetIndex(id, newVal)) { // sets global
                        // global already existed... oops
                        throwObjection("'" + id->toString() + "' already exists!");
                    }
                    break;
                }
                case OP_GETGLOBAL: {
                    GObjectString* id = currentChunk->identifiers[GETARG_Ax(inst)];
                    stack.push(globals.getIndex(id));
                    break;
                }
                case OP_SETGLOBAL: {
                    GValue newVal = stack.pop();
                    GObjectString* id = currentChunk->identifiers[GETARG_Ax(inst)];
                    // if global didn't exist, throw objection!
                    if (!globals.checkSetIndex(id, newVal)) {
                        throwObjection("'" + id->toString() + "' does not exist!");
                    }
                    break;
                }
                case OP_GETBASE: {
                    int indx = GETARG_Ax(inst);
                    GValue local = stack.getBase(indx);
                    DEBUGLOG(stack.printStack());
                    DEBUGLOG(std::cout << "getting local at stack[base-" << indx << "] : " << local.toString() << std::endl);
                    stack.push(local);
                    break;
                }
                case OP_SETBASE: {
                    int indx = GETARG_Ax(inst);
                    GValue val = stack.pop(); // gets value off of stack
                    DEBUGLOG(std::cout << "setting local at stack[base-" << indx << "] to " << val.toString() << std::endl);
                    stack.setBase(indx, val);
                    break;
                }
                case OP_GETUPVAL: {
                    int indx = GETARG_Ax(inst);
                    DEBUGLOG(std::cout << "grabbing upvalue[" << indx << "] " << (frame->closure->upvalues[indx]->val)->toString() << std::endl);
                    stack.push(*frame->closure->upvalues[indx]->val);
                    break;
                }
                case OP_SETUPVAL: {
                    int indx = GETARG_Ax(inst);
                    *frame->closure->upvalues[indx]->val = stack.pop();
                    break;
                }
                case OP_CLOSURE: {
                    // grabs function from constants
                    GObjectFunction* func = (GObjectFunction*)(currentChunk->constants[GETARG_Ax(inst)]).val.obj;
                    // creates new closure
                    GObjectClosure* closure = new GObjectClosure(func);
                    stack.push(GValue((GObject*)closure));
                    Gavel::addGarbage((GObject*)closure);

                    // grab upvals/locals
                    for (int i = 0; i < closure->upvalueCount; i++) {
                        inst = *(frame->pc)++;
                        int index = GETARG_Ax(inst);
                        switch(GET_OPCODE(inst)) {
                            case OP_GETUPVAL: { // it's an upvalue of our current closure
                                DEBUGLOG(std::cout << "creating linked upval to " << frame->closure->upvalues[index]->val->toString() << std::endl);
                                closure->upvalues[i] = frame->closure->upvalues[index];
                                break;
                            }
                            case OP_GETBASE: { // it's a local from this frame
                                DEBUGLOG(std::cout << "creating local upval to " << (frame->basePointer + index)->toString() << std::endl);
                                closure->upvalues[i] = captureUpvalue(frame->basePointer + index);
                                break;
                            }
                            default:
                                throwObjection("OPCODE ERR. [" + std::to_string(GET_OPCODE(inst)) + "]");
                                return GSTATE_RUNTIME_OBJECTION;
                        }
                    }
                    Gavel::checkGarbage();
                    break;
                }
                case OP_CLOSE: { // iAx - Closes local at stack[base-Ax] to the heap, doesn't pop however.
                    int localIndx = GETARG_Ax(inst);
                    closeUpvalues(frame->basePointer + localIndx);
                    break;
                }
                case OP_POP: {
                    int ax = GETARG_Ax(inst);
                    DEBUGLOG(stack.printStack());
                    DEBUGLOG(std::cout << "popping stack[top] " << ax << " times" << std::endl);
                    stack.pop(ax); // pops whatever is on the stack * Ax
                    break;
                }
                case OP_IFJMP: { // if stack.pop() == false, jmp
                    int offset = GETARG_Ax(inst);
                    GValue val = stack.pop(); // NOTE: *DOES* pop the value 
                    if (isFalsey(val)) {
                        DEBUGLOG(std::cout << "stack[top] is false, JMPing by " << offset << " instructions" << std::endl);
                        frame->pc += offset; // perform the jump
                    }
                    break;
                }
                case OP_CNDNOTJMP: { // if stack[top] == false, jmp
                    int offset = GETARG_Ax(inst);
                    GValue val = stack.getTop(0); // NOTE: does *NOT POP THE VALUE!* 
                    if (isFalsey(val)) {
                        DEBUGLOG(std::cout << "stack[top] is false, JMPing by " << offset << " instructions" << std::endl);
                        frame->pc += offset; // perform the jump
                    }
                    break;
                }
                case OP_CNDJMP: {
                    int offset = GETARG_Ax(inst);
                    GValue val = stack.getTop(0); // NOTE: does *NOT POP THE VALUE!* 
                    if (!isFalsey(val)) {
                        DEBUGLOG(std::cout << "stack[top] is true, JMPing by " << offset << " instructions" << std::endl);
                        frame->pc += offset; // perform the jump
                    }
                    break;
                }
                case OP_JMP: {
                    int offset = GETARG_Ax(inst);
                    DEBUGLOG(std::cout << "JMPing by " << offset << " instructions" << std::endl);
                    frame->pc += offset; // perform the jump
                    break;
                }
                case OP_JMPBACK: {
                    int offset = -GETARG_Ax(inst);
                    DEBUGLOG(std::cout << "JMPing by " << offset << " instructions" << std::endl);
                    frame->pc += offset; // perform the jump
                    break;
                }
                case OP_CALL: {
                    int args = GETARG_Ax(inst);
                    call(args);
                    Gavel::checkGarbage();
                    break;
                }
                case OP_INDEX: {
                    GValue indx = stack.pop(); // stack[top]
                    GValue tbl = stack.pop(); // stack[top-1]

                    if (ISGVALUEBASETABLE(tbl)) {
                        stack.push(reinterpret_cast<GObjectTableBase*>(tbl.val.obj)->getIndex(indx));
                    } else {
                        throwObjection("Cannot index non-table value " + tbl.toStringDataType());
                    }
                    break;
                }
                case OP_NEWINDEX: {
                    GValue newVal = stack.pop(); // stack[top]
                    GValue indx = stack.pop(); // stack[top-1]
                    GValue tbl = stack.pop(); // stack[top-2]

                    if (ISGVALUEBASETABLE(tbl)) {
                        reinterpret_cast<GObjectTableBase*>(tbl.val.obj)->setIndex(indx, newVal);
                    } else {
                        throwObjection("Cannot index non-table value " + tbl.toStringDataType());
                    }
                    break;
                }
                case OP_EQUAL: {
                    GValue n1 = stack.pop();
                    GValue n2 = stack.pop();
                    stack.push(n1.equals(n2)); // push result
                    break;
                }
                case OP_LESS:       { BINARY_OP(<); break; }
                case OP_GREATER:    { BINARY_OP(>); break; }
                case OP_NEGATE: {
                    GValue val = stack.pop();
                    if (val.type != GAVEL_TNUMBER){
                        throwObjection("Cannot negate non-number value " + val.toStringDataType() + "!");
                        break;
                    }
                    stack.push(CREATECONST_NUMBER(-val.val.number));
                    break;
                }
                case OP_NOT: {
                    stack.push(isFalsey(stack.pop()));
                    break;
                }
                case OP_ADD: { 
                    int Indx = GETARG_Axs(inst);
                    GValue n1 = stack.pop();
                    GValue n2 = stack.pop();
                    GValue newVal;
                    if (ISGVALUESTRING(n2) || ISGVALUESTRING(n1)) {
                        // concatinate the strings
                        Gavel::checkGarbage();
                        newVal = GValue((GObject*)Gavel::addString(n2.toString() + n1.toString()));
                    } else if (ISGVALUENUMBER(n1) && ISGVALUENUMBER(n2)) {
                        // pushes to the stack
                        newVal = CREATECONST_NUMBER(READGVALUENUMBER(n1) + READGVALUENUMBER(n2));
                    } else {
                        throwObjection("Cannot perform arithmetic on " + n1.toStringDataType() + " and " + n2.toStringDataType());
                        break;
                    }
                    stack.push(newVal);
                    break;
                }
                case OP_SUB:    { BINARY_OP(-); break; }
                case OP_MUL:    { BINARY_OP(*); break; }
                case OP_DIV:    { BINARY_OP(/); break; }
                case OP_INC: {
                    int type = GETARG_Ax(inst);
                    GValue num = stack.pop();
                    if (!ISGVALUENUMBER(num)) {
                        throwObjection("Cannot increment on " + num.toStringDataType());
                        break;
                    }

                    DEBUGLOG(std::cout << "incrementing " << num.toString());

                    // first push the value that will be left on the stack
                    stack.push(READGVALUENUMBER(num) + (type == 1 ? 0 : 1));
                    // then push the value that will be assigned
                    stack.push(READGVALUENUMBER(num) + 1);
                    break;
                }
                case OP_DEC: {
                    int type = GETARG_Ax(inst);
                    GValue num = stack.pop();
                    if (!ISGVALUENUMBER(num)) {
                        throwObjection("Cannot decrement on " + num.toStringDataType());
                        break;
                    }

                    // first push the value that will be left on the stack
                    stack.push(READGVALUENUMBER(num) - (type == 1 ? 0 : 1));
                    // then push the value that will be assigned
                    stack.push(READGVALUENUMBER(num) - 1);
                    break;
                }
                case OP_TRUE: {
                    DEBUGLOG(std::cout << "pushing true to the stack" << std::endl);
                    stack.push(CREATECONST_BOOL(true));
                    break;
                }
                case OP_FALSE: {
                    DEBUGLOG(std::cout << "pushing false to the stack" << std::endl);
                    stack.push(CREATECONST_BOOL(false));
                    break;
                }
                case OP_NIL: {
                    DEBUGLOG(std::cout << "pushing nil to the stack" << std::endl);
                    stack.push(CREATECONST_NIL());
                    break;
                }
                case OP_NEWTABLE: {
                    DEBUGLOG(std::cout << "pushing new table to the stack" << std::endl);
                    
                    int pairs = GETARG_Ax(inst);
                    GValue tbl = CREATECONST_TABLE();

                    for (int i = 0; i < pairs; i++) {
                        // grab pair from the stack
                        GValue val = stack.pop();
                        GValue indx = stack.pop();

                        // set the key/value pair in the table
                        READGVALUETABLE(tbl).setIndex(indx, val);
                    }

                    Gavel::addGarbage(reinterpret_cast<GObject*>(tbl.val.obj));
                    stack.push(tbl);
                    break;
                }
                case OP_RETURN: { // i
                    return GSTATE_OK;
                }
                default:
                    throwObjection("INVALID OPCODE: " + std::to_string(GET_OPCODE(inst)));
                    break;
            }
        }
        return status;
    }
};

#undef BINARY_OP

namespace Gavel {
    GTable<GObjectString*> strings;
    std::vector<GObject*> greyObjects; // objects that are marked grey
    GObject* objList = NULL; // another linked list to track our allocated objects on the heap
    GState* states = NULL;
    GChunk* chunks = NULL;
    size_t bytesAllocated = 0;
    size_t nextGc = GC_INITALMEMORYTHRESH;
#ifdef GSTRING_INTERN
    size_t stringThreshGc = GC_INITIALSTRINGSTHRESH;
#endif

    void checkGarbage() {
#ifdef GSTRING_INTERN
        // if strings are bigger than our threshhold, clean them up. 
        if (strings.getSize() > stringThreshGc) {
            collectGarbage();
            // if most of them are still being used, let more for next time
            if (strings.getSize()*2 > stringThreshGc) {
                stringThreshGc =  stringThreshGc + strings.getSize();
            }
        }
#endif

        if (bytesAllocated > nextGc) {
            collectGarbage(); // collect the garbage
            DEBUGGC(std::cout << "New bytesAllocated: " << bytesAllocated << std::endl);
            if (bytesAllocated * 2 > nextGc) {
                nextGc += bytesAllocated;
            }
        }
    }

    // a very-small amount of string interning is done, however the GTable implementation still uses ->equals(). This will help in the future when string interning becomes a priority
    GObjectString* addString(std::string str) {
        GObjectString* newStr = new GObjectString(str);
#ifdef GSTRING_INTERN
        GObjectString* key = (GObjectString*)strings.findExistingKey(newStr);
        if (key == NULL) {
            strings.setIndex(newStr, CREATECONST_NIL());
            addGarbage(newStr); // add it to our GC AFTER so we don't make out gc clean it up by accident :sob:
            return newStr;
        }

        delete newStr;
        return key;
#else
        addGarbage(newStr);
        return newStr;
#endif
    }

    // creates a new GState and adds to the states linked list
    GState* newState() {
        GState* st = new GState();

        if (states != NULL) {
            st->next = states;
        }

        states = st;
        return st;
    }

    // creates a new GChunk and adds to the chunk linked list
    GChunk* newChunk() {
        GChunk* ch = new GChunk();

        if (chunks != NULL) {
            ch->next = chunks;
        }

        chunks = ch;
        return ch;
    }

    void freeChunk(GChunk* ch) {
        GChunk* currentChunk = chunks;
        GChunk* prev = NULL;

        while (currentChunk != ch && currentChunk != NULL) {
            prev = currentChunk;
            currentChunk = currentChunk->next;
        }

        if (currentChunk == NULL)
            return;

        // unlink it from the list!
        if (prev != NULL)
            prev->next = currentChunk->next;
        else
            chunks = NULL;
        
        // finally, delete the chunk!
        delete ch;
    }

    void freeState(GState* st) {
        GState* currentState = states;
        GState* prev = NULL;

        while (currentState != st && currentState != NULL) {
            prev = currentState;
            currentState = currentState->next;
        }

        if (currentState == NULL)
            return;

        // unlink it from the list!
        if (prev != NULL)
            prev->next = currentState->next;
        else
            states = NULL;

        // finally, delete the state!
        delete st;

        // now throw away everything that state was hoarding!
        Gavel::collectGarbage();
    }

// =============================================================[[GARBAGE COLLECTION]]=============================================================

    void markObject(GObject* o) {
        if (o == NULL || o->isGray) // make sure it exists & we havent marked it yet
            return;

        DEBUGGC(std::cout << "marking " << o->toString() << std::endl);
        
        // mark grey and keep track of it
        o->isGray = true;
        greyObjects.push_back(o);
    }
    
    void markValue(GValue val) {
        // if it's an object, mark it
        if (ISGVALUEOBJ((val)))
            markObject((GObject*)val.val.obj);
    }

    template <typename T>
    void markTable(GTable<T>* tbl) {
        for (auto pair : tbl->hashTable) {
            if constexpr (std::is_same<T, GObjectString*>() || std::is_same<T, GObject*>()) {
                if (pair.first.key != NULL) // skip null refs
                    markObject((GObject*)pair.first.key);
            } else if constexpr (std::is_same<T, GValue>()) {
                markValue(pair.first.key);
            }
            markValue(pair.second);
        }
    }

    template <typename T>
    void removeWhiteTable(GTable<T>* tbl) {
        auto it = tbl->hashTable.begin();
        while (it != tbl->hashTable.end()) {

            // remove keys that are white
            if (it->first.key != NULL && !it->first.key->isGray) {
                it = tbl->hashTable.erase(it);
            } else {
                it++;
            }
        }
    }

    void markArray(std::vector<GValue>* arr) {
        for (GValue val : *arr) {
            markValue(val);
        }
    }

    void blackenObject(GObject* obj) {
        DEBUGGC(std::cout << "blackening " << obj->toStringDataType() << " : " << obj->toString() << std::endl);

        switch (obj->type) {
            case GOBJECT_UPVAL: {
                // mark the upvalue, (the pointer, not the closed one. if we're no longer pointing to that we shouldn't mark it as being used!)
                markValue(*reinterpret_cast<GObjectUpvalue*>(obj)->val);
                break;
            }
            case GOBJECT_FUNCTION: {
                // mark constant
                markArray(&reinterpret_cast<GObjectFunction*>(obj)->val->constants);
                // mark the identifiers (regretting not just using strings now :eyes:)
                for (GObjectString* str :reinterpret_cast<GObjectFunction*>(obj)->val->identifiers) {
                    markObject((GObject*)str);
                }
                break;
            }
            case GOBJECT_CLOSURE: {
                GObjectClosure* closure = reinterpret_cast<GObjectClosure*>(obj);
                // mark the function object
                markObject((GObject*)closure->val);
                // traverse upvalues, mark them
                for (int i = 0; i < closure->upvalueCount; i++) {
                    markObject((GObject*)closure->upvalues[i]);
                }
                break;
            }
            case GOBJECT_BOUNDCALL: {
                GObjectBoundCall* boundCall = reinterpret_cast<GObjectBoundCall*>(obj);
                markObject((GObject*)boundCall->tbl);
                break;
            }
            case GOBJECT_TABLE: {
                GObjectTable* tblObj = reinterpret_cast<GObjectTable*>(obj);
                markTable<GValue>(&tblObj->val);
                break;
            }
            case GOBJECT_PROTOTABLE: {
                GObjectPrototable* ptblObj = reinterpret_cast<GObjectPrototable*>(obj);
                for (auto pair : ptblObj->hashTable) {
                    // mark the keys
                    markValue(pair.first.key);
                    pair.second->mark(); // GProto's can hold GObjects (like GProtoCFunction)
                }
                break;
            }
            default: // no defined behavior
                break;
        }
    }

    void traceReferences() {
        while (greyObjects.size() > 0) {
            int currentIndex = greyObjects.size() - 1;
            
            // this probably adds more to the greyObjects vector
            blackenObject(greyObjects[currentIndex]);

            // pop the right object!!!
            greyObjects.erase(greyObjects.begin() + currentIndex);
        }
        greyObjects.clear();
    }

    void markStates() {
        GState* state = states;

        while (state != NULL) {
            state->markRoots();
            state = state->next;
        }
    }

    void markChunks() {
        GChunk* chunk = chunks;

        while (chunk != NULL) {
            chunk->markRoots();
            chunk = chunk->next;
        }
    }

    void sweepUp() {
        GObject* object = objList;
        GObject* prev = NULL;

        // traverse the linked list of GObjects, removing all WHITE objects (not grey-marked objects)
        while (object != NULL) {
            if (object->isGray) {
                object->isGray = false; // unmark it to prepare for the next garbage collect
                prev = object;
                object = object->next; // goes to next linked object
            } else {
                DEBUGGC(std::cout << "freeing " << object->getSize() << " bytes [" << object->toStringDataType() << " : " << object->toString() << "]" << std::endl);
                // free memory and remove it from the linked list!!
                GObject* garb = object;
                bytesAllocated = bytesAllocated - object->getSize(); // sub our size

                object = object->next;
                if (prev != NULL) {
                    prev->next = object;
                } else {
                    objList = object;
                }

                delete garb; // free's it using the virtual member's deconstructor. yay inheritance!
            }
        }
    }

    void collectGarbage() { 
        DEBUGGC(std::cout << "\033[1;31m---====[[ COLLECTING GARBAGE, AT " << bytesAllocated << " BYTES... CPUNCH INCLUDED! ]]====---\033[0m" << std::endl);  

        markStates();
        markChunks();
        traceReferences();
        removeWhiteTable<GObjectString*>(&strings);
        sweepUp();
    }

    void addGarbage(GObject* g) { // for values generated dynamically, add it to our garbage to be marked & sweeped up!
        // track memory
        bytesAllocated += g->getSize();

        if (objList != NULL) {
            // append g to the objList
            g->next = objList;
        }
        objList = g;
    }

    /* newGValue(<t> value) - Helpful function to auto-turn some basic datatypes into a GValue for ease of embeddability
        - value : value to turn into a GValue
        returns : GValue
    */
    template <typename T>
    inline GValue newGValue(T x) {
        if constexpr (std::is_same<T, double>() || std::is_same<T, float>() || std::is_same<T, int>())
            return CREATECONST_NUMBER(x);
        else if constexpr (std::is_same<T, bool>())
            return CREATECONST_BOOL(x);
        else if constexpr (std::is_same<T, char*>() || std::is_same<T, const char*>() || std::is_same<T, std::string>()) {
            GObjectString* obj = addString(x);
            return GValue((GObject*)obj);
        } else if constexpr (std::is_same<T, GAVELCFUNC>()) {
            GValue temp = CREATECONST_CFUNCTION(x);
            addGarbage((GObject*)temp.val.obj);
            return temp;
        } else if constexpr ( std::is_same<T, GObjectFunction*>()) {
            addGarbage((GObject*)x);
            // convert to closure
            GObjectClosure* cls = new GObjectClosure(x);
            addGarbage((GObject*)cls);
            return GValue((GObject*)cls);
        } else if constexpr (std::is_same<T, GObject*>() || std::is_same<T, GObjectString*>() || std::is_same<T, GObjectTable*>() || std::is_same<T, GObjectPrototable*>() || std::is_same<T, GObjectCFunction*>() || std::is_same<T, GObjectClosure*>()) {
            addGarbage((GObject*)x);
            return GValue((GObject*)x);
        } else if constexpr (std::is_same<T, GValue>())
            return x;
            
        std::cout << "WANRING: Datatype cannot be guessed! GValue is nil!" << std::endl;
        return CREATECONST_NIL();
    }

}

// ===========================================================================[[ COMPILER/LEXER ]]===========================================================================

#ifndef EXCLUDE_COMPILER

typedef enum {
    // single character tokens
    TOKEN_MINUS, // -
    TOKEN_PLUS, // +
    TOKEN_STAR, // *
    TOKEN_SLASH, // /
    TOKEN_DOT, // .
    TOKEN_COMMA, // ,
    TOKEN_COLON, // :
    TOKEN_OPEN_PAREN, // (
    TOKEN_CLOSE_PAREN,  // )
    TOKEN_OPEN_BRACE,  // {
    TOKEN_CLOSE_BRACE, // }
    TOKEN_OPEN_BRACKET,  // [
    TOKEN_CLOSE_BRACKET, // ]

    // equality character tokens
    TOKEN_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_EQUAL_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_OR,
    TOKEN_AND,

    // unary ops
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,
    TOKEN_BANG,

    // variables/constants
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_HEXADEC,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NIL,

    // keywords
    TOKEN_END,
    TOKEN_DO,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_ELSEIF,
    TOKEN_WHILE,
    TOKEN_THEN,
    TOKEN_FOR,
    TOKEN_FUNCTION,
    TOKEN_RETURN,
    TOKEN_VAR,
    TOKEN_LOCAL,
    TOKEN_GLOBAL,
    
    TOKEN_EOS, // marks an end of statement i.e newline or ;
    TOKEN_EOF, // marks end of file
    TOKEN_ERROR // marks an error while tokenizing
} GTokenType;

typedef enum {                  
    PREC_NONE,                    
    PREC_ASSIGNMENT,  // =        
    PREC_OR,          // or       
    PREC_AND,         // and      
    PREC_EQUALITY,    // == !=    
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -      
    PREC_FACTOR,      // * /      
    PREC_UNARY,       // ! -      
    PREC_INDEX,       // []
    PREC_CALL,        // . ()
    PREC_PRIMARY                  
} Precedence;

typedef enum {
    PARSEFIX_NUMBER,
    PARSEFIX_STRING,
    PARSEFIX_BINARY,
    PARSEFIX_LITERAL,
    PARSEFIX_DEFVAR,
    PARSEFIX_VAR,
    PARSEFIX_UNARY,
    PARSEFIX_PREFIX,
    PARSEFIX_GROUPING,
    PARSEFIX_INDEX,
    PARSEFIX_LAMBDA,
    PARSEFIX_CALL,
    PARSEFIX_AND,
    PARSEFIX_OR,
    PARSEFIX_SKIP,
    PARSEFIX_ENDPARSE,
    PARSEFIX_NONE
} ParseFix;

struct ParseRule {
    ParseFix prefix;
    ParseFix infix;
    Precedence precedence;
    ParseRule(ParseFix p, ParseFix i, Precedence pc):
        prefix(p), infix(i), precedence(pc) {}
};

ParseRule GavelParserRules[] = {
    {PARSEFIX_UNARY,    PARSEFIX_BINARY,    PREC_TERM},     // TOKEN_MINUS
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_TERM},     // TOKEN_PLUS
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_FACTOR},   // TOKEN_STAR
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_FACTOR},   // TOKEN_SLASH
    {PARSEFIX_NONE,     PARSEFIX_INDEX,     PREC_INDEX},     // TOKEN_DOT
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_COMMA
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_COLON
    {PARSEFIX_GROUPING, PARSEFIX_CALL,      PREC_CALL},     // TOKEN_OPEN_PAREN 
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_CLOSE_PAREN 
    {PARSEFIX_LITERAL,  PARSEFIX_NONE,      PREC_NONE},     // TOKEN_OPEN_BRACE 
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_CLOSE_BRACE
    {PARSEFIX_NONE,     PARSEFIX_INDEX,     PREC_INDEX},     // TOKEN_OPEN_BRACKET 
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_CLOSE_BRACKET

    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_COMPARISON},// TOKEN_LESS
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_COMPARISON},// TOKEN_GREATER
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_EQUALITY}, // TOKEN_EQUAL_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_COMPARISON},// TOKEN_LESS_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_COMPARISON},// TOKEN_GREATER_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_EQUALITY}, // TOKEN_BANG_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_OR,        PREC_OR},       // TOKEN_OR
    {PARSEFIX_NONE,     PARSEFIX_AND,       PREC_AND},      // TOKEN_AND

    {PARSEFIX_PREFIX,   PARSEFIX_BINARY,    PREC_NONE},     // TOKEN_PLUS_PLUS (can be before or after an expression :])
    {PARSEFIX_PREFIX,   PARSEFIX_BINARY,    PREC_NONE},     // TOKEN_MINUS_MINUS (same here)
    {PARSEFIX_UNARY,    PARSEFIX_NONE,      PREC_NONE},     // TOKEN_BANG

    {PARSEFIX_VAR,      PARSEFIX_NONE,      PREC_NONE},     // TOKEN_IDENTIFIER
    {PARSEFIX_STRING,   PARSEFIX_NONE,      PREC_NONE},     // TOKEN_STRING
    {PARSEFIX_NUMBER,   PARSEFIX_NONE,      PREC_NONE},     // TOKEN_NUMBER
    {PARSEFIX_NUMBER,   PARSEFIX_NONE,      PREC_NONE},     // TOKEN_HEXADEC
    {PARSEFIX_LITERAL,  PARSEFIX_NONE,      PREC_NONE},     // TOKEN_TRUE
    {PARSEFIX_LITERAL,  PARSEFIX_NONE,      PREC_NONE},     // TOKEN_FALSE
    {PARSEFIX_LITERAL,  PARSEFIX_NONE,      PREC_NONE},     // TOKEN_NIL

    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_END
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_DO
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_IF
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_ELSE
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_ELSEIF
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_WHILE
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_THEN
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_FOR
    {PARSEFIX_LAMBDA,   PARSEFIX_LAMBDA,    PREC_NONE},     // TOKEN_FUNCTION
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_RETURN
    {PARSEFIX_DEFVAR,   PARSEFIX_NONE,      PREC_NONE},     // TOKEN_VAR
    {PARSEFIX_DEFVAR,   PARSEFIX_NONE,      PREC_NONE},     // TOKEN_LOCAL
    {PARSEFIX_DEFVAR,   PARSEFIX_NONE,      PREC_NONE},     // TOKEN_GLOBAL

    {PARSEFIX_SKIP,     PARSEFIX_SKIP,      PREC_NONE},     // TOKEN_EOS 
    {PARSEFIX_ENDPARSE, PARSEFIX_ENDPARSE,  PREC_NONE},     // TOKEN_EOF 
    {PARSEFIX_ENDPARSE, PARSEFIX_ENDPARSE,  PREC_NONE}      // TOKEN_ERROR 
};

typedef enum {
    PARSER_STATUS_OK,
    PARSER_STATUS_OBJECTION
} GParserStatus;

typedef enum {
    CHUNK_FUNCTION,
    CHUNK_SCRIPT
} ChunkType;

class GavelParser {
private:
    GObjectFunction* function = NULL;
    GavelParser* parent = NULL; // our enclosing function
    ChunkType chunkType;
    int args = 0;

    GObjection objection;

    const char* script;
    const char* currentChar;
    size_t scriptSize;

    bool panic = false;
    bool quitParse = false;
    int line = 0;
    int openBraces = 0;
    int pushedVals = 0;
    int pushedOffset = 0; // when entering a new expression, this is the ammount of pushed values we started with

    struct Token {
        GTokenType type;
        std::string str;

        Token() {}

        Token(GTokenType t):
            type(t) {}

        Token(GTokenType t, std::string s):
            type(t), str(s) {}

        Token(GTokenType t, char* s, size_t sz):
            type(t) {
            str = std::string(s, sz); // copies the char buffer array with size of sz to a std::string. hooray c++ features!
        }
    };

    struct Local {
        std::string name;
        int depth;
        bool isCaptured;
    };

    struct Upvalue {
        int index;
        bool isLocal; 
    }; 

    Local locals[MAX_LOCALS]; // holds our locals
    std::vector<Upvalue> upvalues; // holds our upvalues
    int localCount = 0;
    int scopeDepth = 0; // current scope depth

    // this holds our hashtable for a quick and easy lookup if we have a reserved word, and get it's corrosponding GTokenType
    std::map<std::string, GTokenType> reservedKeywords = {

        // control flow stuff
        {"if",      TOKEN_IF},
        {"then",    TOKEN_THEN},
        {"else",    TOKEN_ELSE},
        {"elseif",  TOKEN_ELSEIF},
        {"while",   TOKEN_WHILE},
        {"for",     TOKEN_FOR},
        {"do",      TOKEN_DO},
        {"end",     TOKEN_END},
        {"return",  TOKEN_RETURN},
        
        {"and",     TOKEN_AND},
        {"or",      TOKEN_OR},

        // literals
        {"true",    TOKEN_TRUE},
        {"false",   TOKEN_FALSE},
        {"nil",     TOKEN_NIL},

        // scope definitions
        {"var",     TOKEN_VAR},
        {"local",   TOKEN_LOCAL},
        {"global",  TOKEN_GLOBAL},

        {"function",TOKEN_FUNCTION}
    };

    void throwObjection(std::string e) {
        if (panic)
            return;

        DEBUGLOG(std::cout << "OBJECTION THROWN : " << e << std::endl);
        panic = true;
        objection = GObjection(e, line);
    }

    void setParent(GavelParser* p) {
        parent = p;
    }

// =================================================================== [[Scope handlers]] ====================================================================

    int findLocal(std::string id) {
        // search variables for a match with id
        for (int i = localCount - 1; i >= 0; i--) {
            if (locals[i].depth == -1) // it's not initialized yet!
                continue;
            
            // our locals are always going to be at the end of the array, because they grow.
            if (locals[i].name.compare(id) == 0) {
                return i;
            }
        }

        // didn't find it
        return -1;
    }

    int declareLocal(std::string id) {
        // check if we have space for the local
        if (localCount >= MAX_LOCALS) {
            throwObjection("Max locals reached!!");
            return -1;
        }

        DEBUGLOG(std::cout << "LOCAL VAR : " << id << std::endl);


        locals[localCount] = {id, -1, false}; // adds new local in an "uninitalized" state
        return localCount++;
    }

    int addUpvalue(int in, bool isLocal) {
        // find pre-existing upvalue
        for (int i  = 0; i < upvalues.size(); i++) {
            if (upvalues[i].index == in && upvalues[i].isLocal == isLocal) 
                return i;
        }

        upvalues.push_back({in, isLocal});
        return upvalues.size() - 1;
    }

    int findUpval(std::string id) {
        if (parent == NULL)
            return -1; // don't even check parent, it doesn't exist

        DEBUGLOG(std::cout << "resolving local '" << id << "'" << std::endl); 
        int localIndx = parent->findLocal(id);
        if (localIndx != -1) {
            parent->locals[localIndx].isCaptured = true; // mark the local as a captured upvalue
            return addUpvalue(localIndx, true);
        }

        DEBUGLOG(std::cout << "resolving upval '" << id << "'" << std::endl); 
        // resolve uovalue
        int upval = parent->findUpval(id);
        if (upval != -1) {
            // it found an upvalue in the parent function that refernces our value!
            return addUpvalue(upval, false);
        }

        return -1;
    }

    void markLocalInitalized() {
        locals[localCount - 1].depth = scopeDepth;
    }

    void beginScope() {
        DEBUGLOG(std::cout << "---NEW SCOPE" << std::endl);
        scopeDepth++; // increment local scope!
    }

    void endScope() {
        DEBUGLOG(std::cout << "---END SCOPE" << std::endl);
        scopeDepth--; // decrement local scope!
        int localsToPop = 0;
        while (localCount > 0 && locals[localCount - 1].depth > scopeDepth) {
            if (locals[localCount - 1].isCaptured) { // close the upvalue
                emitInstruction(CREATE_iAx(OP_CLOSE, localsToPop)); // we tell the vm to close this local at base - localsToPop
            }
            localsToPop++;
            localCount--;
        }

        // pops the locals :)
        if (localsToPop > 0)
            emitInstruction(CREATE_iAx(OP_POP, localsToPop));
    }

// =================================================================== [[Functions for tokenizing]] ====================================================================

    // if we've parsed the whole script
    inline bool isEnd() {
        return (currentChar - script) >= scriptSize || panic;
    }

    // increments currentChar and returns the character
    char advanceChar() {
        currentChar++;
        return currentChar[-1]; // gets the charcter right behind it
    }

    // peeks current character
    char peekChar() {
        return *currentChar;
    }

    char peekNextChar() {
        if (!isEnd()) // if there's still more source to parse, return the next char
            return currentChar[1];
        return '\0'; // otherwise return a null terminator cuz /shrug
    }

    // check the next character to what's expected, if it's not it return false. if it is, increment currentChar and return true
    bool matchChar(char expected) {
        if (isEnd() || *currentChar != expected)
            return false;
        
        currentChar++;
        return true;
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
                return true;
            default: // not a numeric
                return false;
        }
    }

    static bool isAlpha(char c) {
        return isalpha(c) || c == '_';
    }

    Token checkReserved(std::string& word) {
        if (reservedKeywords.find(word) != reservedKeywords.end()) // whoops it's a reserved word
            return Token(reservedKeywords[word]);
        
        // else it's just an identifier!
        return Token(TOKEN_IDENTIFIER, word);
    }

    Token readString() {
        std::string str;

        while (peekChar() != '"' && !isEnd()) {
            // check if we're encoding something into the string
            if (peekChar() == '\\') {
                advanceChar();
                if (isEnd())
                    return Token(TOKEN_ERROR, "Unterminated string!");
                
                switch (peekChar()) {
                    case 'n': // new line
                        str += '\n';
                        break;
                    case 't': // tab
                        str += '\t';
                        break;
                    case '\\': // wants to use '\'
                        str += '\\';
                        break;
                    case '"': // wants to include a "
                        str += '"';
                        break;
                    default: { 
                        if (isNumeric(peekChar())) {
                            // read byte
                            std::string num;
                            while (isNumeric(peekChar()) && !isEnd()) 
                                num += advanceChar();
                            int i = atoi(num.c_str());

                            if (i > 255)
                                return Token(TOKEN_ERROR, "character cannot be > 255!");

                            currentChar--; // move back
                            str += (char)i; // add byte to string
                            break;
                        }
                        return Token(TOKEN_ERROR, "Unrecognized escape sequence!");
                    }
                }
                
                advanceChar();
            } else {
                str += advanceChar();
            }
        }

        advanceChar();

        if (isEnd())
            return Token(TOKEN_ERROR, "Unterminated string!");

        return Token(TOKEN_STRING, str);
    }

    Token readNumber() {
        std::string str;

        if (peekChar() == '0' && *(currentChar+1) == 'x') { // read a hexadecimal number
            currentChar+=2;

            while (isNumeric(peekChar()) || isalpha(peekChar()) && !isEnd()) {
                str += advanceChar();
            }

             return Token(TOKEN_HEXADEC, str);
        }

        while (isNumeric(peekChar()) && !isEnd() || peekChar() == '.') {
            str += advanceChar();
        }

        return Token(TOKEN_NUMBER, str);
    }

    Token readIdentifier() {
        std::string name;

        // strings can be alpha-numeric + _
        while ((isAlpha(peekChar()) || isNumeric(peekChar())) && peekChar() != '.' && !isEnd()) {
            name += advanceChar();
        }

        return checkReserved(name);
    }

    // skips spaces, tabs, /r, etc.
    void consumeWhitespace() {
        while (!isEnd()) {
            switch (peekChar()) {
                case ' ':
                case '\t':
                case '\r':
                    advanceChar();
                    break;

                case '/': {
                    if (peekNextChar() == '/') {
                        // A comment goes until the end of the line.
                        while (peekChar() != '\n' && !isEnd()) advanceChar();
                    } else {
                        return;
                    }                                                
                    break;
                }
                default:
                    return;
            }
        }
    }

    Token scanNextToken() {
        SCANNEXTTOKENSTART:
        
        consumeWhitespace();

        if (isEnd())
            return Token(TOKEN_EOF); // end of file

        char character = advanceChar();
        if (isNumeric(character)) {
            currentChar--;
            return readNumber();
        }
        if (isAlpha(character)) {
            currentChar--;
            return readIdentifier();
        }
        
        switch (character) {
            // SINGLE CHARACTER TOKENS
            case '(': openBraces++; return Token(TOKEN_OPEN_PAREN);
            case ')': openBraces--; return Token(TOKEN_CLOSE_PAREN); 
            case '{': openBraces++; return Token(TOKEN_OPEN_BRACE); 
            case '}': openBraces--; return Token(TOKEN_CLOSE_BRACE); 
            case '[': openBraces++; return Token(TOKEN_OPEN_BRACKET); 
            case ']': openBraces--; return Token(TOKEN_CLOSE_BRACKET); 
            case '.': return Token(TOKEN_DOT); 
            case '*': return Token(TOKEN_STAR); 
            case '/': return Token(TOKEN_SLASH); 
            case '+': {
                if (*currentChar == '+') {
                    advanceChar();
                    return Token(TOKEN_PLUS_PLUS);
                }
                return Token(TOKEN_PLUS); 
            }
            case '-': {
                if (*currentChar == '-') {
                    advanceChar();
                    return Token(TOKEN_MINUS_MINUS);
                }
                return Token(TOKEN_MINUS); 
            }
            case ',': return Token(TOKEN_COMMA); 
            case ':': return Token(TOKEN_COLON); 
            case ';': return Token(TOKEN_EOS); // you can end a statement in a single line.
            case '\n': {
                line++; // new line :)
                if (openBraces == 0) // all the braces are equal, nothing is waiting to be closed. so it's okay to end the statement for our user :) (ur welcome!!)
                    return Token(TOKEN_EOS);
                else // starts over :)
                    goto SCANNEXTTOKENSTART;
                break;
            }

            // ONE OR TWO CHARACTER TOKENS
            case '=': return Token(matchChar('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
            case '>': return Token(matchChar('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
            case '<': return Token(matchChar('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
            case '!': return Token(matchChar('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG); 

            // LITERALS
            case '"': return readString();
            case '\0': line++; return Token(TOKEN_EOF); // we just consumed the null-terminator. get out NOW aaaAAAAAA
            default:
                return Token(TOKEN_ERROR, std::string("Unrecognized symbol: \"") + character + "\"");
        }
    }

    Token currentToken;
    Token previousToken;

    inline Token getPreviousToken() {
        return previousToken;
    }

    inline Token getCurrentToken() {
        return currentToken;
    }

    Token getNextToken() {
        previousToken = currentToken;
        currentToken = scanNextToken(); // gets the next token
        DEBUGLOG(std::cout << "Token\t("<< previousToken.type << ") : " << previousToken.str << std::endl);
        if (currentToken.type == TOKEN_ERROR)
            throwObjection(currentToken.str);
        return currentToken;
    }

    inline bool checkToken(GTokenType type) {
        return currentToken.type == type;
    }

    bool matchToken(GTokenType type) {
        if (!checkToken(type)) 
            return false;
        // else consume next token and return false
        getNextToken();
        return true;
    }

// ======================================================== [[Functions for parsing tokens into bytecode :eyes:]] ========================================================

    inline GChunk* getChunk() {
        return function->val;
    }

    inline int computeOffset(int i) {
        return (getChunk()->code.size() - i) - 1;
    }

    inline int emitInstruction(INSTRUCTION i) {
        return getChunk()->addInstruction(i, line);
    }

    inline int emitReturn() {
        emitPUSHCONST(CREATECONST_NIL());
        return emitInstruction(CREATE_iAx(OP_RETURN, 1));
    }

    inline ParseRule getRule(GTokenType t) {
        return GavelParserRules[t];
    }

    /* emitPUSHCONST(GValue c)
        adds constant to the getChunk() constant list, then adds the OP_LOADCONST instruction
    */
    int emitPUSHCONST(GValue c) {
        pushedVals++;
        return emitInstruction(CREATE_iAx(OP_LOADCONST, getChunk()->addConstant(c)));
    }

    /* emitJumpBack(int instructionIndex)
        creates an instruction to jmp back to the given instructionIndex
    */
    int emitJumpBack(int i) {
        return emitInstruction(CREATE_iAx(OP_JMPBACK, computeOffset(i)));
    }

    int emitPlaceHolder() {
        return emitInstruction(0); // placeholder
    }

    // patches a placehoder with an instruction
    void patchPlaceholder(int i, INSTRUCTION inst) {
        getChunk()->patchInstruction(i, inst);
    }

    bool consumeToken(GTokenType expectedType, std::string errStr) {
        if (getCurrentToken().type == expectedType) {
            getNextToken(); // advance to the next token
            return true;
        }

        throwObjection(errStr);
        return false;
    }

    void namedVariable(std::string id, bool canAssign) {
        int getOp, setOp;
        int indx = findLocal(id);
        if (indx != -1) {
            DEBUGLOG(std::cout << "'" << id << "' is a local!" << std::endl);
            // found the local :flushed:
            getOp = OP_GETBASE;
            setOp = OP_SETBASE;
        } else {
            indx = findUpval(id);
            if (indx != -1) {
                DEBUGLOG(std::cout << "'" << id << "' is an upval!" << std::endl);
                // it's an upvalue!
                getOp = OP_GETUPVAL;
                setOp = OP_SETUPVAL;
            } else {
                DEBUGLOG(std::cout << "'" << id << "' is a global!" << std::endl);
                // didn't find the local, default to global
                indx = getChunk()->addIdentifier(id);
                getOp = OP_GETGLOBAL;
                setOp = OP_SETGLOBAL;
            }
        }
        
        if (canAssign && matchToken(TOKEN_EQUAL)) {            
            expression();  
            emitInstruction(CREATE_iAx(setOp, indx));
            pushedVals--;
        } else if (canAssign && matchToken(TOKEN_PLUS_PLUS)) {
            emitInstruction(CREATE_iAx(getOp, indx));
            emitInstruction(CREATE_iAx(OP_INC, 1)); // it'll leave the pre inc value on the stack
            pushedVals++; // even after we assign we'll have an extra pushed value
            emitInstruction(CREATE_iAx(setOp, indx));
        } else if (canAssign && matchToken(TOKEN_MINUS_MINUS)) {
            emitInstruction(CREATE_iAx(getOp, indx));
            emitInstruction(CREATE_iAx(OP_DEC, 1)); // it'll leave the pre dec value on the stack
            pushedVals++; // even after we assign we'll have an extra pushed value
            emitInstruction(CREATE_iAx(setOp, indx));
        }else {            
            emitInstruction(CREATE_iAx(getOp, indx));
            pushedVals++;
        }
    }

    void createTable() {
        // keep track of how many object & key pairs are going to populate the table
        int pairs = 0;

        // setup table 
        if (!matchToken(TOKEN_CLOSE_BRACE)) {
            do {

                // get key

                int startPushed = pushedVals;
                expression();

                // we expect ':' to separate the key from the value
                if (!matchToken(TOKEN_COLON)) {
                    throwObjection("Illegal syntax! Separate the key from the value with ':'!");
                    return;
                }

                // make sure we have something to actually set as the key
                if (startPushed >= pushedVals) {
                    throwObjection("Illegal syntax! Key expected!");
                    return;
                }

                // get value

                startPushed = pushedVals;
                expression();
                // make sure we have something to actually set as the value
                if (startPushed >= pushedVals) {
                    throwObjection("Illegal syntax! Value expected!");
                    return;
                }

                pairs++;
            } while (matchToken(TOKEN_COMMA));
            consumeToken(TOKEN_CLOSE_BRACE, "Expected an end to table definition!");
        }

        // rebalance the stack
        pushedVals-=pairs*2;

        // craft table with [pairs] (object & key)s on stack
        emitInstruction(CREATE_iAx(OP_NEWTABLE, pairs));
        pushedVals++; // the table is now on the stack, so track that
    }

    void defineVariable(Token keyword) {
        if (matchToken(TOKEN_IDENTIFIER)) {
            std::string varName = previousToken.str;
            switch (keyword.type) {
                case TOKEN_VAR: { // scope type is automatically picked for you
                    DEBUGLOG(std::cout << "VAR : " << previousToken.str << std::endl);
                    if (scopeDepth > 0)  // if we're in a scope *at all*, this should be a local variable
                        goto DEFINELOCALLABEL;
                    else // it's a global var
                        goto DEFINEGLOBALLABEL;
                    break;
                }
                case TOKEN_LOCAL: { // scope type is forced to local
                    DEFINELOCALLABEL:
                    if (matchToken(TOKEN_EQUAL)) { // it's assigning it aswell
                        expression(); // pushes local to stack
                        pushedVals--;
                    } else { // just allocating space for it
                        emitInstruction(CREATE_i(OP_NIL)); // sets the local to 'nil'
                    }
                    declareLocal(varName);
                    markLocalInitalized(); // allows our parser to use it :)
                    break;
                }
                case TOKEN_GLOBAL: { // scope type is forced to global
                    DEFINEGLOBALLABEL:
                    if (matchToken(TOKEN_EQUAL)) { // it's a global by default
                        int id = getChunk()->addIdentifier(varName);
                        expression(); // get var
                        emitInstruction(CREATE_iAx(OP_DEFINEGLOBAL, id));
                        pushedVals--;
                    } else { // just allocating space
                        int id = getChunk()->addIdentifier(varName);
                        emitInstruction(CREATE_i(OP_NIL)); // sets the global to 'nil'
                        emitInstruction(CREATE_iAx(OP_DEFINEGLOBAL, id));
                    }
                    break;
                }
                default:
                    throwObjection("ERR INVALID TOKEN");
                    break;
            }
        } else {
            throwObjection("Identifier expected after 'var'");
        }
    }

    int parseArguments() {
        int passedArgs = 0;
        if (!checkToken(TOKEN_CLOSE_PAREN)) {
            do {
                DEBUGLOG(std::cout << "grabbing expression" << std::endl);
                expression();
                passedArgs++;
            } while (matchToken(TOKEN_COMMA));
        }
        consumeToken(TOKEN_CLOSE_PAREN, "Expect ')' to end function call!");

        pushedVals -= passedArgs; // rebalance the stack
        return passedArgs;
    }

    // read rule and decide how to parse the token.
    void runParseFix(Token token, ParseFix rule, bool canAssign) {
        switch (rule) {
            // BINARY OPERATORS
            case PARSEFIX_BINARY:   binaryOp(token); break;
            case PARSEFIX_UNARY:    unaryOp(token); break;
            case PARSEFIX_PREFIX:   prefix(token); break;
            // CONDITIONALS
            case PARSEFIX_OR: {
                int endJmp = emitPlaceHolder(); // allocate space for the jump
                emitInstruction(CREATE_iAx(OP_POP, 1));

                parsePrecedence(PREC_OR);
                patchPlaceholder(endJmp, CREATE_iAx(OP_CNDJMP, computeOffset(endJmp)));
                break;
            }
            case PARSEFIX_AND: {
                int endJmp = emitPlaceHolder(); // allocate space for the jump
                emitInstruction(CREATE_iAx(OP_POP, 1)); // pop boolean from previous conditional expression

                parsePrecedence(PREC_AND); // parse the rest of the conditional
                patchPlaceholder(endJmp, CREATE_iAx(OP_CNDNOTJMP, computeOffset(endJmp))); // if it's false skip the whole conditional
                break;
            }
            // CONSTANTS
            case PARSEFIX_NUMBER: {
                DEBUGLOG(std::cout << "NUMBER CONSTANT: " << token.str << std::endl);
                double num;

                switch (token.type) {
                    case TOKEN_NUMBER: {
                        num = std::stod(token.str.c_str());
                        break;    
                    }
                    case TOKEN_HEXADEC: {
                        num = (double)strtol(token.str.c_str(), NULL, 16); // parses number from base16
                        break;
                    }
                }

                emitPUSHCONST(CREATECONST_NUMBER(num));
                break;
            }
            case PARSEFIX_STRING: { // emits the string :))))
                emitPUSHCONST(Gavel::addString(token.str));
                break;
            }
            case PARSEFIX_LITERAL: {
                switch (token.type) {
                    case TOKEN_TRUE:    emitInstruction(CREATE_i(OP_TRUE)); pushedVals++; break;
                    case TOKEN_FALSE:   emitInstruction(CREATE_i(OP_FALSE)); pushedVals++; break;
                    case TOKEN_NIL:     emitInstruction(CREATE_i(OP_NIL)); pushedVals++; break;
                    case TOKEN_OPEN_BRACE: { // table
                        // we don't increment pushedVals, because createTable does that for us!
                        createTable();
                        break;
                    }
                    default:
                        break; // shouldn't ever happen but /shrug
                }
                break;
            }
            case PARSEFIX_LAMBDA: {
                // parse lambda & push it to stack
                functionCompile(CHUNK_FUNCTION, "_"+function->getName());
                break;
            }
            case PARSEFIX_GROUPING: {
                DEBUGLOG(std::cout << "-started grouping" << std::endl);
                expression();
                DEBUGLOG(std::cout << "Token (" << getCurrentToken().type << ")\t" << getCurrentToken().str << std::endl);
                consumeToken(TOKEN_CLOSE_PAREN, "Expected ')' after expression.");
                DEBUGLOG(std::cout << "-ended grouping" << std::endl);
                break;
            }
            case PARSEFIX_INDEX: {
                DEBUGLOG(std::cout << "indexing..." << std::endl);
                // First get the index
                if (token.type == TOKEN_DOT) {
                    if (!consumeToken(TOKEN_IDENTIFIER, "Expected index string after '.'"))
                        break;

                    DEBUGLOG(std::cout << "index with \"" << previousToken.str << "\"" << std::endl);

                    emitPUSHCONST(Gavel::addString(previousToken.str));
                } else if (token.type == TOKEN_OPEN_BRACKET) {
                    int startPushed = pushedVals;
                    expression();
                    if (startPushed >= pushedVals) {
                        throwObjection("Expected an index!");
                        break;
                    }

                    if (!consumeToken(TOKEN_CLOSE_BRACKET, "Expected ']' after expression."))
                        break;
                }

                if (matchToken(TOKEN_EQUAL)) { // if it's assigning the index, aka new index
                    // get newVal
                    int startPushed = pushedVals;
                    expression();
                    if (startPushed >= pushedVals) {
                        throwObjection("Expected an expression!");
                        break;
                    }

                    pushedVals-=3; // OP_NEWINDEX pops 3 values off the stack (newval, index, and table)
                    emitInstruction(CREATE_i(OP_NEWINDEX));
                } else {
                    pushedVals--; // 2 values are popped (index & table), but one is pushed (val), so in total 1 less value on the stack
                    emitInstruction(CREATE_i(OP_INDEX));
                }
                break;
            }
            case PARSEFIX_CALL: {
                int passedArgs = parseArguments();
                emitInstruction(CREATE_iAx(OP_CALL, passedArgs));
                break;
            }
            case PARSEFIX_DEFVAR: { // new variable being declared
                defineVariable(token);
                break;
            }
            case PARSEFIX_VAR: {
                namedVariable(token.str, canAssign);
                break;
            }
            case PARSEFIX_SKIP:
            case PARSEFIX_ENDPARSE:
                break;
            default:
                throwObjection("Illegal syntax! token: [" + std::to_string(token.type) + "] rule: " + std::to_string(rule));
                break;
        }
    }

    // tells our parser to parse a precedence
    void parsePrecedence(Precedence pre) {
        getNextToken();
        Token token = previousToken;
        ParseRule rule = getRule(token.type);
        DEBUGLOG(std::cout << "PREFIX\t");

        bool canAssign = pre <= PREC_ASSIGNMENT;
        runParseFix(token, rule.prefix, canAssign); // run the prefix, expects currentToken to be unparsed.

        while (pre <= getRule(getCurrentToken().type).precedence && !panic) {
            getNextToken();
            DEBUGLOG(std::cout << "INFIX\t");
            runParseFix(previousToken, getRule(previousToken.type).infix, canAssign);
        }

        if (canAssign && matchToken(TOKEN_EQUAL)) {
            // invalid assignment!!
            throwObjection("Invalid assignement!");
        }
    }

    void block() {
        while (!checkToken(TOKEN_END) && !checkToken(TOKEN_EOF) && !panic) {
            declaration();
        }

        consumeToken(TOKEN_END, "Expected 'end' to close scope");
    }

    void forStatement() {
        beginScope(); // opens a scope

        consumeToken(TOKEN_OPEN_PAREN, "Expected '(' after 'for'");
        if (matchToken(TOKEN_IDENTIFIER)) {

        } else if (matchToken(TOKEN_EOS)) {
            // no intializer
        } else {
            expression();
        }
        consumeToken(TOKEN_EOS, "Expected ';' after assignment");


        // parse conditional
        int loopStart = getChunk()->code.size() - 2;
        
        int exitJmp = -1;
        if (!matchToken(TOKEN_EOS)) {
            DEBUGLOG(std::cout << "parsing conditional..." << std::endl);
            expressionStatement();
            DEBUGLOG(std::cout << "done parsing conditional..." << std::endl);

            exitJmp = emitPlaceHolder();
            pushedVals--; // the expression is popped by OP_IFJMP
        }

        if (!matchToken(TOKEN_CLOSE_PAREN)) {                              
            int bodyJump = emitPlaceHolder();

            int incrementStart = getChunk()->code.size() - 2;
            expression();
            pushedOffset = balanceStack(pushedOffset);
            //emitInstruction(CREATE_iAx(OP_POP, 1));
            consumeToken(TOKEN_CLOSE_PAREN, "Expect ')' after for clauses.");

            emitJumpBack(loopStart);
            loopStart = incrementStart;
            patchPlaceholder(bodyJump, CREATE_iAx(OP_JMP, computeOffset(bodyJump)));
        }

        // enter loop body
        beginScope();
        if (matchToken(TOKEN_DO)) {
            block();
        }
        endScope();

        emitJumpBack(loopStart);

        if (exitJmp != -1)
            patchPlaceholder(exitJmp, CREATE_iAx(OP_IFJMP, computeOffset(exitJmp)));

        endScope(); // closes a scope
    }

    void forEachStatement() {
        
    }

    void whileStatement() {
        int loopStart = getChunk()->code.size() - 2;
        expression(); // parse conditional maybe?
        
        int exitJmp = emitPlaceHolder();
        pushedVals--;

        // our loop body. they can use 'do .. end' to expand the body to multiple statements
        statement();

        emitJumpBack(loopStart);

        patchPlaceholder(exitJmp, CREATE_iAx(OP_IFJMP, computeOffset(exitJmp)));
    }

    void ifStatement() {
        // parse expression until 'then'
        expression();
        consumeToken(TOKEN_THEN, "expected 'then' after expression!");

        // allocate space for our conditional jmp instruction
        int cndjmp = emitPlaceHolder();
        pushedVals--;
       
        int curLine = line;

        // starts a new scope
        beginScope();
        while (!(checkToken(TOKEN_END) || checkToken(TOKEN_ELSE) || checkToken(TOKEN_ELSEIF)) && !checkToken(TOKEN_EOF) && !panic) {
            declaration();
        }
        endScope();

        if (matchToken(TOKEN_ELSE)) {
            // make space for the else jmp
            int elseJmp = emitPlaceHolder();

            // patch our jmp with the offset
            patchPlaceholder(cndjmp, CREATE_iAx(OP_IFJMP, computeOffset(cndjmp)));
            
            // starts a new scope
            beginScope();
            block(); // parses until 'end'
            endScope();
            patchPlaceholder(elseJmp, CREATE_iAx(OP_JMP, computeOffset(elseJmp)));
        } else if (matchToken(TOKEN_ELSEIF)) {
            // make space for the else jmp
            int elseJmp = emitPlaceHolder();
            
            // patch our jmp with the offset
            patchPlaceholder(cndjmp, CREATE_iAx(OP_IFJMP, computeOffset(cndjmp)));

            // parse elseif conditional
            ifStatement();
            patchPlaceholder(elseJmp, CREATE_iAx(OP_JMP, computeOffset(elseJmp)));
        } else if (matchToken(TOKEN_END)) {
            // patch our jmp with the offset
            patchPlaceholder(cndjmp, CREATE_iAx(OP_IFJMP, computeOffset(cndjmp)));
        } else {
            throwObjection("'end' expected to end scope to if statement defined on line " + std::to_string(curLine));
        }
    }

    void functionCompile(ChunkType t, std::string n) {
        GavelParser funcCompiler(currentChar, t, n);

        consumeToken(TOKEN_OPEN_PAREN, "Expected '(' for function definition!");
        if (!checkToken(TOKEN_CLOSE_PAREN)) {
            do {
                getNextToken();
                DEBUGLOG(std::cout << "marking " << previousToken.str << " as local" << std::endl);
                funcCompiler.args++; // increment arguments
                funcCompiler.declareLocal(previousToken.str);
                funcCompiler.markLocalInitalized();
            } while (matchToken(TOKEN_COMMA) && !panic);
        }
        consumeToken(TOKEN_CLOSE_PAREN, "Exepcted ')' to end function definition!");

        // move tokenizer state
        funcCompiler.setParent(this);
        funcCompiler.line = line;
        funcCompiler.currentChar = currentChar;
        funcCompiler.previousToken = previousToken;
        funcCompiler.currentToken = currentToken;
        // compile function block
        funcCompiler.beginScope();
        funcCompiler.block();
        funcCompiler.endScope();
        
        // after we compile the function block, push function constant to stack
        GObjectFunction* fObj = funcCompiler.getFunction();
        funcCompiler.emitReturn();
        pushedVals++;
        emitInstruction(CREATE_iAx(OP_CLOSURE, getChunk()->addConstant(GValue((GObject*)fObj))));

        // list out upvalues
        for (Upvalue upval : funcCompiler.upvalues) {
            // uses instructions, a little fat but eh, it's whatever.
            emitInstruction(CREATE_iAx((upval.isLocal ? OP_GETBASE : OP_GETUPVAL), upval.index));
        }

        // restore our tokenizer state
        line = funcCompiler.line;
        currentChar = funcCompiler.currentChar;
        previousToken = funcCompiler.previousToken;
        currentToken = funcCompiler.currentToken;
        return;
    }

    void functionDeclaration() {
        if (matchToken(TOKEN_IDENTIFIER)) {
            std::string id = previousToken.str;
            bool local = false;

            if (scopeDepth > 0) {
                local = true;
                // it's a local
                declareLocal(id);
                markLocalInitalized();
            }

            functionCompile(CHUNK_FUNCTION, id);

            if (!local) { // if it's a global, define it 
                emitInstruction(CREATE_iAx(OP_DEFINEGLOBAL, getChunk()->addIdentifier(id))); 
            }
            pushedVals--;
        } else {
            throwObjection("Identifier expected for function!");
        }
    }

    void expressionStatement() {                        
        expression();
        if ((pushedVals-pushedOffset) <= 0) {
            throwObjection("Expression expected!");
            return;
        }
        consumeToken(TOKEN_EOS, "Expect ';' after expression.");
    }

    // parses a single expression
    void expression() {
        parsePrecedence(PREC_ASSIGNMENT); // parses until assignement is reached :eyes:
    }

    // called at the end of a statement, returns new value of pushedVals
    int balanceStack(int offset) {
        if ((pushedVals-offset) < 0) {
            throwObjection("Expression expected! [" + std::to_string((pushedVals - offset)) + "]");
        } else if ((pushedVals-offset) > 0) {
            DEBUGLOG(std::cout << "POPING! " << std::endl);
            emitInstruction(CREATE_iAx(OP_POP, (pushedVals - offset))); // pop unexpected values
            pushedVals = pushedVals - (pushedVals - offset);
        } // else - stack is already balanced! yay!
        return pushedVals;
    }

    void statement() {
        int pastPushed = pushedOffset; // saves past pushed
        pushedOffset = pushedVals;

        if (matchToken(TOKEN_DO)) {
            beginScope();
            block();
            endScope();
        } else if (matchToken(TOKEN_IF)) {
            ifStatement();
        } else if (matchToken(TOKEN_WHILE)) {
            whileStatement();
        } else if (matchToken(TOKEN_FOR)) {
            forStatement();
        } else if (matchToken(TOKEN_FUNCTION)) {
            functionDeclaration();
        } else if (matchToken(TOKEN_RETURN)) {
            expression();

            // there was a const/var being returned
            if (pushedVals > 0) {
                emitInstruction(CREATE_iAx(OP_RETURN, pushedVals));
                pushedVals = 0;
            } else {
                // write generic return (aka return nil)
                emitReturn();
            }
        } else {
            expression();
        }

        pushedOffset = pastPushed;
        // balance the stack
        pushedOffset = balanceStack(pushedOffset);
    }

    void declaration() {
        statement();
    }

    void prefix(Token token) {
        consumeToken(TOKEN_IDENTIFIER, "identifier expected after prefix operator");
        std::string ident = getPreviousToken().str;
        namedVariable(ident, false); // don't let them assign, just get the value of the var on the stack

        switch (token.type) {
            case TOKEN_PLUS_PLUS: {
                emitInstruction(CREATE_iAx(OP_INC, 2)); // it'll leave the post inc value on the stack
                // pushedVal is already incremented in namedVariable
                break;
            }
            case TOKEN_MINUS_MINUS: {
                emitInstruction(CREATE_iAx(OP_DEC, 2)); // it'll leave the post dec value on the stack
                // pushedVal is already incremented in namedVariable
                break;
            }
            default:
                return; // shouldn't ever happen tbh
        }

        int setOp;
        int indx = findLocal(ident);
        if (indx != -1) {
            // found the local :flushed:
            setOp = OP_SETBASE;
        } else {
            indx = findUpval(ident);
            if (indx != -1) {
                // it's an upvalue!
                setOp = OP_SETUPVAL;
            } else {
                // didn't find the local, default to global
                indx = getChunk()->addIdentifier(ident);
                setOp = OP_SETGLOBAL;
            }
        }

        emitInstruction(CREATE_iAx(setOp, indx));
    }

    void unaryOp(Token token) {
        DEBUGLOG(std::cout << "Unary operator token! " << std::endl);
        parsePrecedence(PREC_UNARY); // parses using lowest precedent level, basically anything will stop it lol
        DEBUGLOG(std::cout << "ended Unary operation token" << std::endl);

        switch (token.type) {
            case TOKEN_MINUS:   emitInstruction(CREATE_i(OP_NEGATE)); break;
            case TOKEN_BANG:    emitInstruction(CREATE_i(OP_NOT)); break;
            default:
                return;
        }
    }

    void binaryOp(Token token) {
        DEBUGLOG(std::cout << "Binary operator token! " << std::endl);
        // Compile the right operand.                            
        ParseRule rule = getRule(token.type);                 
        parsePrecedence((Precedence)(rule.precedence + 1));    
        DEBUGLOG(std::cout << "end Binary operator token! " << std::endl);

        // Emit the operator instruction.                        
        switch (token.type) {   
            case TOKEN_EQUAL_EQUAL:     emitInstruction(CREATE_i(OP_EQUAL)); break; 
            case TOKEN_BANG_EQUAL:      emitInstruction(CREATE_i(OP_EQUAL)); emitInstruction(CREATE_i(OP_NOT)); break;
            case TOKEN_LESS:            emitInstruction(CREATE_i(OP_LESS)); break; 
            case TOKEN_LESS_EQUAL:      emitInstruction(CREATE_i(OP_GREATER)); emitInstruction(CREATE_i(OP_NOT)); break; 
            case TOKEN_GREATER:         emitInstruction(CREATE_i(OP_GREATER)); break; 
            case TOKEN_GREATER_EQUAL:   emitInstruction(CREATE_i(OP_LESS)); emitInstruction(CREATE_i(OP_NOT)); break; 
            case TOKEN_PLUS:            emitInstruction(CREATE_i(OP_ADD)); break;
            case TOKEN_MINUS:           emitInstruction(CREATE_i(OP_SUB)); break;
            case TOKEN_STAR:            emitInstruction(CREATE_i(OP_MUL)); break;
            case TOKEN_SLASH:           emitInstruction(CREATE_i(OP_DIV)); break;
            default:                                               
                return; // Unreachable.                              
        } 
        pushedVals--;
    }

public:
    GavelParser() {}
    GavelParser(const char* s, ChunkType ct = CHUNK_SCRIPT, std::string n = "_MAIN"): 
        script(s), currentChar(s), chunkType(ct) {
        scriptSize = strlen(s);
        function = new GObjectFunction();
        function->setName(n);

        locals[localCount++] = {"", -1, false}; // allocates space for our function on the stack
    }

// ======================================================== [[Public utility functions]] ========================================================

    GObjection getObjection() {
        return objection;
    }

    bool compile() {
        getNextToken();
        while (!(matchToken(TOKEN_EOF) || panic)) { // keep parsing till the end of the file or a panic is thrown
            declaration();
        }

        emitReturn(); // mark end of function

        if (panic) {
            // free function for them
            delete getFunction();
        }

        // returns true if we're *not* in a paniced state
        return !panic;
    }

    GChunk* getRawChunk() {
        return getChunk();
    }

    GObjectFunction* getFunction() {
        function->setArgs(args);
        function->setUpvalueCount(upvalues.size());
        return function;
    }
};

#endif

// =============================================================[[STANDARD LIBRARY]]=============================================================

namespace GavelLib {
    GValue _print(GState* state, int args) {
        // prints all the passed arguments
        for (int i = args-1; i >= 0; i--) {
            std::cout << state->stack.getTop(i).toString();
        }

        std::cout << std::endl;
        return CREATECONST_NIL(); // no return value (technically there is [NIL], but w/e)
    }
    
    GValue _input(GState* state, int args) {
        // prints all the passed arguments
        for (int i = args-1; i >= 0; i--) {
            std::cout << state->stack.getTop(i).toString();
        }

        std::string i;
        std::getline(std::cin, i);

        return Gavel::newGValue(i); // newGValue is the recommended way to create values. it'll handle stuff like adding to the gc, and has automatic bindings for c++ primitives to gvalues!
    }

    GValue _compileString(GState* state, int args) {
#ifndef EXCLUDE_COMPILER
        // verifies args
        if (args != 1) {
            state->throwObjection("Expected 1 argument, " + std::to_string(args) + " given");
            return CREATECONST_NIL();
        }

        GValue arg = state->stack.getTop(0);
        if (!ISGVALUESTRING(arg)) {
            state->throwObjection("Expected string, got " + arg.toStringDataType());
            return CREATECONST_NIL();
        }

        // compiles GObjectFunction from string
        GavelParser compiler(READGVALUESTRING(arg).c_str());
        if (!compiler.compile()) { // compiler objection was thrown, return nil
            std::cout << compiler.getObjection().getFormatedString() << std::endl;
            return CREATECONST_NIL();
        }

        // returns new function
        return Gavel::newGValue(compiler.getFunction());
#else
        state->throwObjection("The has the compiler stripped!");
        return CREATECONST_NIL();
#endif
    }

    GValue _tonumber(GState* state, int args) {
        if (args != 1) {
            state->throwObjection("Expected 1 argument, " + std::to_string(args) + " given");
            return CREATECONST_NIL();
        }

        GValue arg = state->stack.getTop(0);
        if (!ISGVALUESTRING(arg)) {
            state->throwObjection("Expected string, got " + arg.toStringDataType());
            return CREATECONST_NIL();
        }

        return Gavel::newGValue((atof(READGVALUESTRING(arg).c_str())));
    }

    GValue _tostring(GState* state, int args) {
        if (args != 1) {
            state->throwObjection("Expected 1 argument, " + std::to_string(args) + " given");
            return CREATECONST_NIL();
        }

        GValue arg = state->stack.getTop(0);
        return Gavel::newGValue(arg.toString());
    }

    // library implementation for math.sin
    GValue _sin(GState* state, int args) {
        if (args != 1) {
            state->throwObjection("Expected 1 argument, " + std::to_string(args) + " given");
            return CREATECONST_NIL();
        }

        GValue arg = state->stack.getTop(0);
        if (!ISGVALUENUMBER(arg)) {
            state->throwObjection("Expected number, got " + arg.toStringDataType());
            return CREATECONST_NIL();
        }

        // reads arg number value, calls sin() and returns the result as a GValue
        return Gavel::newGValue(sin(READGVALUENUMBER(arg)));
    }

    // library implementation for math.cos
    GValue _cos(GState* state, int args) {
        if (args != 1) {
            state->throwObjection("Expected 1 argument, " + std::to_string(args) + " given");
            return CREATECONST_NIL();
        }

        GValue arg = state->stack.getTop(0);
        if (!ISGVALUENUMBER(arg)) {
            state->throwObjection("Expected number, got " + arg.toStringDataType());
            return CREATECONST_NIL();
        }

        // reads arg number value, calls cos() and returns the result as a GValue
        return Gavel::newGValue(cos(READGVALUENUMBER(arg)));
    }

    // library implementation for math.tan
    GValue _tan(GState* state, int args) {
        if (args != 1) {
            state->throwObjection("Expected 1 argument, " + std::to_string(args) + " given");
            return CREATECONST_NIL();
        }

        GValue arg = state->stack.getTop(0);
        if (!ISGVALUENUMBER(arg)) {
            state->throwObjection("Expected number, got " + arg.toStringDataType());
            return CREATECONST_NIL();
        }

        // reads arg number value, calls tan() and returns the result as a GValue
        return Gavel::newGValue(tan(READGVALUENUMBER(arg)));
    }

    void loadIO(GState* state) {
        state->setGlobal("print", _print);
        state->setGlobal("input", _input);
        state->setGlobal("compilestring", _compileString);
    }

    void loadString(GState* state) {

    }

    void loadMath(GState* state) {
        GObjectTable* tbl = new GObjectTable();
        tbl->setIndex("pi", 3.14159265);
        tbl->setIndex("sin", _sin);
        tbl->setIndex("cos", _cos);
        tbl->setIndex("tan", _tan);
        state->setGlobal("math", tbl);
    }

    void loadLibrary(GState* state) {
        loadIO(state);
        loadMath(state);

        state->setGlobal("tonumber", _tonumber);
        state->setGlobal("tostring", _tostring);
    }

    std::string getVersion() {
        return "GavelScript [COSMO] " GAVEL_MAJOR "." GAVEL_MINOR;
    }
}

// ===========================================================================[[ (DE)SERIALIZER/(UN)DUMPER ]]===========================================================================

#define GCODEC_VERSION_BYTE '\x01'
#define GCODEC_HEADER_MAGIC "COSMO"

// TODO: add support for comparing double sizes to be more platform independent

/* GDump
    This class is in charge of dumping GObjectFunction* to a binary blob (of uint8_ts). This is useful for precompiling scripts in memory, sending scripts over a network, or just dumping to a file for reuse later.
*/
class GDump {
private:
    std::ostringstream data;
    std::string out;

    bool getBigEndian() {
        union {
            uint32_t i;
            char bytes[4];
        } encodedi = {0xBEDBABE};

        return encodedi.bytes[0] == 0xBE;
    }

    void writeByte(uint8_t b) {
        data.write(reinterpret_cast<const char*>(&b), sizeof(uint8_t));
    }

    // we use uint32_t so we know it we be the same size no matter the platform
    void writeSizeT(uint32_t s) {
        data.write(reinterpret_cast<const char*>(&s), sizeof(uint32_t));
    }

    void writeInstruction(INSTRUCTION inst) {
        data.write(reinterpret_cast<const char*>(&inst), sizeof(INSTRUCTION));
    }

    void writeRawString(const char* str, int strSize) {
        writeSizeT(strSize); // writes size of string
        data.write(reinterpret_cast<const char*>(str), strSize); // writes string to stream!
    }

    // assumes GValue base was already written (GAVEL_TOBJ)
    void writeObject(GObject* obj) {
        writeByte(obj->type); // writes our type
        // some objects are impossible to serialize because they are runtime-only, eg. GOBJECT_UPVAL or GOBJECT_CLOSURE
        // only "portable" objects are serialized
        switch(obj->type) {
            // skips GOBJECT_NULL; there's nothing to do
            case GOBJECT_STRING:
                // writes string to stream!
                writeRawString(READOBJECTVALUE(obj, GObjectString*).c_str(), READOBJECTVALUE(obj, GObjectString*).size());
                break;
            case GOBJECT_TABLE:
                // NOTE: vanilla GavelScript doesn't generate tables in the constant list, however i'm adding
                // support for serializing tables because this project is designed to be very hackable so if you 
                // wanted to add support for constant tables in the parser, you can!

                writeSizeT(READOBJECTVALUE(obj, GObjectTable*).getSize());
                for (auto pair : READOBJECTVALUE(obj, GObjectTable*).hashTable) {
                    writeValue(pair.first.key); // first write the key
                    writeValue(pair.second); // then the value
                }
                break;
            case GOBJECT_FUNCTION: {
                GObjectFunction* func = reinterpret_cast<GObjectFunction*>(obj);
                writeRawString(func->getName().c_str(), func->getName().size()); // first the name
                writeSizeT(func->getArgs()); // arg count
                writeSizeT(func->getUpvalueCount()); // then the upvalues
                writeChunk(func->val); // and finally the chunk
                break;
            }
            default:
                // no data to write
                break;
        }
    }

    void writeValue(GValue val) {
        writeByte(val.type); // writes the type first
        switch(val.type) {
            // skips GAVEL_TNIL; there's nothing to do
            case GAVEL_TBOOLEAN:
                writeByte(READGVALUEBOOL(val)); // writes the value of true/false
                break;
            case GAVEL_TNUMBER:
                // writes double as bytes to stream (this is basically the only thing platform dependant)
                data.write(reinterpret_cast<const char*>(&READGVALUENUMBER(val)), sizeof(double));
                break;
            case GAVEL_TOBJ:
                writeObject(val.val.obj);
                break;
            default:
                // no data to write
                break;
        }
    }

    void writeIdentifiers(std::vector<GObjectString*> idnts) {
        writeSizeT(idnts.size());
        for (GObjectString* str : idnts) {
            writeRawString(str->val.c_str(), str->val.size());
        }
    }

    void writeConstants(std::vector<GValue> vals) {
        writeSizeT(vals.size());
        for (GValue val : vals) {
            writeValue(val);
        }
    }

    void writeDebugInfo(std::vector<int> lines) {
        writeSizeT(lines.size());
        for (int l : lines) {
            writeSizeT(l);
        }
    }

    void writeInstructions(std::vector<INSTRUCTION> insts) {
        writeSizeT(insts.size());
        for (INSTRUCTION i : insts) {
            writeInstruction(i);
        }
    }

    void writeChunk(GChunk* chk) {
        // write the identifiers
        writeIdentifiers(chk->identifiers);
        // write the constants
        writeConstants(chk->constants);
        // write debug info (line information)
        writeDebugInfo(chk->lineInfo);
        // and finally, write the instructions
        writeInstructions(chk->code);
    }

public:
    GDump(GObjectFunction* objFunc) {
        // write file magic
        data.write(GCODEC_HEADER_MAGIC, strlen(GCODEC_HEADER_MAGIC));
        // write codec version byte
        writeByte(GCODEC_VERSION_BYTE);
        // write our endian-ness
        writeByte(getBigEndian());

        // start at root GObjectFunction
        writeObject((GObject*)objFunc);
        
        // TODO: compute hash
        // TDOD: maybe compress data using lz77 ?

        // write a null-byte
        data.write("\0", 1);

        out = data.str();
    }

    void* getData() {
        return (void*)out.c_str();
    }

    size_t getSize() {
        return out.size();
    }
};

/* GUndump
    This class is in charge of deserializing output from GDump into a GObjectFunction* object :)
*/
class GUndump {
private:
    void* data;
    int dataSize;
    int offset = 0;
    bool panic = false;
    bool reverseEndian; // if we need to reverse the endian-ness of some datatypes (like uint32_t or doubles)

    GObjectFunction* root = NULL;

    bool getBigEndian() {
        union {
            uint32_t i;
            char bytes[4];
        } encodedi = {0xBEDBABE};

        return encodedi.bytes[0] == 0xBE;
    }

    void throwObjection(std::string str) {
        panic = true;
        std::cout << str << std::endl;
        // if we're debugging, just exit
        DEBUGLOG(
            exit(0);
        )
    }

    // copies [sz] bytes from data at offset to [buffer], also inc offset by [sz]
    inline void read(void* buffer, int sz, bool endianMatters = false) {
        // sanity check
        if (offset + sz >= dataSize) 
            return throwObjection("Malformed binary!");

        // copy [sz] bytes from data + offset to buffer
        memcpy(buffer, (uint8_t*)data + offset, sz);

        // now reverse buffer to fix endian-ness 
        if (endianMatters && reverseEndian) {
            uint8_t tmp;
            uint8_t* bufferBytes = (uint8_t*)buffer;
            
            for (int i = 0, z = sz-1; i < z; i++, z--) {
                tmp = bufferBytes[i];
                bufferBytes[i] = bufferBytes[z];
                bufferBytes[z] = tmp;
            }
        }

        offset += sz;
    }

    uint8_t readByte() {
        uint8_t tmp;
        read(&tmp, sizeof(uint8_t));
        return tmp;
    }

    uint32_t readSizeT() {
        uint32_t tmp;
        read(&tmp, sizeof(uint32_t));
        return tmp;
    }

    // TODO: These instructions are platform dependant to the original machine that compiled the script. Parse each instruction and read them in a platform independant way
    INSTRUCTION readInstruction() {
        INSTRUCTION tmp;
        read(&tmp, sizeof(INSTRUCTION));
        return tmp;
    }

    std::string readRawString() {
        // get size of string
        int size = readSizeT();

        // copies from data to string
        std::string tmp((const char*)((uint8_t*)data + offset), size);
        offset += size;

        // returns tmp
        return tmp;
    }

    GObject* readObject() {
        uint8_t otype = readByte();
        switch (otype) {
            case GOBJECT_NULL: 
                return new GObject();
            case GOBJECT_STRING: {
                std::string str = readRawString();
                return reinterpret_cast<GObject*>(Gavel::addString(str));
            }
            case GOBJECT_FUNCTION: {
                std::string name = readRawString(); // first the name
                int args = readSizeT(); // arg count
                int upvals = readSizeT(); // then the upvalues
                GChunk* chk = readChunk(); // and finally the chunk

                return reinterpret_cast<GObject*>(new GObjectFunction(chk, args, upvals, name));
            }
            default:
                return new GObject();
        }
    }

    GValue readValue() {
        // first read datatype
        uint8_t gtype = readByte();
        switch (gtype) {
            case GAVEL_TNIL:
                return CREATECONST_NIL();
            case GAVEL_TBOOLEAN:
                return CREATECONST_BOOL(readByte());
            case GAVEL_TNUMBER: {
                double num;
                read(&num, sizeof(double));
                return CREATECONST_NUMBER(num);
            }
            case GAVEL_TOBJ: {
                return GValue(readObject());
            }
            default:
                // we have no idea what it is, just return a nil for now
                return CREATECONST_NIL();
        }
    }

    std::vector<GObjectString*> readIdentifiers() {
        std::vector<GObjectString*> idnts;
        int size = readSizeT();

        std::string buf;
        for (int i = 0; i < size; i++) {
            buf = readRawString();
            idnts.push_back(Gavel::addString(buf));
        }

        return idnts;
    }

    std::vector<GValue> readConstants() {
        std::vector<GValue> consts;
        int size = readSizeT();

        for (int i = 0; i < size; i++) {
            consts.push_back(readValue());
        }

        return consts;
    }

    std::vector<int> readDebugInfo() {
        std::vector<int> lines;

        int size = readSizeT();
        for (int i = 0; i < size; i++) {
            lines.push_back(readSizeT());
        }

        return lines;
    }

    std::vector<INSTRUCTION> readInstructions() {
        std::vector<INSTRUCTION> insts;

        int size = readSizeT();
        for (int i = 0; i < size; i++) {
            insts.push_back(readInstruction());
        }

        return insts;
    }

    GChunk* readChunk() {
        GChunk* chk = Gavel::newChunk();
        // read the identifiers
        chk->identifiers = readIdentifiers();
        // read the constants
        chk->constants = readConstants();
        // read debug info (line information)
        chk->lineInfo = readDebugInfo();
        // and finally, read the instructions
        chk->code = readInstructions();

        return chk;
    }

public:
    GUndump(void* d, int ds): data(d), dataSize(ds) {
        // compare file magic
        int magicLen = strlen(GCODEC_HEADER_MAGIC);
        if (memcmp(data, GCODEC_HEADER_MAGIC, magicLen) != 0) {
            throwObjection("Wrong file type!");
            return;
        }
        offset += magicLen;

        // grab gcodec version
        uint8_t vers = readByte();
        // compare gcodec version
        if (vers != GCODEC_VERSION_BYTE) {
            throwObjection("Unsupported version of codec!");
            return;
        }

        // grab endian encoding && detect ours
        bool dataBigEndian = readByte();
        reverseEndian = dataBigEndian != getBigEndian();

        // read root
        GObject* funcObj = readObject();
        // sanity check
        if (funcObj->type != GOBJECT_FUNCTION) {
            throwObjection("Expected Function as root object!");
            return;
        }

        // set root!
        root = reinterpret_cast<GObjectFunction*>(funcObj);
    }

    GObjectFunction* getData() {
        return root;
    }
};

#undef GCODEC_VERSION_BYTE
#undef GCODEC_HEADER_MAGIC
#undef DEBUGLOG

#endif // hi there :)