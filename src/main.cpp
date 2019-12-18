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

        function factWrapper(x) {
            result = fact(x);
            print("Factorial of ", x, " is ", type(result), ": ", result); // type will give you the datatype of a GValue!

            if (x > 1)
                factWrapper(x-1); 
        }
        
        factWrapper(8); // simple loop using recursion. soon i'll add real loops ok...
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

    return 0;
}