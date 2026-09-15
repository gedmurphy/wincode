/*!

    @file Tests/WinCodeFuzz.cpp

    @brief A libFuzzer target for everything that reads your text, so the
           query parser, lookups, name search and flag decoding.

    @details Only the fuzz preset builds it, with AddressSanitizer. Run it like
             WinCodeFuzz.exe -max_total_time=60 -max_len=512.

             We take the input's bytes as UTF-16, since every query is, so the
             fuzzer can reach any character, lone surrogates included.

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
// Name search reads every table, so only search the first few names to keep
// each run quick enough for the fuzzer to explore.
//
static constexpr size_t k_MaxNames = 2;


/*!

    @brief Runs one input through everything that reads a query.

    @param[in] Data - The input.

    @param[in] Size - Its size, in bytes.

    @return Zero, as libFuzzer requires.

*/
extern "C"
int
LLVMFuzzerTestOneInput (
    _In_reads_bytes_(Size) const uint8_t* Data,
    _In_ size_t Size
    )
{
    std::wstring query;
    std::wstring family;
    ParsedQuery parsed;
    CodeMatch match;
    uint32_t value;

    query.resize(Size / sizeof(WCHAR));
    if (!query.empty())
    {
        memcpy(query.data(), Data, query.size() * sizeof(WCHAR));
    }

    //
    // Every query is a C string by the time it's parsed, so an embedded null
    // ends it there.
    //
    query.resize(wcsnlen(query.c_str(), query.size()));

    parsed = ParseQuery(query.c_str());

    for (const ParsedNumber& number : parsed.Numbers)
    {
        SCAST(void)(LookupValue(number.Value));
    }

    for (size_t i = 0; (i < parsed.Names.size()) && (i < k_MaxNames); i++)
    {
        SCAST(void)(SearchNames(parsed.Names[i].c_str()));
    }

    if (ParseFlagQuery(query.c_str(), &family, &value))
    {
        SCAST(void)(DecodeFlags(family.c_str(), value, &match));
    }

    if (ParseIoctlQuery(query.c_str(), &value))
    {
        DecodeIoctl(value, &match);
    }

    //
    // The same input as a header, since a header is text from anywhere.
    //
    ReadHeaders({ HeaderSource{ L"fuzz.h", query, UserTreatAs::Automatic } });

    return 0;
}
