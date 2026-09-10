#include "fimlite/console.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace fimlite
{

ConsoleCodePage::ConsoleCodePage()
    : previous_(GetConsoleOutputCP())
{
    SetConsoleOutputCP(CP_UTF8);
}

ConsoleCodePage::~ConsoleCodePage()
{
    if (previous_ != 0)
    {
        SetConsoleOutputCP(previous_);
    }
}

} // namespace fimlite
