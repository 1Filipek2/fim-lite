#pragma once

namespace fimlite
{

class ConsoleCodePage
{
public:
    ConsoleCodePage();
    ~ConsoleCodePage();

    ConsoleCodePage(const ConsoleCodePage&) = delete;
    ConsoleCodePage& operator = (const ConsoleCodePage&) = delete;

private:
    unsigned int previous_;
};

} // namespace fimlite
