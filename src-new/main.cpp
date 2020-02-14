#include "gavel-rewrite.h"

int main() {
    GavelParser test(R"(
        // factorial stress test, old gavel took ~20ish seconds
        var i = 1000
        while (i > 0) do
            i = i - 1
            var z = 100
            while (z > 1) do
                z = z - 1
                var total = 1
                var x = z
                while (x > 1) do
                    total = total * x
                    x = x - 1
                end
            end
        end
    )");

    if (test.compile()) {
        // compile successful
        GChunk* mainChunk = test.getChunk();
        GState* state = new GState();
        if (state->runChunk(mainChunk) != GSTATE_OK) {
            std::cout << state->getObjection().getFormatedString() << std::endl;
        }
        //mainChunk->dissassemble();

        delete mainChunk;
        delete state;
    } else {
        std::cout << test.getObjection().getFormatedString() << std::endl;
        delete test.getChunk();
    }

    return 1;
}