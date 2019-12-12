/*
    GavelScript Demo for adding C API to GavelScript.
        Demo: Getting POSIX username!
*/

#include "gavel.h"
#include <unistd.h>
#include <pwd.h>

GValue getUsername(GState* state, int args) {
    // get POSIX username and return, else return NULL
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw)
    {
        return CREATECONST_STRING(pw->pw_name);
    }
    return CREATECONST_NULL();
}

int main()
{
    GavelCompiler testScript(R"(
        print("The user that started this process is: ", getUsername());
    )");
    GState* yaystate = new GState();
    _gchunk* mainChunk = testScript.parse();

    // loads print
    Gavel::lib_loadLibrary(mainChunk);

    // adds our own C Function
    GChunk::setLocalVar(mainChunk, "getUsername", new CREATECONST_CFUNC(getUsername));

    // runs the script
    Gavel::executeChunk(yaystate, mainChunk);

    return 0;
}