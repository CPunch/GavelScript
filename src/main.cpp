/*
    GavelScript Main, this is mainly used for testing 
*/

#include <fstream>
#include "gavel.h"

GValue* lib_quit(GState* state, int args) {
    exit(0);

    // this shouldn't even be executed tbh (might not even be compiled in depending on the compiler :eyes:)
    return CREATECONST_NULL();
}

GChunk* testFunc = NULL;

GValue* lib_setFunc(GState* state, int args) {
    GValue* top = state->getTop();
    if (top->type == GAVEL_TCHUNK) {
        testFunc = READGVALUECHUNK(top);
    }
    
    return CREATECONST_NULL();
}

GValue* lib_testCall(GState* state, int args) {
    GState* nstate = new GState();
    Gavel::lib_loadLibrary(nstate);

    nstate->callFunction(testFunc, "Hello ", "World");

    delete nstate;

    return CREATECONST_NULL();
}

int main(int argc, char* argv[])
{
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            std::ifstream ifs(argv[i]);
            std::string script((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            GState* state = new GState();
            Gavel::lib_loadLibrary(state);
            GavelCompiler compiler(script.c_str());
            GChunk* mainChunk = compiler.compile();

            if (mainChunk == NULL) {
                std::cout << compiler.getObjection().getFormatedString() << std::endl;
                delete state, mainChunk;
                continue;
            }

            std::cout << "running " << argv[i] << std::endl;

            if (!state->start(mainChunk)) {
                // objection occurred
                std::cout << state->getObjection().getFormatedString() << std::endl;
                state->stack.clearStack();
            }
            getchar();
            delete state, mainChunk;
        }
        return 0;
    }
    
    std::cout << Gavel::getVersionString() << " not-so-interactive shell" << std::endl;

    std::string script;
    GState* state = new GState();
    Gavel::lib_loadLibrary(state);

    state->setGlobal("quit", lib_quit);
    state->setGlobal("setFunc", lib_setFunc);
    state->setGlobal("callFunc", lib_testCall);

    
    GValueTable GTT;
    GTT.newIndex("pi", 3.1415926535);
    GTT.newIndex(1, "Hello");
    GTT.newIndex(2, "World");

    state->setGlobal("GTable", reinterpret_cast<GValue*>(&GTT));

    std::vector<GChunk*> chks;
    // clone of GTT was set to GTable. modifications to GTT will NOT be reflected to the GavelScript env.

    while (true) {
        std::cout << ">> ";
        std::getline(std::cin, script);
        GavelCompiler compiler(script.c_str());
        GChunk* mainChunk = compiler.compile();

        if (mainChunk == NULL) {
            std::cout << compiler.getObjection().getFormatedString() << std::endl;
            continue;
        }

        /*GavelSerializer testSerializer;
        std::vector<BYTE> data = testSerializer.serialize(mainChunk);
        GavelDeserializer testDeserializer(data);
        mainChunk = testDeserializer.deserialize();*/

        chks.push_back(mainChunk);

        if (!state->start(mainChunk)) {
            // objection occurred
            std::cout << state->getObjection().getFormatedString() << std::endl;
            state->stack.clearStack();
        }
    }

    for (GChunk* chk : chks) {
        delete chk;
    }

    delete state;
    return 0;
}