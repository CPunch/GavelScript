#include "gavel-rewrite.h"

int main() {
    GavelParser test(R"(
        function factorial(num) 
            var total = 1
            for (var i = num; i > 0; i=i-1) do
                total = total * i
            end
            return total
        end

        for (var z = 1000; z > 0; z=z-1) do
            for (var x = 100; x > 0; x=x-1) do
                print("The factorial of ", x, " is ", factorial(x))
            end
        end
    )");

    if (test.compile()) {
        // compile successful
        GObjectFunction* mainFunc = test.getFunction();
        
        GState* state = new GState();
        GavelLib::addLibrary(state);

        //mainFunc->val->dissassemble();

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