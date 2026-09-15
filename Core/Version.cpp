/*!

    @file Core/Version.cpp

    @brief The version string.

    @details The number comes from project() in CMakeLists.txt as
             WINCODE_VERSION_MAJOR, _MINOR and _PATCH, so there's one place to
             change it.

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
// Two levels, so the argument gets expanded before it's stringized.
//
#define WINCODE_STRINGIZE2(x) #x
#define WINCODE_STRINGIZE(x) WINCODE_STRINGIZE2(x)

static constexpr WCHAR k_Version[] = L"" WINCODE_STRINGIZE(WINCODE_VERSION_MAJOR)
                                     "." WINCODE_STRINGIZE(WINCODE_VERSION_MINOR)
                                     "." WINCODE_STRINGIZE(WINCODE_VERSION_PATCH);


/*!

    @brief The WinCode version, for example 2.0.0.

    @return A static, never null, string.

*/
PCWSTR
WinCodeVersion (
    void
    )
{
    return k_Version;
}


/*!

    @brief Reads a version into its first three numbers.

    @param[in] Text - The version.

    @param[out] Parts - Receives the numbers, zero where there are none.

*/
static
void
ReadVersion (
    _In_z_ PCWSTR Text,
    _Out_writes_(3) uint32_t* Parts
    )
{
    PCWSTR c;

    Parts[0] = Parts[1] = Parts[2] = 0;

    c = Text;
    if ((*c == L'v') || (*c == L'V'))
    {
        c++;
    }

    for (int part = 0; part < 3; part++)
    {
        if ((*c < L'0') || (*c > L'9'))
        {
            return;
        }

        while ((*c >= L'0') && (*c <= L'9'))
        {
            Parts[part] = (Parts[part] * 10) + SCAST(uint32_t)(*c - L'0');
            c++;
        }

        if (*c != L'.')
        {
            return;
        }

        c++;
    }
}


/*!

    @brief Compares two versions written as numbers between dots, as in
           "2.0.0" or "v2.1".

    @details A leading v is ignored, and a missing part counts as zero, so
             "2.1" and "2.1.0" are the same. Anything after the numbers, as in
             "2.1.0-beta", is ignored.

    @param[in] A - One version.

    @param[in] B - The other.

    @return Less than zero if A is older, zero if they're the same, greater than
            zero if A is newer.

*/
int
CompareVersions (
    _In_z_ PCWSTR A,
    _In_z_ PCWSTR B
    )
{
    uint32_t a[3];
    uint32_t b[3];

    ReadVersion(A, a);
    ReadVersion(B, b);

    for (int part = 0; part < 3; part++)
    {
        if (a[part] != b[part])
        {
            return (a[part] < b[part]) ? -1 : 1;
        }
    }

    return 0;
}
