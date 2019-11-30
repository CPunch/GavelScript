/* 

     ██████╗  █████╗ ██╗   ██╗███████╗██╗     
    ██╔════╝ ██╔══██╗██║   ██║██╔════╝██║     
    ██║  ███╗███████║██║   ██║█████╗  ██║     
    ██║   ██║██╔══██║╚██╗ ██╔╝██╔══╝  ██║     
    ╚██████╔╝██║  ██║ ╚████╔╝ ███████╗███████╗
     ╚═════╝ ╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚══════╝
            By: Seth Stubbs (CPunch)

Register-Based VM, Inspired by the Lua Source project :) 
    - Each instrcution is encoded in a 16bit integer.
    - Stack-based VM with max 16 instructions (9 currently used)
    - dynamically-typed
    - custom lexer & parser
    - user-definable c functionc
    - like 0 error handling so goodluck debugging your scripts LOL
    - and of course, free and open source!
*/

#ifndef _GSTK_HPP
#define _GSTK_HPP

#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <math.h>

// add x to show debug info
#define DEBUGLOG(x) 

#define GAVELSYNTAX_COMMENTSTART    '/'
#define GAVELSYNTAX_ASSIGNMENT      '='
#define GAVELSYNTAX_OPENSCOPE       '{'
#define GAVELSYNTAX_ENDSCOPE        '}'
#define GAVELSYNTAX_OPENCALL        '('
#define GAVELSYNTAX_ENDCALL         ')'
#define GAVELSYNTAX_STRING          '\"'
#define GAVELSYNTAX_SEPARATOR       ','
#define GAVELSYNTAX_ENDOFLINE       ';'
#define GAVELSYNTAX_IFCASE          "if"

#define GAVELSYNTAX_BOOLOPEQUALS    "=="
#define GAVELSYNTAX_BOOLOPLESS      '<'
#define GAVELSYNTAX_BOOLOPMORE      '>'

#define GAVELSYNTAX_BOOLTRUE        "true"
#define GAVELSYNTAX_BOOLFALSE       "false"

// 16bit instructions
#define INSTRUCTION unsigned short int

#define STACK_MAX 256

/* 
    Instructions & bitwise operations to get registers

        16 possible opcodes due to it being 4 bits. 9 currently used. Originally used positional arguments in the instructions for stack related operations, however that limited the stack size &
    made the GavelCompiler needlessly complicated :(. This is an exeperimental project and i 100% expect it to crash randomly. please don't use this until i release a stable version haha. This was
    also a project I made for a blog post about creating a scripting language. This has become overly-compilcated so I'll either have to break the post into a bunch of parts, or just showcase it and 
    maybe highlight some key features. IDK, I'll figure it out.

    Instructions are 16bit integers with everything encoded in it, currently there are 2 types of intructions:
        - i
            - 'Opcode' : 4 bits
            - 'Reserved space' : 12 bits
        - iAx
            - 'Opcode' : 4 bits
            - 'Ax' : 12 bits
*/
#define SIZE_OP		    4
#define SIZE_A          6
#define SIZE_Ax		    12

#define POS_OP		    0
#define POS_A		    (POS_OP + SIZE_OP)

// creates a mask with `n' 1 bits
#define MASK(n)	            (~((~(INSTRUCTION)0)<<n))

#define GET_OPCODE(i)	    (((OPCODE)((i)>>POS_OP)) & MASK(SIZE_OP))
#define GETARG_Ax(i)	    (int)(((i)>>POS_A) & MASK(SIZE_Ax))

#define CREATE_i(o)	        (((INSTRUCTION)(o))<<POS_OP)
#define CREATE_iAx(o,a)	    ((((INSTRUCTION)(o))<<POS_OP) | (((INSTRUCTION)(a))<<POS_A))

// ===========================================================================[[ VIRTUAL MACHINE ]]===========================================================================

typedef enum { // [MAX : 16] [CURRENT : 9]
    //              ===============================[[STACK MANIPULATION]]===============================
    OP_PUSHVALUE, //    iAx - pushes consts[Ax] onto the stack
    OP_POP, //          iAx - pops Ax values off the stack
    OP_GETVAR, //       i   - pushes vars[(string)stack[top]] onto the stack
    OP_SETVAR, //       i   - sets vars[stack[top - 1]] to stack[top] & calls OP_POP[2]
    OP_CALL, //         iAx - Ax is the number of arguments to be passed to stack[top - Ax - 1] (arguments + chunk is popped off of stack when returning!)

    //              ==================================[[CONDITIONALS]]==================================
    OP_BOOLOP, //       iAx - tests stack[top] with stack[top - 1] then pushes result as a boolean onto the stack. Ax is the type of comparison to be made (BOOLOP)
    OP_TEST, //         i   - if stack[top] is true, executes next instruct, otherwise pc is jumped by 2

    //              ===================================[[ARITHMETIC]]===================================
    OP_ARITH, //        iAx - does arithmatic with stack[top] to stack[top - 1]. Ax is the type of arithmatic to do (OPARITH). Result is pushed onto the stack

    //              ================================[[MISC INSTRUCTIONS]]===============================
    OP_END //           i   - marks end of chunk
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
    TOKEN_COMMENT,
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
    TOKEN_BOOLOP,
    TOKEN_ENDOFLINE,
    TOKEN_ENDOFFILE
} TOKENTYPE;

typedef enum {
    GAVELSTATE_RESUME,
    GAVELSTATE_YIELD,
    GAVELSTATE_END
} GAVELSTATE;

// type info for everything thats allowed on the stack
typedef enum {
    GAVEL_TNULL,
    GAVEL_TCHUNK, // information for a gavel chunk.
    GAVEL_TCFUNC, // lets us assign stuff like "print" in the VM to c++ functions
    GAVEL_TSTRING,
    GAVEL_TDOUBLE, // double
    GAVEL_TBOOLEAN // bool
} GAVEL_TYPE;

struct GValue;

// compares the null-terminated string it refernces and not the pointer
struct cmp_str
{
    bool operator()(char const *a, char const *b) const
    {
        return strcmp(a, b) < 0;
    }
};

// defines a chunk. each chunk has locals
struct _gchunk {
    INSTRUCTION* chunk;
    GValue* consts;
    _gchunk* parent;
    std::map<char*, GValue, cmp_str> locals;
    _gchunk()                               { parent = NULL; }
    _gchunk(INSTRUCTION* c)                 { chunk = c; parent = NULL; }
    _gchunk(INSTRUCTION* c, GValue* cons)   { chunk = c; consts = cons; parent = NULL; }
};

class GState;

typedef GValue (*GAVELCFUNC)(GState*, int);

union _gvalue {
    int n;
    _gchunk* i;
    GAVELCFUNC cfunc; // GState : state it called from, 
    char* str;
    double d;
    bool b;
    _gvalue() {}
    _gvalue(_gchunk* t)     { i = t;}
    _gvalue(GAVELCFUNC c)   { cfunc = c;}
    _gvalue(char* t)        { str = t;}
    _gvalue(double t)       { d = t;}
    _gvalue(bool t)         { b = t;}
};

/* GValue : Gavel Value
    [_gvalue] : 8 bytes -- holds raw value
    [unsigned char] : 1 byte -- holds type information
*/
struct GValue {
    _gvalue value;
    unsigned char type; // type info
    GValue() {}
    GValue(_gvalue g, unsigned char t) {
        value = g;
        type = t;
    }
    // so we can easily compare GValues
    bool operator==(const GValue& other)
    {
        if (other.type == type) {
            switch (type)
            {
                case GAVEL_TCHUNK:
                    return value.i == other.value.i;
                case GAVEL_TSTRING:
                    return strcmp(value.str, other.value.str) == 0;
                case GAVEL_TDOUBLE:
                    return value.d == other.value.d;
                case GAVEL_TBOOLEAN:
                    return value.b == other.value.b;
                default:
                    break;
            }
        }
        return false;
    }

    bool operator<(const GValue& other) {
        if (other.type == type) {
            switch (type) 
            {
                case GAVEL_TDOUBLE:
                    return value.d < other.value.d;
                default:
                    break;
            }
        }
        return false;
    }

    bool operator>(const GValue& other) {
        if (other.type == type) {
            switch (type) 
            {
                case GAVEL_TDOUBLE:
                    return value.d > other.value.d;
                default:
                    break;
            }
        }
        return false;
    }

    bool operator<=(const GValue& other) {
        if (other.type == type) {
            switch (type) 
            {
                case GAVEL_TDOUBLE:
                    return value.d <= other.value.d;
                default:
                    break;
            }
        }
        return false;
    }

    bool operator>=(const GValue& other) {
        if (other.type == type) {
            switch (type) 
            {
                case GAVEL_TDOUBLE:
                    return value.d >= other.value.d;
                default:
                    break;
            }
        }
        return false;
    }

    std::string toString() {
        switch (type)
        {
            case GAVEL_TCHUNK: {
                std::stringstream stream;
                stream << "Gavel chunk : " << value.i;
                return stream.str();
            }
            case GAVEL_TCFUNC: {
                std::stringstream stream;
                stream << "Gavel C Function : " << reinterpret_cast<int*>(value.cfunc);
                return stream.str();
            }
            case GAVEL_TSTRING:
                return value.str;
            case GAVEL_TDOUBLE: {
                std::stringstream stream;
                stream << value.d;
                return stream.str();
            }
            case GAVEL_TBOOLEAN:
                return value.b ? "true" : "false";
            default:
                return "NULL";
        }
    }
};

#define CREATECONST_CFUNC(n)    GValue(_gvalue((GAVELCFUNC)n), GAVEL_TCFUNC)
#define CREATECONST_CHUNK(n)    GValue(_gvalue((_gchunk*)n), GAVEL_TCHUNK)
#define CREATECONST_NULL()      GValue(_gvalue(), GAVEL_TNULL)
#define CREATECONST_STRING(n)   GValue(_gvalue((char*)n), GAVEL_TSTRING)
#define CREATECONST_DOUBLE(n)   GValue(_gvalue((double)n), GAVEL_TDOUBLE)
#define CREATECONST_BOOL(n)     GValue(_gvalue((bool)n), GAVEL_TBOOLEAN)

/* GStack
    Stack for GState. I would've just used std::stack, but it annoyingly hides the container from us in it's protected members :/
*/
class GStack {
private:
    GValue* container;
    int size;
    int top;

    bool isEmpty() {
        return top == -1;
    }

    bool isFull() {
        return top >= (size-1);
    }

public:
    GStack(int s = STACK_MAX) {
        container = new GValue[s];
        size = s;
        top = -1;
    }
    ~GStack() {
        delete container;
    }

    GValue* pop(int times = 1) {
        if (isEmpty()) {
            return NULL;
        }
        for (int i = 0; i < times; i ++)
            top--;
        return &container[top];
    }

    int push(GValue t) {
        if (isFull()) {
            return -1;
        }

        container[++top] = t;
    }

    GValue* getTop(int offset = 0) {
        if (isEmpty()) {
            // TODO: OBJECTION
            return NULL;
        }

        if (top - offset < 0) {
            return NULL;
        }
        return &container[top - offset];
    }

    bool setTop(GValue* g, int offset = 0) {
         if (isEmpty()) {
            // TODO: OBJECTION
            return false;
        }

        if (top - offset < 0) {
            return false;
        }

        container[top - offset] = *g;
        return true;
    }

    int getSize() {
        return top;
    }

    // DEBUG function to print the stack and label everything !! 
    void printStack() {
        std::cout << "\n===[[Stack Dump]]===" << std::endl;
        for (int i = 0; i <= top; ++i) {
            std::cout << std::setw(4) << i << " - " << container[i].toString() << std::endl; 
        }
        std::cout << "\n====================" << std::endl;
    }
};

/* GChunk
    This holds some basic functions for _gchunks
*/
namespace GChunk {
    void setVar(_gchunk* c, char* key, GValue* var) {
        c->locals[key] = *var;
    }

    GValue getVar(_gchunk* c, char* key) {
        _gchunk* currentChunk = c;

        while (currentChunk != NULL) {
            if (currentChunk->locals.find(key) != currentChunk->locals.end()) {
                return currentChunk->locals[key];
            }
            //std::cout << "help" << std::endl;
            currentChunk = currentChunk->parent;
        }

        // var doesnt exist
        return CREATECONST_NULL();
    }
}

/* GState 
    This holds the stack, constants, global vars, and holds methods to easily modify them.
*/
class GState {
public:
    GStack stack;
    GAVELSTATE state;
    INSTRUCTION* pc;
    GStack callStack; // should only hold TADDRESS or TCFUNC, but it technically hold GValue so they can be whatever :/ pls don't do that

    GState() {
        state = GAVELSTATE_RESUME;
    }

    GValue pushCFunction(GAVELCFUNC f) {
        GValue temp = CREATECONST_CFUNC(f);
        stack.push(temp);

        return temp;
    }

    GValue pushString(char* str) {
        GValue temp = CREATECONST_STRING(str);
        stack.push(temp);

        return temp;
    }

    GValue pushDouble(double num) {
        GValue temp = CREATECONST_DOUBLE(num);
        stack.push(temp);

        return temp;
    }

    GValue pushBool(bool b) {
        GValue temp = CREATECONST_BOOL(b);
        stack.push(temp);

        return temp;
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
            return t->value.d;
        }
        else {
            // TODO: OBJECTION
            return 0;
        }
    }

    bool toBool(int i = 0) {
        GValue* t = getTop(i);
        if (t->type == GAVEL_TBOOLEAN) {
            return t->value.b;
        }
        else {
            // TODO: OBJECTION
            return false;
        }
    }

    char* toString(int i = 0) {
        GValue* t = getTop(i);
        if (t->type == GAVEL_TSTRING) {
            return t->value.str;
        }
        else {
            // TODO: OBJECTION
            return "";
        }
    }

    void throwObjection(std::string error) {
        std::cout << "OBJECTION! : " << error << std::endl;

        // pauses state
        state = GAVELSTATE_END;
    }
};

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
    state->stack.pop(2); \
    if (_t->type == _t2->type) { \
        switch (_t->type) { \
            case GAVEL_TDOUBLE: \
                state->pushDouble(ARITH_ ##op(_t2->value.d, _t->value.d)); \
                break; \
            default: \
                break; \
        } \
    } \
    else { \
    }

// Main interpreter
namespace Gavel {
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

    void lib_loadLibrary(_gchunk* chunk) {
        GChunk::setVar(chunk, "print", new CREATECONST_CFUNC(lib_print));
    }

    void executeChunk(GState* state, _gchunk* chunk) {
        state->pc = chunk->chunk;
        bool chunkEnd = false;
        while(state->state == GAVELSTATE_RESUME && !chunkEnd) 
        {
            INSTRUCTION inst = *(state->pc)++;
            DEBUGLOG(std::cout << "OP: " << GET_OPCODE(inst) << std::endl);
            switch(GET_OPCODE(inst))
            {
                case OP_PUSHVALUE: { // iAx
                    DEBUGLOG(std::cout << "pushing const[" << GETARG_Ax(inst) << "] to stack" << std::endl);
                    state->stack.push(chunk->consts[GETARG_Ax(inst)]);
                    break;
                }
                case OP_POP: { // iAx
                    // for Ax times, pop stuff off the stack
                    int times = GETARG_Ax(inst);
                    state->stack.pop(times);
                    break;
                }
                case OP_GETVAR: { // i
                    GValue* top = state->getTop();
                    state->stack.pop(); // pops
                    if (top->type == GAVEL_TSTRING) {
                        DEBUGLOG(std::cout << "pushing " << top->value.str << " to stack" << std::endl); 
                        state->stack.push(GChunk::getVar(chunk, top->value.str));
                    } else { // not a valid identifier
                        state->stack.push(CREATECONST_NULL());
                    }
                    break;
                }
                case OP_SETVAR: { // i -- sets vars[stack[top-1]] to stack[top]
                    GValue* top = state->getTop(1);
                    GValue* var = state->getTop();
                    if (top->type == GAVEL_TSTRING && var != NULL) {
                        DEBUGLOG(std::cout << "setting " << top->value.str << " to stack[top]" << std::endl); 
                        GChunk::setVar(chunk, top->value.str, var);

                        // pops var + identifier
                        state->stack.pop(2);
                    } else { // not a valid identifier
                        state->throwObjection("Invalid identifier! String expected!");
                    }
                    break;
                }
                case OP_BOOLOP: { // i checks top & top - 1
                    BOOLOP  bop = (BOOLOP)GETARG_Ax(inst);
                    GValue* _t = state->getTop(1);
                    GValue* _t2 = state->getTop();
                    state->stack.pop(2); // pop the 2 vars
                    bool t = false;
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
                    state->pushBool(t); // this will use our already defined == operator in the GValue struct
                    break;
                }
                case OP_TEST: { // i
                    if (!state->toBool(0)) {
                       // if false, skip next 2 instructions
                       DEBUGLOG(std::cout << "false! skipping chunk!" << std::endl);
                       state->pc += 2;
                    }
                    state->stack.pop(); // pop bool value (or whatever lol)
                    break;
                }
                case OP_CALL: { // iAx
                    int totalArgs = GETARG_Ax(inst);
                    DEBUGLOG(std::cout << "calling chunk at stack[" << totalArgs << "]" << std::endl);
                    GValue* top = state->getTop(totalArgs); // gets chunk off stack hopefully lol

                    switch (top->type)
                    {
                        case GAVEL_TCHUNK: { // it's a gavel chunk, so set parent to this chunk & call gavel::executeChunk with chunk
                            // TODO: i reallllllly don't want to deal with defining functions yet,, so LOL

                            /* basic idea:
                            - save pc to a local value here
                            - call executeChunk with state & _gchunk of GValue
                            - reset pc, 
                            */
                            INSTRUCTION* savedPc = state->pc;
                            top->value.i->parent = chunk; // just make sure ;)
                            executeChunk(state, top->value.i); // it's the chunk's job to pop the args & chunk
                            state->stack.pop(totalArgs + 1);
                            state->pc = savedPc;
                            break;
                        }
                        case GAVEL_TCFUNC: { // it's a c functions, so call the c function
                            GValue ret = top->value.cfunc(state, totalArgs-1); // call the c function with our state & number of parameters, value returned is the return value (if any)
                            state->stack.pop(totalArgs + 1); // pop args & chunk
                            if (ret.type != GAVEL_TNULL) { // if it's not NULL, put it on the stack :)
                                state->stack.push(ret);
                            }

                            break;
                        }
                        default:

                            state->throwObjection("GValue is not a callable object! : " + std::to_string(top->type));
                            break;
                    }
                    break;
                }
                case OP_ARITH: { // iAx
                    OPARITH aop = (OPARITH)GETARG_Ax(inst); // gets the type of arithmetic to do
                    switch (aop) 
                    {
                        case OPARITH_ADD: {
                            iAx_ARITH(inst, ADD);
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
                case OP_END: { // i
                    DEBUGLOG(std::cout << "END" << std::endl);
                    chunkEnd = true;
                    break;
                }
                default:
                    std::cout << "INVALID INSTRUCTION" << std::endl;
                    state->state = GAVELSTATE_YIELD;
                    break;
            }
        }
    }
}

// ===========================================================================[[ COMPILER ]]===========================================================================

class GavelToken {
public:
    TOKENTYPE type;
    GavelToken() {}
    GavelToken(TOKENTYPE t) {
        type = t;
    }

    virtual ~GavelToken() { }
};

class GavelToken_Comment : public GavelToken {
public:
    std::string text;
    GavelToken_Comment() {}
    GavelToken_Comment(std::string s) {
        type = TOKEN_COMMENT;
        text = s;
    }
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
    GValue cons;
    GavelToken_Constant() {}
    GavelToken_Constant(GValue c) {
        type = TOKEN_CONSTANT;
        cons = GValue(c.value, c.type);
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

#define CREATELEXERTOKEN_COMMENT(x)     GavelToken_Comment(x)
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
#define CREATELEXERTOKEN_SEPARATOR()    GavelToken(TOKEN_SEPARATOR)
#define CREATELEXERTOKEN_EOL()          GavelToken(TOKEN_ENDOFLINE)
#define CREATELEXERTOKEN_EOF()          GavelToken(TOKEN_ENDOFFILE)

// this will build a _gchunk from a scope, then give it back!
class GavelScopeParser {
private:
    std::vector<INSTRUCTION> insts;
    std::vector<GValue> consts;
    std::vector<GavelToken*>* tokenList;

public:
    GavelScopeParser(std::vector<GavelToken*>* tl) {
        tokenList = tl;
    }

    // returns index of constant
    int addConstant(GValue c) {
        for (int i = 0; i < consts.size(); i++)
            if (consts[i] == c)
                return i;
        consts.push_back(c);
        return consts.size() - 1;
    }

    GavelToken* peekNextToken(int i) {
        if (i >= tokenList->size()-1) {
            DEBUGLOG(std::cout << "end of token list! : " << i << std::endl);
            return new CREATELEXERTOKEN_EOF();
        }
        return (*tokenList)[i];
    }

    void parserObjection(std::string err) {
        std::cout << "OBJECTION! : " << err << std::endl;
        exit(0);
    }

    // parse mini-scopes, like the right of =, or anything in ()
    GavelToken* parseContext(int* indx) {
        do {
            GavelToken* token = peekNextToken(*indx);
            DEBUGLOG(std::cout << "CONTEXT TOKEN: " << token->type << std::endl);
            switch(token->type) {
                case TOKEN_CONSTANT: {
                    GValue cons = dynamic_cast<GavelToken_Constant*>(token)->cons;
                    int constIndx = addConstant(cons);
                    // pushes const onto stack
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, constIndx));
                    break;
                }
                case TOKEN_VAR: {
                    int varIndx = addConstant(CREATECONST_STRING(dynamic_cast<GavelToken_Variable*>(token)->text.c_str())); // this looks ugly lol
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, varIndx)); // pushes identifier onto stack
                    insts.push_back(CREATE_i(OP_GETVAR)); // pushes value of the identifier onto stack & pops identifier
                    break;
                }
                case TOKEN_ARITH: {
                    OPARITH op = dynamic_cast<GavelToken_Arith*>(token)->op;
                    // get other tokens lol
                    (*indx)++;
                    GavelToken* retn = parseContext(indx);
                    insts.push_back(CREATE_iAx(OP_ARITH, op));

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

                    return retn;
                }
                case TOKEN_BOOLOP: {
                    // parse everything to the right of the boolean operator. how the fuck do fix ?
                    GavelToken* nxt = parseContext(&(++(*indx)));

                    BOOLOP op = dynamic_cast<GavelToken_BoolOp*>(token)->op;
                    insts.push_back(CREATE_iAx(OP_BOOLOP, op));
                    return nxt;
                }
                default:
                    // doesn't recognize token, return to caller
                    return token;
                    break;
            }
            (*indx)++;
        } while(true);
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
                    int varIndx = addConstant(CREATECONST_STRING(dynamic_cast<GavelToken_Variable*>(token)->text.c_str())); // this looks ugly lol
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, varIndx)); // pushes identifier onto stack
                    
                    if (peekNext->type == TOKEN_ASSIGNMENT) {
                        (*indx)+=2;
                        DEBUGLOG(std::cout << "indx : " << *indx << std::endl);
                        GavelToken* nxt;
                        if (parseContext(indx)->type != TOKEN_ENDOFLINE)
                        {
                            parserObjection("Invalid syntax!");
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
                    if (peekNextToken(*indx + 1)->type == TOKEN_ENDCALL) {
                        insts.push_back(CREATE_iAx(OP_CALL, 0));
                        break;
                    }
                    (*indx)++;

                    // parse until ENDCALL
                    GavelToken* nxt;
                    int numArgs = 0; // keeps track of the arguments :)!
                    while(nxt = parseContext(indx)) {
                        // only 2 tokens are allowed, ENDCALL & SEPARATOR
                        if (nxt->type == TOKEN_ENDCALL) {
                            break; // stop parsing
                        } else if (nxt->type != TOKEN_SEPARATOR) {
                            parserObjection("Invalid syntax! \"" "," "\" or \"" ")" "\" expected!");
                        }
                        numArgs++;
                        (*indx)++;
                    }
                    insts.push_back(CREATE_iAx(OP_CALL, numArgs+1));
                    break;
                }
                case TOKEN_IFCASE: {
                    DEBUGLOG(std::cout << " if case " << std::endl);

                    if (peekNextToken(++(*indx))->type != TOKEN_OPENCALL) {
                        parserObjection("Invalid syntax! \"" "(" "\" expected after \"" GAVELSYNTAX_IFCASE "\"");
                    }

                    GavelToken* nxt;
                    do {
                        (*indx)++;
                        nxt = parseContext(indx);

                        if (nxt->type == TOKEN_ENDCALL) {
                            break;
                        } else {
                            parserObjection("Invalid syntax! \"" ")" "\" expected!");
                        }
                    } while(true);

                    // parse next line or scope
                    if (peekNextToken(++(*indx))->type != TOKEN_OPENSCOPE) {
                        // SINGLE LINE
                        _gchunk* scope = (new GavelScopeParser(tokenList))->singleLineChunk(indx);
                        int chunkIndx = addConstant(CREATECONST_CHUNK(scope));

                        insts.push_back(CREATE_i(OP_TEST));
                        insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));
                        insts.push_back(CREATE_iAx(OP_CALL, 0)); // calls a chunk with 0 arguments.
                    } else {
                        // parse whole scope (which will be taken care of by TOKEN_OPENSCOPE ok ty)
                        (*indx)--;
                        break;
                    }
                    return;
                }
                case TOKEN_OPENSCOPE: {
                    (*indx)++;
                    _gchunk* scope = (new GavelScopeParser(tokenList))->parseScope(indx);
                    int chunkIndx = addConstant(CREATECONST_CHUNK(scope));

                    insts.push_back(CREATE_i(OP_TEST));
                    insts.push_back(CREATE_iAx(OP_PUSHVALUE, chunkIndx));
                    insts.push_back(CREATE_iAx(OP_CALL, 0)); // calls a chunk with 0 arguments.
                    return;
                }
                case TOKEN_ENDSCOPE:
                case TOKEN_ENDOFLINE: 
                case TOKEN_ENDOFFILE:
                    //(*indx)++;
                    return;
                default:
                    DEBUGLOG(std::cout << "unknown token! " << token->type << std::endl);
                    break;
            }
            (*indx)++;
        } while(true);
    }

    _gchunk* singleLineChunk(int* indx) {
        DEBUGLOG(std::cout << "parsing single line" << std::endl);
        parseLine(indx);
        insts.push_back(CREATE_i(OP_END));
        DEBUGLOG(std::cout << "exiting single line" << std::endl);
        return new _gchunk(&insts[0], &consts[0]);
    }

    // this will parse a WHOLE scope, aka { /* code */ }
    _gchunk* parseScope(int* indx) {
        DEBUGLOG(std::cout << "parsing new scope" << std::endl);
        do {
            parseLine(indx);
        } while(peekNextToken(++(*indx))->type != TOKEN_ENDSCOPE && peekNextToken(*indx)->type != TOKEN_ENDOFFILE);
        DEBUGLOG(std::cout << "exiting scope.." << std::endl);

        insts.push_back(CREATE_i(OP_END));

        return new _gchunk(&insts[0], &consts[0]);
    }
};

class GavelCompiler {
private:
    std::vector<GavelToken*> tokenList;
    std::vector<INSTRUCTION> insts;
    std::vector<GValue> consts;
    char* code;
    char* currentChar;
    size_t len;
    int lines;

public:
    GavelCompiler(char* c) {
        code = c;
        currentChar = c;
        len = strlen(c);
        lines = 0;
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

    GValue readNumber() {
        std::string num;

        while (isNumeric(*currentChar)) {
            num += *currentChar++; // adds char to string
        }

        *currentChar--;

        return CREATECONST_DOUBLE(std::stod(num.c_str()));
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

    GavelToken* nextToken() {
        GavelToken* token = NULL;
        while (true) {
            switch (*currentChar) {
                case GAVELSYNTAX_COMMENTSTART:
                    if (peekNext() == GAVELSYNTAX_COMMENTSTART) {
                        *currentChar++;
                        char* commentStart = currentChar;
                        // it's a comment, so skip to next line
                        int length = nextLine();
                    } else { // it's just / by itself lol, so it's trying to divide!!!!!
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
                    token =  new CREATELEXERTOKEN_CONSTANT(CREATECONST_STRING(readString()->c_str()));
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
                default:
                    if (isEmpty(*currentChar)) {
                        // we don't care about whitespace. this is not a structured langauge...... lookin' at you python....
                        break;
                    }

                    // check for constants or identifier
                    if (isNumeric(*currentChar)) {
                        DEBUGLOG(std::cout << " number " << std::endl);
                        token = new CREATELEXERTOKEN_CONSTANT(readNumber());
                    } else if (isalpha(*currentChar)) { // identifier
                        //======= [[ reserved words lol ]] =======
                        std::string ident = readIdentifier();

                        if (ident == GAVELSYNTAX_IFCASE) {
                            DEBUGLOG(std::cout << " IF " << std::endl);
                            token = new CREATELEXERTOKEN_IFCASE();
                        } else if (ident == GAVELSYNTAX_BOOLFALSE) {
                            DEBUGLOG(std::cout << " FALSE " << std::endl);
                            token = new CREATELEXERTOKEN_CONSTANT(CREATECONST_BOOL(false));
                        } else if (ident == GAVELSYNTAX_BOOLTRUE) {
                            DEBUGLOG(std::cout << " TRUE " << std::endl);
                            token = new CREATELEXERTOKEN_CONSTANT(CREATECONST_BOOL(true));
                        } else {
                            DEBUGLOG(std::cout << " var: " << ident << std::endl);
                            token = new CREATELEXERTOKEN_VAR(ident);
                        }
                    } else { 
                        OPARITH op = isOp(*currentChar);
                        DEBUGLOG(std::cout << " OPERATOR " << std::endl);
                        if (op != OPARITH_NONE) {
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

    _gchunk parse() {
        DEBUGLOG(std::cout << "[*] COMPILING SCRIPT.." << std::endl);
        buildTokenList();

        int i = 0;
        _gchunk* mainChunk = (new GavelScopeParser(&tokenList))->parseScope(&i);
        mainChunk->parent = NULL;

        DEBUGLOG(std::cout << "[*] DONE!" << std::endl);

        // build instruction & consts table
        return *mainChunk;
    }
};

#endif // hi there :)