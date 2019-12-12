/*
    GavelScript Main, this is mainly used for testing 
*/

#include "gavel.h"

int main()
{
    GavelCompiler testScript(R"(
        function test(i) {
            if (i == 1)
                t(); // this will throw an objection because t doesn't exist (NULL) line 4
            test(i-1);
        }
        test(5);
    )");
    GState* yaystate = new GState();
    _gchunk* mainChunk = testScript.compile();

    // loads print
    Gavel::lib_loadLibrary(mainChunk);

    // runs the script
    Gavel::executeChunk(yaystate, mainChunk);

    return 0;
}