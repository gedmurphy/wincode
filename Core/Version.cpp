/*!

    @file Core/Version.cpp

    @brief The version string.

    @details The number comes from project() in CMakeLists.txt, passed in as
             WINCODE_VERSION_MAJOR, _MINOR and _PATCH, so there is one place
             to change it.

    @author Ged Murphy

    @copyright Copyright (c) 2026 Ged Murphy

    This file is part of WinCode.

    WinCode is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    WinCode is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with WinCode. If not, see <https://www.gnu.org/licenses/>.

*/

#include "WinCode.hpp"


//
// Two levels, so the argument is macro expanded before it is stringized.
//
#define WINCODE_STRINGIZE2(x) #x
#define WINCODE_STRINGIZE(x) WINCODE_STRINGIZE2(x)

static constexpr WCHAR k_Version[] = L"" WINCODE_STRINGIZE(WINCODE_VERSION_MAJOR)
                                     "." WINCODE_STRINGIZE(WINCODE_VERSION_MINOR)
                                     "." WINCODE_STRINGIZE(WINCODE_VERSION_PATCH);


PCWSTR
WinCodeVersion (
    void
    )
{
    return k_Version;
}
