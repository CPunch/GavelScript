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

        i = 8;
        while (i > 0) {
            result = fact(i);
            if (i == 1)
                print("last loop!");
            print("The factorial of ", i, " is ", result);
            i=i-1;
        }
    )");
    GState* yaystate = new GState();
    _gchunk* mainChunk = testScript.compile();

    // testing the deserializer!!
    GavelSerializer testSerializer;
    std::vector<BYTE> data = testSerializer.serialize(mainChunk);
    GavelDeserializer testDeserializer(data);
    mainChunk = testDeserializer.deserialize();

    // loads print
    Gavel::lib_loadLibrary(mainChunk);
    // runs the script
    Gavel::executeChunk(yaystate, mainChunk);

    //yaystate->stack.printStack();

    return 0;
}