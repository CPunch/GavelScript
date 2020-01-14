/*
    GavelScript Main, this is mainly used for testing 
*/

#include <fstream>
#include "gavel.h"

GValue* lib_quit(GState* state, int args) {
    exit(0);

    // this shouldn't even be executed tbh
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
                std::cout << compiler.getObjection() << std::endl;
                delete state, mainChunk;
                continue;
            }

            if (!state->start(mainChunk)) {
                // objection occurred
                std::cout << state->getObjection() << std::endl;
                state->stack.clearStack();
            }

            delete state, mainChunk;
        }
        return 0;
    }
    
    std::cout << Gavel::getVersionString() << " not-so-interactive shell" << std::endl;

    std::string script;
    GState* state = new GState();
    Gavel::lib_loadLibrary(state);

    state->setGlobal("quit", lib_quit);
    
    GValueTable GTT;
    GTT.newIndex("pi", 3.1415926535);
    GTT.newIndex(1, "Hello");
    GTT.newIndex(2, "World");

    state->setGlobal("GTable", reinterpret_cast<GValue*>(&GTT));
    // clone of GTT was set to GTable. modifications to GTT will NOT be reflected to the GavelScript env.

    while (true) {
        std::cout << ">> ";
        std::getline(std::cin, script);
        GavelCompiler compiler(script.c_str());
        GChunk* mainChunk = compiler.compile();

        if (mainChunk == NULL) {
            std::cout << compiler.getObjection() << std::endl;
            continue;
        }

        /*GavelSerializer testSerializer;
        std::vector<BYTE> data = testSerializer.serialize(mainChunk);
        GavelDeserializer testDeserializer(data);
        mainChunk = testDeserializer.deserialize();*/

        if (!state->start(mainChunk)) {
            // objection occurred
            std::cout << state->getObjection() << std::endl;
            state->stack.clearStack();
        }
    }

    delete state;
    return 0;
}