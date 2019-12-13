/*
    GavelScript Main, this is mainly used for testing 
*/

#include "gavel.h"

int main()
{
    GavelCompiler testScript(R"(
        function fact(i) {
            if (i == 1)
                return 1;
            return i*fact(i-1);
        }

        x = 5;
        print("The factorial of ", x, " is ", fact(x));
    )");
    GState* yaystate = new GState();
    _gchunk* mainChunk = testScript.compile();

    // loads print
    Gavel::lib_loadLibrary(mainChunk);

    // runs the script
    Gavel::executeChunk(yaystate, mainChunk);

    return 0;
}