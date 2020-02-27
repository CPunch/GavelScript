#include "gavel-rewrite.h"

/* Factorial test (current benchmark on cpunch's machine: 2.29s goal: 1.5s)
function fact(num)
    var total = 1
    for (var i = num; i > 1; i=i-1) do
        total = total * i
    end
    return total
end

for (var i = 1000; i > 0; i=i-1) do
    for (var x = 100; x > 0; x=x-1) do
        print("The factorial of ", x, " is ", fact(x))
    end
end
*/

int main() {
    GavelParser test(R"(
        function fact(num)
            var total = 1
            for (var i = num; i > 1; i=i-1) do
                total = total * i
            end
            return total
        end

        for (var i = 1000; i > 0; i=i-1) do
            for (var x = 100; x > 0; x=x-1) do
                print("The factorial of " + x + " is " + fact(x))
            end
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