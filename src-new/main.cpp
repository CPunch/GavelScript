#include "gavel-rewrite.h"

int main() {
    GavelParser test(R"(
        // ; are optional
        test = "Hello World!"
        local num = 0;
        do
            local hi = test
            hi = "noooo"
        end
        num = "ok"
        tst = num
    )");

    if (test.compile()) {
        // compile successful
        GChunk* mainChunk = test.getChunk();
        GState* state = new GState();
        if (state->runChunk(mainChunk) != GSTATE_OK) {
            std::cout << state->getObjection().getFormatedString() << std::endl;
        }

        delete mainChunk;
        delete state;
    } else {
        std::cout << test.getObjection().getFormatedString() << std::endl;
    }

    return 1;
}