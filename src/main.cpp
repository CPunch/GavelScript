#include "gavel-rewrite.h"

int main() {
    GavelParser test(R"(
        do
            var lclTest = "hi"

            function test()
                return lclTest
            end

            function setTest()
                print("Before: " + test())
                lclTest = "HAHAHA"
            end

            setTest();
            print("After: " + test())
        end
    )");

    if (test.compile()) {
        // compile successful
        GObjectFunction* mainFunc = test.getFunction();
        
        GState* state = new GState();
        GavelLib::addLibrary(state);

        mainFunc->val->dissassemble();

        if (state->start(mainFunc) != GSTATE_OK) {
            std::cout << state->getObjection().getFormatedString() << std::endl;
        }
        //state->printGlobals();

        delete mainFunc;
        delete state;
    } else {
        std::cout << test.getObjection().getFormatedString() << std::endl;
        delete test.getFunction();
    }

    return 1;
}