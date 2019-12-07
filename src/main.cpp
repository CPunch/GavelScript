#include "gavel.h"

int main()
{
    GavelCompiler testScript(R"(    // 1
        function fact(i) {          // 2
            if (i == 0) {           // 3
                return 1;           // 4
            }                       // 5
            return i*fact(i-1);     // 6
        }

        print(fact(150));
    )");
    GState* yaystate = new GState();
    _gchunk* mainChunk = testScript.parse();

    // loads print
    Gavel::lib_loadLibrary(mainChunk);

    /*int arrayTest[10]; // user var test!
    for (int i = 0; i < 10; i++)
        arrayTest[i] = i*i;

    GChunk::setVar(&mainChunk, "testUserVar", new CREATECONST_USERV((void*)&arrayTest, [](_uservar* t, _uservar* o) {
        // equals
        return false;
    }, [](_uservar* t, _uservar* o) {
        // lessthan 
        return false;
    }, [](_uservar* t, _uservar* o) {
        // morethan 
        return false;
    }, [](_uservar* t) {
        // toString 
        std::stringstream out;
        out << "Int Array: [";
        for (int i = 0; i < 10; i ++) {
            out << ((int*)t->ptr)[i];
            if (i != 9)
                out << ", ";
        }
        out << "]";
        return out.str();
    }));*/

    Gavel::executeChunk(yaystate, mainChunk);

    // debug :)
    // yaystate->stack.printStack();

    return 0;
}