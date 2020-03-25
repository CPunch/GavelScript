#include "gavel-rewrite.h"

/* https://github.com/yhirose/cpp-linenoise
    This library is just to have a nice readline-like console experience. Please checkout this project and the original linenoise.c if you 
get the chance. It's a wonderful library with a really cool goal :D
*/
#include "linenoise.hpp"

int main(int argc, char* argv[])
{
    if (argc > 1) { // if they're passing filenames to run
        for (int i = 1; i < argc; i++) {
            // load file to string
            std::ifstream ifs(argv[i]);
            std::string script((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

            // compiles script
            GavelParser compiler(script.c_str());
            if (!compiler.compile()) { // compiler objection was thrown
                // print objection, skip to next iteration
                std::cout << argv[i] << ": " << compiler.getObjection().getFormatedString() << std::endl;
                continue;
            }

            // create state
            GState* state = new GState();
            GavelLib::loadLibrary(state); // loads standard library to the state

            GObjectFunction* mainFunc = compiler.getFunction();

            if (state->start(mainFunc) != GSTATE_OK) {
                // objection occurred
                std::cout << argv[i] << ": " << state->getObjection().getFormatedString() << std::endl;
            }

            delete mainFunc;
            delete state;
        }
        return 0;
    }


    // create state
    GState* state = new GState();
    GavelLib::loadLibrary(state); // loads standard library to the state

    // make a vector to store all of the functions we create to free later :)
    std::vector<GObjectFunction*> funcs;

    std::cout << GavelLib::getVersion() << " very-interactive-shell! " << std::endl;

    // you can write full scripts in the shell
    linenoise::SetMultiLine(true);

    while(true) {
        std::string script;
        if (linenoise::Readline(">> ", script)) {
            // if it returns true, this terminal is stupid and doesn't work with input properly.
            break;
        }
        // script now holds our nicely formated single-line script
        linenoise::AddHistory(script.c_str());

        // compile the script
        GavelParser compiler(script.c_str());
        if (!compiler.compile()) { // compiler objection was thrown
            // print objection, skip to next iteration
            std::cout << compiler.getObjection().getFormatedString() << std::endl;
            continue;
        }

        GObjectFunction* mainFunc = compiler.getFunction();

        mainFunc->val->dissassemble();

        if (state->start(mainFunc) != GSTATE_OK) {
            // objection occurred
            std::cout << state->getObjection().getFormatedString() << std::endl;
        }

        funcs.push_back(mainFunc);
    }

    // free functions
    for (GObjectFunction* f : funcs) {
        delete f;
    }

    delete state;

    return 0;
}