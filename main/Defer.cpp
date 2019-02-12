#include "Defer.h"

CallOnScopeExit::CallOnScopeExit(std::function<void()> cleanupHandler)
    : cleanupHandler(cleanupHandler)
{
}

CallOnScopeExit::~CallOnScopeExit()
{
    if (cleanupHandler) {
        cleanupHandler();
    }
}
