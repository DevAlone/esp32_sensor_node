#pragma once

#include <functional>

class CallOnScopeExit {
public:
    CallOnScopeExit(std::function<void()> cleanupHandler);
    ~CallOnScopeExit();

private:
    std::function<void()> cleanupHandler;
};

/// Used to call some function on exit of scope
/// for example when exiting from function
#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)
#define defer(cleanupHandler) CallOnScopeExit CONCAT(_defer_, __LINE__)(cleanupHandler)
