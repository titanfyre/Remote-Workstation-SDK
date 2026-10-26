#pragma once
#include "RuntimeSetup.h"
namespace AppSetup {
RuntimeSetup::Result Execute(bool install) noexcept;
bool Installed() noexcept;
std::wstring Location();
}
