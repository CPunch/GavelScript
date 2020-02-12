#include "gavel-rewrite.h"

int main() {
    GavelParser test(R"(
        // ; are optional
        test = "Hello World!"
        local num = 3 * (5/2)
        do
            local hi = test
        end
        num = "hi";
        tst = num
    )");

    GavelParser test2(R"(
        a = "nope"
        okay = test + " :eyes:"
    )");

    if (test.compile()) {
        // compile successful
        GChunk* mainChunk = test.getChunk();
        GState* state = new GState();
        state->runChunk(mainChunk);
        if (test2.compile()){
            GChunk* secondChunk = test2.getChunk();
            state->runChunk(secondChunk);
            state->stack.printStack();
            delete secondChunk;
        } else {
            delete test2.getChunk();
            std::cout << test2.getObjection().getFormatedString() << std::endl;
        }

        delete mainChunk;
        delete state;
    } else {
        std::cout << test.getObjection().getFormatedString() << std::endl;
    }

    return 1;
}