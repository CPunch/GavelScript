#include "gavel-rewrite.h"

int main() {
    GavelParser test(R"(
        // factorial stress test, old gavel took ~10ish seconds

        // do everything a thousand times
        for (var indx = 1000; indx > 0; indx = indx -1) do
            // get factorials 1-100
            for (var z = 100; z > 1; z = z - 1) do
                // this part computes the factorial
                var total = 1
                for (var i = z; i > 1; i=i-1) do
                    total = total * i
                end
            end
        end
    )");

    if (test.compile()) {
        // compile successful
        GChunk* mainChunk = test.getChunk();
        GState* state = new GState();
        mainChunk->dissassemble();
        if (state->runChunk(mainChunk) != GSTATE_OK) {
            std::cout << state->getObjection().getFormatedString() << std::endl;
        }

        delete mainChunk;
        delete state;
    } else {
        std::cout << test.getObjection().getFormatedString() << std::endl;
        delete test.getChunk();
    }

    return 1;
}