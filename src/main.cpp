/*
    GavelScript Main, this is mainly used for testing 
*/

#include "gavel.h"

int main(int argc, char* argv[])
{
    std::cout << Gavel::getVersionString() << " not-so-interactive shell" << std::endl;

    std::string script;
    while (true) {
        std::cout << ">> ";
        std::getline(std::cin, script);
        GavelCompiler compiler((char*)script.c_str());
        _gchunk* mainChunk = compiler.compile();

        if (mainChunk == NULL) {
            continue;
        }

        GState* state = new GState();
        Gavel::lib_loadLibrary(mainChunk);
        Gavel::executeChunk(state, mainChunk);
        Gavel::freeChunk(mainChunk);
        delete state;
    }

    return 0;
}