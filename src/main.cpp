#include "gavel.h"

/* https://github.com/yhirose/cpp-linenoise
    This library is just to have a nice readline-like console experience. Please checkout this project and the original linenoise.c if you 
get the chance. It's a wonderful library with a really cool goal :D
*/
#include "linenoise.hpp"

class A {
public:
    std::string test;
    
    A(std::string t):
        test(t) {}

    static GValue protoTestCall(GState* state, std::vector<GValue> &args) {
        GValue tblVal = state->stack.getTop(0); // prototable will always be the top value on the stack

        if (!ISGVALUEPROTOTABLE(tblVal)) { // sanity check
            std::cout << "failed aaaa!! [" << tblVal.type << "]" << std::endl;
            return CREATECONST_NIL();
        }

        // READGVALUEPROTOTABLE returns the userdata pointer setup in the constructor of GObjectPrototable
        std::cout << ((A*)READGVALUEPROTOTABLE(tblVal))->test << std::endl;

        return CREATECONST_NIL();
    }
};

int main(int argc, char* argv[]) {
    if (argc > 1) { // if they're passing filenames to run
        // default is to run the file

        // load file to string
        std::ifstream ifs(argv[1]);
        std::string script((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        
        // create state
        GState* state = Gavel::newState();
        GavelLib::loadLibrary(state); // loads standard library to the state
        GObjectFunction* mainFunc = NULL;

        // check if it's a compiled script
        if (GUndump::checkHeader((void*)script.c_str())) {
            GUndump deserializer((void*)script.c_str(), script.length());
            mainFunc = deserializer.getData();
            mainFunc->val->disassemble();

        } else {
            // compiles script
            GavelParser compiler(script.c_str());
            if (!compiler.compile()) { // compiler objection was thrown
                // print objection, skip to next iteration
                std::cout << argv[1] << ": " << compiler.getObjection().getFormatedString() << std::endl;
                return 1;
            }

            mainFunc = compiler.getFunction();
            if (argc > 2) { // write compiled script to a file
                std::ofstream fout;
                fout.open(argv[2], std::ios::binary | std::ios::out);

                GDump serializer(mainFunc);
                fout.write((char*)serializer.getData(), serializer.getSize());
                fout.close();
                std::cout << "Compiled script and wrote to " << argv[2] << std::endl;

                delete mainFunc;
                Gavel::freeState(state);
                return 0;
            }
        }

        if (state->start(mainFunc) != GSTATE_OK) {
            // objection occurred
            std::cout << argv[1] << ": " << state->getObjection().getFormatedString() << std::endl;
        }

        delete mainFunc;
        Gavel::freeState(state);
        return 0;
    }

    std::cout << GavelLib::getVersion() << " somewhat-interactive-shell! " << std::endl;

    // create state
    GState* state = Gavel::newState();
    GavelLib::loadLibrary(state); // loads standard library to the state

    // make a vector to store all of the functions we create to free later :)
    std::vector<GObjectFunction*> funcs;

    // you can write full scripts in the shell
    linenoise::SetMultiLine(true);

    // prototable test! this wraps a c++ object to a prototable in gavelscript!
    A testA("HELLO WORLD!");
    GObjectPrototable* tbl = new GObjectPrototable((void*)&testA);
    tbl->newIndex("test", &testA.test);
    tbl->newIndex("printVal", A::protoTestCall);

    // sets the table to a global var so we can access it
    state->setGlobal("_G", tbl);

    while(true) {
        std::string script;
        if (linenoise::Readline(">> ", script)) {
            // if it returns true, this terminal is stupid and doesn't work with input properly.
            break;
        }
        // script now holds our nicely formated single-line script
        linenoise::AddHistory(script.c_str());

        // compile the script
        GavelParser compiler(script.c_str());
        if (!compiler.compile()) { // compiler objection was thrown
            // print objection, skip to next iteration
            std::cout << compiler.getObjection().getFormatedString() << std::endl;
            continue;
        }

        GObjectFunction* mainFunc = compiler.getFunction();

        mainFunc->val->disassemble();

        if (state->start(mainFunc) != GSTATE_OK) {
            // objection occurred
            std::cout << state->getObjection().getFormatedString() << std::endl;
        }

        funcs.push_back(mainFunc);
    }

    // free functions
    for (GObjectFunction* f : funcs) {
        delete f;
    }

    Gavel::freeState(state);

    return 0;
}