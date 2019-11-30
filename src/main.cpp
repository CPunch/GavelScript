#include "gavel.h"

int main()
{
    GavelCompiler testScript(R"(
        testVar = 200*2.5;
        testVar2 = testVar + 3;
        boolTest = false == 1 == 2;
        if (boolTest) { // this is a comment and will be ignored by the compiler!!!
            print("((200*2.5)+3)/2 = ", testVar2 / 2);
            if (testVar2 > 500) { // comparing 503 > 500 so should be true
                print("hi : ", testVar);
            }
        }
        print("i should always print! goodbye!!!");
    )");
    GState* yaystate = new GState();
    _gchunk mainChunk = testScript.parse();

    // loads print
    Gavel::lib_loadLibrary(&mainChunk);
    Gavel::executeChunk(yaystate, &mainChunk);

    // debug :)
    //yaystate->stack.printStack();

    return 0;
}