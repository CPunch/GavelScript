#include "gavel-rewrite.h"

int main() {
    GavelParser test(R"(
        // ; are optional, () are just syntaxical sugar for grouping
        var test = "Hello World!"
        if (1 == 1) then
            var lclTest = "wtf"
            test = lclTest
        end
        var ok = "hi";
    )");

    if (test.compile()) {
        // compile successful
        GChunk* mainChunk = test.getChunk();
        GState* state = new GState();
        if (state->runChunk(mainChunk) != GSTATE_OK) {
            std::cout << state->getObjection().getFormatedString() << std::endl;
        }
        mainChunk->dissassemble();

        delete mainChunk;
        delete state;
    } else {
        std::cout << test.getObjection().getFormatedString() << std::endl;
        delete test.getChunk();
    }

    return 1;
}