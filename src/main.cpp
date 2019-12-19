/*
    GavelScript Main, this is mainly used for testing 
*/

#include "gavel.h"

int main()
{
    GavelCompiler testScript(R"(
        test = "hello!";
        print(test);
        test2 = "hello!";
        test2 = "EPIC!";
        print(test2);
    )");
    GState* yaystate = new GState();
    _gchunk* mainChunk = testScript.compile();

    // testing the deserializer!!
    /*GavelSerializer testSerializer;
    std::vector<BYTE> data = testSerializer.serialize(mainChunk);
    GavelDeserializer testDeserializer(data);
    mainChunk = testDeserializer.deserialize();*/

    // loads print
    Gavel::lib_loadLibrary(mainChunk);

    std::cout << "STARTING EXECUTION" << std::endl;
    // runs the script
    Gavel::executeChunk(yaystate, mainChunk);

    //yaystate->stack.printStack();

    return 0;
}