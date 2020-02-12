/* 

     ██████╗  █████╗ ██╗   ██╗███████╗██╗     
    ██╔════╝ ██╔══██╗██║   ██║██╔════╝██║     
    ██║  ███╗███████║██║   ██║█████╗  ██║     
    ██║   ██║██╔══██║╚██╗ ██╔╝██╔══╝  ██║     
    ╚██████╔╝██║  ██║ ╚████╔╝ ███████╗███████╗
     ╚═════╝ ╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚══════╝
        Copyright (c) 2019-2020, Seth Stubbs
    
    Version 1.0
        - Complete rewrite
        - Better parser based off of a varient of a Pratt parser 
        - Better stack management
        - Better memory management
        - Closures, better scopes, etc.

    Any version of GavelScript prior to this was a necessary evil :(, it was a learning curve okay....
*/

#ifndef _GSTK_HPP
#define _GSTK_HPP

#include <iostream>
#include <iomanip>
#include <memory>
#include <type_traits>
#include <vector>
#include <map>
#include <unordered_map>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <math.h>

// add x to show debug info
#define DEBUGLOG(x) x

// version info
#define GAVEL_MAJOR "1"
#define GAVEL_MINOR "0"

// switched to 32bit instructions!
#define INSTRUCTION int

#define STACK_MAX 256
#define MAX_LOCALS STACK_MAX - 1

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
        - iAB
            - 'Opcode' : 6 bits
            - 'A' : 13 bits [MAX: 8192]
            - 'B' : 13 bits [MAX: 8192]
*/
#define SIZE_OP		        6
#define SIZE_Ax		        26
#define SIZE_A		        13
#define SIZE_B		        13

#define MAXREG_Ax           pow(2, SIZE_Ax)
#define MAXREG_A            pow(2, SIZE_A)
#define MAXREG_B            pow(2, SIZE_B)

#define POS_OP		        0
#define POS_A		        (POS_OP + SIZE_OP)
#define POS_B		        (POS_A + SIZE_A)

// creates a mask with `n' 1 bits
#define MASK(n)	            (~((~(INSTRUCTION)0)<<n))

#define GET_OPCODE(i)	    (((OPCODE)((i)>>POS_OP)) & MASK(SIZE_OP))
#define GETARG_Ax(i)	    (int)(((i)>>POS_A) & MASK(SIZE_Ax))
#define GETARG_A(i)	        (int)(((i)>>POS_A) & MASK(SIZE_A))
#define GETARG_B(i)	        (int)(((i)>>POS_B) & MASK(SIZE_B))

/* These will create bytecode instructions. (Look at OPCODE enum right below this for references)
    o: OpCode, eg. OP_POP
    a: Ax, A eg. 1
    b: B eg. 2
*/
#define CREATE_i(o)	        (((INSTRUCTION)(o))<<POS_OP)
#define CREATE_iAx(o,a)	    ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A))
#define CREATE_iAB(o,a,b)   ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A) | (((INSTRUCTION)(b))<<POS_B))

// ===========================================================================[[ VIRTUAL MACHINE ]]===========================================================================

typedef enum { // [MAX : 64] 
    //              ===============================[[STACK MANIPULATION]]===============================
    OP_LOADCONST,   // iAx - Loads chunk->const[Ax] and pushes the value onto the stack
    OP_POP,         // iAx - pops stack[top]*Ax
    OP_GETGLOBAL,   // iAx - Pushes global[chunk->identifiers[Ax]]
    OP_GETLOCAL,    // iAx - Pushes stack[top-Ax] to the stack
    OP_SETLOCAL,    // iAx - Sets stack[top-Ax] to stack[top] (after popping it of course)
    OP_SETGLOBAL,   // iAx - sets stack[top] to global[chunk->identifiers[Ax]]
    OP_CLOSURE,     // iAx - Makes a closure with Ax Upvalues
    OP_CLOSE,       // i   - Closes current closure.

    //              ==============================[[TABLES && METATABLES]]==============================

    //              ==================================[[CONDITIONALS]]==================================

    //              ===================================[[BITWISE OP]]===================================
    OP_NEGATE,      // i - Negates stack[top]
    OP_NOT,         // i - falsifies stack[top]
    OP_ADD,         // i - adds stack[top] to stack[top-1]
    OP_SUB,         // i - subs stack[top] from stack[top-1]
    OP_MUL,         // i - multiplies stack[top] with stack[top-1]
    OP_DIV,         // i - divides stack[top] with stack[top-1]
    OP_EQUAL,       // i - pushes (stack[top] == stack[top-1])
    OP_GREATER,     // i - pushes (stack[top] > stack[top-1])
    OP_LESS,        // i - pushes (stack[top] < stack[top-1])

    //              ====================================[[LITERALS]]====================================
    OP_TRUE,
    OP_FALSE,
    OP_NIL,

    //              ================================[[MISC INSTRUCTIONS]]===============================
    OP_RETURN,
    OP_END //           i   - marks the end of chunk
} OPCODE;

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
    GOBJECT_OBJECTION // holds objections
} GObjType;

struct GValue;

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

    GObject() {}
    virtual ~GObject() {}

    virtual bool equals(GObject*) { return false; };
    virtual std::string toString() { return ""; };
    virtual std::string toStringDataType() { return ""; };
    virtual GObject* clone() {return new GObject(); };
    virtual int getHash() {return std::hash<GObjType>()(type); };

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
};

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
                std::stringstream stream;
                stream << val.number;
                return stream.str();
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
#define CREATECONST_DOUBLE(n)   GValue((double)n)
#define CREATECONST_STRING(x)   GValue((GObject*)new GObjectString(x))
#define CREATECONST_OBJECTION(x)GValue((GObject*)new GObjectObjection(x))

#define READOBJECTVALUE(x, type) reinterpret_cast<type>(x)->val

#define READGVALUEBOOL(x)   x.val.boolean
#define READGVALUEDOUBLE(x) x.val.number
#define READGVALUESTRING(x) READOBJECTVALUE(x.val.obj, GObjectString*)
#define READGVALUEOBJECTION(x) READOBJECTVALUE(x.val.obj, GObjectObjection*);

#define ISGVALUEBOOL(x)     x.type == GAVEL_TBOOLEAN
#define ISGVALUEDOUBLE(x)   x.type == GAVEL_TNUMBER
#define ISGVALUENIL(x)      x.type == GAVEL_TNIL

#define ISGVALUEOBJ(x)      x.type == GAVEL_TOBJ

// treat this like a macro, this is to protect against macro expansion and causing undefined behavior :eyes:
inline bool ISGVALUEOBJTYPE(GValue v, GObjType t) {
    return ISGVALUEOBJ(v) && v.val.obj->type == t;
}

#define FREEGVALUEOBJ(x)    delete x.val.obj

/*  GTable
        This is GavelScript's custom hashtable implementation. Originally I used std::map, however string loopup for those values were INCREDIBLY slow. What makes this hash table different is a 
    technique called "String Interning", instead of comparing the characters for each string, we compare the address. We do this by grouping alike values to the same memory address so that we can 
    do fast == comparisons during runtime and not compare the true memory values.
*/
template<typename T>
class GTable {
private:
    struct Entry {
        T key; // dynamically allocated, also may point to the same string due to string interning

        Entry(T k):
            key(k) {}

        bool operator == (const Entry& other) const {
            return key == other.key;
        }
    };

    struct hash_fn {
        std::size_t operator() (Entry v) const {
            return v.key->getHash();
        }
    };

    std::unordered_map<Entry, GValue, hash_fn> hashTable;

public:
    GTable() {}

    T findExistingKey(T key) {
        for (std::pair<Entry, GValue> pair : hashTable) {
            if (pair.first.key->equals(key))
                return pair.first.key;
        }

        return NULL;
    }

    bool checkValidKey(T key) {
        return hashTable.find(key) != hashTable.end();
    }
    
    GValue getIndex(T key) {
        // if key exists, return it
        if (checkValidKey(key)) {
            return hashTable[key];
        }

        // otherwise return NIL
        return CREATECONST_NIL();
    }

    void setIndex(T key, GValue value) {
        hashTable[key] = value;
    }

    std::vector<T> getVectorOfKeys() {
        std::vector<T> keys;
        for (std::pair<Entry, GValue> pair : hashTable) {
            keys.push_back(pair.first.key);
        }
        return keys;
    }

    void printTable() {
        for (std::pair<Entry, GValue> pair : hashTable) {
            std::cout << pair.first.key->toString() << " : " << pair.second.toString() << std::endl;
        }
    }
};

// defines a chunk. each chunk has locals
struct GChunk {
    std::string name; // for debug purposes
    std::vector<INSTRUCTION> code;
    std::vector<GValue> constants;
    std::vector<GObjectString*> identifiers;
    std::vector<int> lineInfo;

    // default constructor
    GChunk() {}

    ~GChunk() {
        // frees all of the constants
        for (GValue c : constants) {
            if (ISGVALUEOBJ(c)) {
                FREEGVALUEOBJ(c);
            }
        }
        // free all of the identifiers
        for (GObjectString* id : identifiers) {
            delete id;
        }
    }

    int addInstruction(INSTRUCTION i, int line) {
        // add INSTRUCTION to our instruction table
        code.push_back(i);
        lineInfo.push_back(line);
        return code.size() - 1;
    }

    int addIdentifier(std::string id) {
        int tmpId = findIdentifier(id);
        if (tmpId != -1)
            return tmpId;

        identifiers.push_back(new GObjectString(id));
        return identifiers.size() - 1;
    }

    int findIdentifier(std::string id) {
        for (int i = 0; i < identifiers.size(); i++) {
            if (identifiers[i]->val.compare(id) == 0)
                return i;
        }

        return -1; // identifier doesn't exist
    }

    int addConstant(GValue c) {
        // check if we already have an identical constant in our constant table, if so return the index
        for (int i  = 0; i < constants.size(); i++) {
            GValue oc = constants[i];
            if (oc.equals(c)) {
                if (ISGVALUEOBJ(c)) { // free unused object
                    FREEGVALUEOBJ(c);
                }
                return i;
            }
        }

        // else, add it to the constant table and return the new index!!
        constants.push_back(c);
        return constants.size() - 1;
    }
};

struct GClosure {
    std::vector<GObject*> garbage; // holds pointers to our dynamically-generated GObjects (so like strings and such)
    GChunk* function;

    GClosure* parent; // GClosures can inherit eachother, sharing upvalues has never been so easy!
};

/* GStack
    Stack for GState. I would've just used std::stack, but it annoyingly hides the container from us in it's protected members :/
*/
class GStack {
private:
    GValue container[STACK_MAX];
    GValue* top;
public:
    GStack() {
        top = container;
    }

    int push(GValue v) {
        *(top++) = v;
        return top - container; // returns the top index
    }

    GValue pop() {
        return *(--top); // returns old top value
    }

    GValue getTop(int i) {
        return *(top - i); // returns offset of stack
    }

    void setTop(int i, GValue v) {
        *(top - i) = v;
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
    GChunk* currentChunk;
    std::vector<GObject*> garbage;
    GTable<GObjectString*> strings;
    GTable<GObjectString*> globals;
    INSTRUCTION* pc = NULL;
    GStateStatus status = GSTATE_OK;

    // determins falsey-ness
    bool isFalsey(GValue v) {
        return ISGVALUENIL(v) || (ISGVALUEBOOL(v) && !READGVALUEBOOL(v));
    }

    GObjectString* addString(std::string str) {
        GObjectString* newStr = ((GObjectString*)CREATECONST_STRING(str).val.obj);
        GObjectString* key = strings.findExistingKey(newStr);
        if (key == NULL) {
            strings.setIndex(newStr, CREATECONST_NIL());
            return newStr;
        }

        delete newStr;
        return key;
    }
    
public:
    GStack stack;
    GState() {}
    ~GState() {
        cleanGarbage();

        for (GObjectString* str : strings.getVectorOfKeys()) {
            delete str;
        }
    }

    void cleanGarbage() { 
        for (GObject* g : garbage) {
            delete g;
        }
        garbage.clear();
    }

    void addGarbage(GObject* g) { // for values generated dynamically, add it to our garbage to be cleaned up!
        garbage.push_back(g);
    }

    /* newGValue(<t> value) - Helpful function to auto-turn some basic datatypes into a GValue for ease of embeddability
        - value : value to turn into a GValue
        returns : GValue
    */
    template <typename T>
    inline GValue newGValue(T x) {
        if constexpr (std::is_same<T, double>() || std::is_same<T, int>())
            return CREATECONST_DOUBLE(x);
        else if constexpr (std::is_same<T, bool>())
            return CREATECONST_BOOL(x);
        else if constexpr (std::is_same<T, char*>() || std::is_same<T, const char*>() || std::is_same<T, std::string>()) {
            GValue obj = CREATECONST_STRING(x);
            addGarbage(obj.val.obj);
            return obj;
        }
        else if constexpr (std::is_same<T, GValue*>())
            return x->clone();

        return CREATECONST_NIL();
    }

    void throwObjection(std::string err) {
        std::cout << err << std::endl;
        status = GSTATE_RUNTIME_OBJECTION;

        GValue obj = CREATECONST_OBJECTION(GObjection(err, currentChunk->lineInfo[pc - &currentChunk->code[0]]));
        addGarbage(obj.val.obj);
        
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

    GStateStatus runChunk(GChunk* chunk) {
        bool chunkEnd = false;

        currentChunk = chunk; // sets currentChunk to our currently-executing chunk
        pc = &chunk->code[0]; // sets program counter

        // load identifiers, make sure string interning works properly. without this another chunk's identifiers would point to completely different addresses.
        std::vector<GObjectString*> pseudoIdentifiers;
        for (GObjectString* str: chunk->identifiers) {
            GObjectString* key = strings.findExistingKey(str);
            if (key == NULL) {
                GObjectString* newStr = (GObjectString*)str->clone();
                strings.setIndex(newStr, CREATECONST_NIL());
                pseudoIdentifiers.push_back(newStr); // adds to pseudoIdentifiers list
            } else {
                pseudoIdentifiers.push_back(key); // it already exists :)
            }
        }
        
        while(!chunkEnd && status == GSTATE_OK) 
        {
            INSTRUCTION inst = *(pc)++; // gets current executing instruction and increment
            DEBUGLOG(std::cout << "OP: " << GET_OPCODE(inst) << std::endl);
            switch(GET_OPCODE(inst))
            {   
                case OP_LOADCONST: { // iAx -- loads chunk->consts[Ax] onto the stack
                    DEBUGLOG(std::cout << "loading constant " << chunk->constants[GETARG_Ax(inst)].toString() << std::endl);
                    stack.push(chunk->constants[GETARG_Ax(inst)]);
                    break;
                }
                case OP_GETGLOBAL: {
                    stack.push(globals.getIndex(pseudoIdentifiers[GETARG_Ax(inst)]));
                    break;
                }
                case OP_SETGLOBAL: {
                    GValue newVal = stack.pop();
                    globals.setIndex(pseudoIdentifiers[GETARG_Ax(inst)], newVal);
                    break;
                }
                case OP_GETLOCAL: {
                    int indx = GETARG_Ax(inst);
                    GValue local = stack.getTop(indx);
                    DEBUGLOG(std::cout << "getting local at stack[top-" << indx << "] : " << local.toString() << std::endl);
                    stack.push(local);
                    break;
                }
                case OP_SETLOCAL: {
                    int indx = GETARG_Ax(inst);
                    GValue val = stack.pop(); // gets value off of stack
                    DEBUGLOG(std::cout << "setting local at stack[top-" << indx << "] to " << val.toString() << std::endl);
                    stack.setTop(indx, val);
                    break;
                }
                case OP_POP: {
                    int ax = GETARG_Ax(inst);
                    for (int i = 0; i < ax; i++) {
                        DEBUGLOG(std::cout << "popping stack[top]" << std::endl);
                        stack.pop(); // pops whatever is on the stack * Ax
                    }
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
                case OP_CLOSE: { // i -- closes current closure
                    // TODO: clean upvals
                    cleanGarbage(); // cleans garbage
                    break;
                }
                case OP_NEGATE: {
                    GValue val = stack.pop();
                    if (val.type != GAVEL_TNUMBER){
                        throwObjection("Cannot negate non-number value " + val.toStringDataType() + "!");
                        break;
                    }
                    stack.push(CREATECONST_DOUBLE(-val.val.number));
                    break;
                }
                case OP_NOT: {
                    stack.push(isFalsey(stack.pop()));
                    break;
                }
                case OP_ADD: { 
                    GValue n1 = stack.pop();
                    GValue n2 = stack.pop();
                    if (ISGVALUEOBJTYPE(n1, GOBJECT_STRING) && ISGVALUEOBJTYPE(n2, GOBJECT_STRING)) {
                        // concatinate the strings
                        GValue newStr = GValue((GObject*)addString(READGVALUESTRING(n2) + READGVALUESTRING(n1))); // automagically adds it to our garbage
                        stack.push(newStr);
                    } else if (ISGVALUEDOUBLE(n1) && ISGVALUEDOUBLE(n2)) {
                        // pushes to the stack
                        stack.push(CREATECONST_DOUBLE(READGVALUEDOUBLE(n1) + READGVALUEDOUBLE(n2)));
                    } else {
                        throwObjection("Cannot perform arithmetic on " + n1.toStringDataType() + " and " + n2.toStringDataType());
                    }
                    break;
                }
                case OP_SUB: { BINARY_OP(-); break; }
                case OP_MUL: { BINARY_OP(*); break; }
                case OP_DIV: { BINARY_OP(/); break; }
                case OP_LESS: { BINARY_OP(>); break; }
                case OP_GREATER: { BINARY_OP(<); break; }
                case OP_EQUAL: {
                    GValue n1 = stack.pop();
                    GValue n2 = stack.pop();
                    stack.push(n1.equals(n2)); // push result
                    break;
                }
                case OP_END: { // i
                    globals.printTable();
                    return status;
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

// ===========================================================================[[ COMPILER/LEXER ]]===========================================================================

typedef enum {
    // single character tokens
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_STAR,
    TOKEN_SLASH, 
    TOKEN_DOT,
    TOKEN_BANG,
    TOKEN_OPEN_PAREN, 
    TOKEN_CLOSE_PAREN, 
    TOKEN_OPEN_BRACE, 
    TOKEN_CLOSE_BRACE,
    TOKEN_OPEN_BRACKET, 
    TOKEN_CLOSE_BRACKET,

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

    // variables/constants
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NIL,

    // keywords
    TOKEN_END,
    TOKEN_DO,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_THEN,
    TOKEN_FOR,
    TOKEN_FUNCTION,
    TOKEN_LOCAL,
    
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
    PREC_CALL,        // . ()     
    PREC_PRIMARY                  
} Precedence;

typedef enum {
    PARSEFIX_NUMBER,
    PARSEFIX_STRING,
    PARSEFIX_BINARY,
    PARSEFIX_LITERAL,
    PARSEFIX_VAR,
    PARSEFIX_UNARY,
    PARSEFIX_GROUPING,
    PARSEFIX_LOCAL,
    PARSEFIX_NONE,
    PARSEFIX_SKIP,
    PARSEFIX_ENDPARSE
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
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_DOT
    {PARSEFIX_UNARY,     PARSEFIX_NONE,      PREC_NONE},    // TOKEN_BANG
    {PARSEFIX_GROUPING, PARSEFIX_NONE,      PREC_NONE},     // TOKEN_OPEN_PAREN 
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_CLOSE_PAREN 
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_OPEN_BRACE 
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_CLOSE_BRACE
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_OPEN_BRACKET 
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_CLOSE_BRACKET

    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_COMPARISON},// TOKEN_LESS
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_COMPARISON},// TOKEN_GREATER
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_EQUALITY}, // TOKEN_EQUAL_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_COMPARISON},// TOKEN_LESS_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_COMPARISON},// TOKEN_GREATER_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_BINARY,    PREC_EQUALITY}, // TOKEN_BANG_EQUAL
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_OR
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_AND

    {PARSEFIX_VAR,      PARSEFIX_NONE,      PREC_NONE},     // TOKEN_IDENTIFIER
    {PARSEFIX_STRING,   PARSEFIX_NONE,      PREC_NONE},     // TOKEN_STRING
    {PARSEFIX_NUMBER,   PARSEFIX_NONE,      PREC_NONE},     // TOKEN_NUMBER
    {PARSEFIX_LITERAL,  PARSEFIX_NONE,      PREC_NONE},     // TOKEN_TRUE
    {PARSEFIX_LITERAL,  PARSEFIX_NONE,      PREC_NONE},     // TOKEN_FALSE
    {PARSEFIX_LITERAL,  PARSEFIX_NONE,      PREC_NONE},     // TOKEN_NIL

    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_END
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_DO
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_IF
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_ELSE
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_WHILE
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_THEN
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_FOR
    {PARSEFIX_NONE,     PARSEFIX_NONE,      PREC_NONE},     // TOKEN_FUNCTION
    {PARSEFIX_VAR,      PARSEFIX_NONE,      PREC_NONE},     // TOKEN_LOCAL

    {PARSEFIX_SKIP,     PARSEFIX_SKIP,      PREC_NONE},     // TOKEN_EOS 
    {PARSEFIX_ENDPARSE, PARSEFIX_ENDPARSE,  PREC_NONE},     // TOKEN_EOF 
    {PARSEFIX_ENDPARSE, PARSEFIX_ENDPARSE,  PREC_NONE}      // TOKEN_ERROR 
};

typedef enum {
    PARSER_STATUS_OK,
    PARSER_STATUS_OBJECTION
} GParserStatus;

class GavelParser {
private:
    GChunk* chunk = NULL;
    GObjection objection;

    const char* script;
    const char* currentChar;
    size_t scriptSize;

    bool panic = false;
    bool quitParse = false;
    int line = 1;
    int openBraces = 0;

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
    };

    Local locals[MAX_LOCALS]; // hold our locals
    int localCount = 0;
    int scopeDepth = 0; // current scope depth

    // this holds our hashtable for a quick and easy lookup if we have a reserved word, and get it's corrosponding GTokenType
    std::map<std::string, GTokenType> reservedKeywords = {

        // control flow stuff
        {"if",      TOKEN_IF},
        {"else",    TOKEN_ELSE},
        {"while",   TOKEN_WHILE},
        {"then",    TOKEN_THEN},
        {"do",      TOKEN_DO},
        {"end",     TOKEN_END},
        
        {"and",     TOKEN_AND},
        {"or",      TOKEN_OR},

        // literals
        {"true",    TOKEN_TRUE},
        {"false",   TOKEN_FALSE},
        {"nil",     TOKEN_NIL},

        // scope definitions
        {"local",   TOKEN_LOCAL},

        {"for",     TOKEN_FOR},
        {"function",TOKEN_FUNCTION}
    };

    void throwObjection(std::string e) {
        if (panic)
            return;

        panic = true;
        objection = GObjection(e, line);
    }

// =================================================================== [[Functions for tokenizing]] ====================================================================

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


        locals[localCount] = {id, -1}; // adds new local in an "uninitalized" state
        return localCount++;
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
            localsToPop++;
            localCount--;
        }

        // pops the locals :)
        emitInstruction(CREATE_iAx(OP_POP, localsToPop));
    }

// =================================================================== [[Functions for tokenizing]] ====================================================================

    // if we've parsed the whole script
    inline bool isEnd() {
        return (currentChar - script) > scriptSize;
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
            case '.': // decimal numbers
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
            str += advanceChar();
        }

        advanceChar();

        if (isEnd())
            return Token(TOKEN_ERROR, "Unterminated string!");

        return Token(TOKEN_STRING, str);
    }

    Token readNumber() {
        std::string str;

        while (isNumeric(peekChar()) && !isEnd()) {
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
            case '+': return Token(TOKEN_PLUS); 
            case '-': return Token(TOKEN_MINUS); 
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
            case '\0': return Token(TOKEN_EOF); // we just consumed the null-terminator. get out NOW aaaAAAAAA
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
        DEBUGLOG(std::cout << "Token\t("<< currentToken.type << ") : " << currentToken.str << std::endl);
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

    inline int emitInstruction(INSTRUCTION i) {
        return chunk->addInstruction(i, line);
    }

    inline ParseRule getRule(GTokenType t) {
        return GavelParserRules[t];
    }

    /* emitPUSHCONST(GValue c)
        adds constant to the chunk constant list, then adds the OP_LOADCONST instruction
    */
    int emitPUSHCONST(GValue c) {
        return emitInstruction(CREATE_iAx(OP_LOADCONST, chunk->addConstant(c)));
    }

    int emitRETURN() {
        return emitInstruction(CREATE_i(OP_RETURN));
    }

    void consumeToken(GTokenType expectedType, std::string errStr) {
        if (getCurrentToken().type == expectedType) {
            getNextToken(); // advance to the next token
            return;
        }

        throwObjection(errStr);
    }

    void namedVariable(std::string id, bool canAssign, bool newLocalOverride = false) {
        if (newLocalOverride) {
            if (canAssign && matchToken(TOKEN_EQUAL) && !checkToken(TOKEN_EOS)) {
                DEBUGLOG(std::cout << "reading expression" << std::endl);
                expression(); // pushes new local onto the stack
                DEBUGLOG(std::cout << "done reading expression" << std::endl);
            } else if (canAssign) {
                throwObjection("Assignemnt expected after local definition!");
            }
            return;
        }

        int getOp, setOp;
        int indx = findLocal(id);
        if (indx != -1) {
            // found the local :flushed:
            indx = (localCount - indx);
            getOp = OP_GETLOCAL;
            setOp = OP_SETLOCAL;
        } else {
            // didn't find the local, default to global
            indx = chunk->addIdentifier(id);
            getOp = OP_GETGLOBAL;
            setOp = OP_SETGLOBAL;
        }
        
        if (canAssign && matchToken(TOKEN_EQUAL)) {            
            expression();  
            emitInstruction(CREATE_iAx(setOp, indx));
        } else {            
            emitInstruction(CREATE_iAx(getOp, indx));
        }
    }

    // read rule and decide how to parse the token.
    void runParseFix(Token token, ParseFix rule, bool canAssign) {
        switch (rule) {
            case PARSEFIX_BINARY:   binaryOp(token); break;
            case PARSEFIX_UNARY:    unaryOp(token); break;
            case PARSEFIX_NUMBER: {
                DEBUGLOG(std::cout << "NUMBER CONSTANT: " << token.str << std::endl);
                double num = std::stod(previousToken.str.c_str());
                emitPUSHCONST(CREATECONST_DOUBLE(num));
                break;
            }
            case PARSEFIX_STRING: { // emits the string :))))
                emitPUSHCONST(CREATECONST_STRING(token.str));
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
            case PARSEFIX_LITERAL: {
                switch (token.type) {
                    case TOKEN_TRUE:    emitInstruction(CREATE_i(OP_TRUE)); break;
                    case TOKEN_FALSE:   emitInstruction(CREATE_i(OP_FALSE)); break;
                    case TOKEN_NIL:     emitInstruction(CREATE_i(OP_NIL)); break;
                    default:
                        break; // shouldn't ever happen but /shrug
                }
                break;
            }
            case PARSEFIX_VAR: {
                if (token.type == TOKEN_LOCAL) {
                    DEBUGLOG(std::cout << "LOCAL TOKEN CONSUMED" << std::endl);
                    // okay so local was consumed, but now we expect an identifier
                    if (matchToken(TOKEN_IDENTIFIER)) {
                        declareLocal(previousToken.str);
                        namedVariable(previousToken.str, canAssign, true); // forcedNewLocal :)
                        markLocalInitalized(); // allows our parser to use it :)
                    } else {
                        throwObjection("Identifier expected after 'local'");
                    }
                } else {
                    namedVariable(token.str, canAssign);
                }
                break;
            }
            case PARSEFIX_SKIP:
            case PARSEFIX_ENDPARSE:
                break;
            default:
                throwObjection("Illegal syntax!");
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
        while (!checkToken(TOKEN_END) && !checkToken(TOKEN_EOF)) {
            declaration();
        }

        consumeToken(TOKEN_END, "Expected 'end' to close scope");
    }

    // parses a single expression
    void expression() {
        parsePrecedence(PREC_ASSIGNMENT); // parses until assignement is reached :eyes:
    }

    void statement() {
        if (matchToken(TOKEN_DO)) {
            beginScope();
            block();
            endScope();
        } else {
            expression();
        }
    }

    void declaration() {
        statement();
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
    }

public:
    GavelParser() {}
    GavelParser(const char* s): 
        script(s), currentChar(s) {
        scriptSize = strlen(s);
        chunk = new GChunk();
    }

    GObjection getObjection() {
        return objection;
    }

// ======================================================== [[Public utility functions]] ========================================================

    bool compile() {
        getNextToken();
        while (!matchToken(TOKEN_EOF)) { // keep parsing till the end of the file 
            declaration();
        }
        chunk->addInstruction(CREATE_i(OP_END), line);
        return !panic;
    }

    GChunk* getChunk() {
        return chunk;
    }
};

#endif // hi there :)