/*!

    @file Core/Ioctl.cpp

    @brief Takes I/O control codes apart, and decodes a value as one when
           asked to with "ioctl 0x2D1400".

    @details Any 32-bit value reads as a CTL_CODE, so a plain lookup only
             shows IOCTLs a header names. Asking for one decodes the value
             whatever it is.

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

#include "Tables.hpp"

#include <format>


//
// Device types from 0x8000 up, and functions from 0x800 up, are kept for
// vendors, so they mean whatever their driver says.
//
static constexpr uint32_t k_VendorDeviceType = 0x8000;
static constexpr uint32_t k_VendorFunction = 0x800;

static constexpr PCWSTR k_Methods[] =
{
    L"METHOD_BUFFERED",
    L"METHOD_IN_DIRECT",
    L"METHOD_OUT_DIRECT",
    L"METHOD_NEITHER"
};

static constexpr PCWSTR k_Accesses[] =
{
    L"FILE_ANY_ACCESS",
    L"FILE_READ_ACCESS",
    L"FILE_WRITE_ACCESS",
    L"FILE_READ_ACCESS | FILE_WRITE_ACCESS"
};


/*!

    @brief Takes a value apart as an IOCTL.

    @param[in] Value - The value.

    @return The fields, with the device type named unless it's a vendor's.

*/
FieldLayout
IoctlFields (
    _In_ uint32_t Value
    )
{
    const TableEntry* entry;
    FieldLayout fields;

    fields.Layout = FieldLayoutKind::Ioctl;
    fields.Severity = 0;
    fields.NtBit = false;
    fields.Facility = Value >> 16;
    fields.Customer = fields.Facility >= k_VendorDeviceType;
    fields.Access = (Value >> 14) & 3;
    fields.Code = (Value >> 2) & 0xFFF;
    fields.Method = Value & 3;

    if (!fields.Customer)
    {
        entry = TableFindFirst(g_IoctlDeviceTypeTable, fields.Facility);
        if (entry != nullptr)
        {
            fields.FacilityName = AsciiToWide(entry->Name);
        }
    }

    return fields;
}


/*!

    @brief Describes an IOCTL's fields, as in "FILE_DEVICE_DISK (0x7),
           function 0x400 (1024), METHOD_BUFFERED, FILE_ANY_ACCESS".

    @param[in] Fields - The fields.

    @return One line.

*/
std::wstring
DescribeIoctlFields (
    _In_ const FieldLayout& Fields
    )
{
    std::wstring text;

    if (!Fields.FacilityName.empty())
    {
        text = std::format(L"{} (0x{:X})", Fields.FacilityName, Fields.Facility);
    }
    else
    {
        text = std::format(L"device type 0x{:X}", Fields.Facility);
    }

    if (Fields.Customer)
    {
        text += L", vendor device type";
    }

    text += std::format(L", function 0x{:X} ({})", Fields.Code, Fields.Code);

    if (Fields.Code >= k_VendorFunction)
    {
        text += L", vendor function";
    }

    text += std::format(L", {}, {}", k_Methods[Fields.Method & 3], k_Accesses[Fields.Access & 3]);
    return text;
}


/*!

    @brief Reads a query that asks for an IOCTL decode: "ioctl" or "fsctl",
           then one number.

    @param[in] Query - The query.

    @param[out] Value - Receives the value.

    @return True if the query asks for one.

*/
bool
ParseIoctlQuery (
    _In_z_ PCWSTR Query,
    _Out_ uint32_t* Value
    )
{
    ParsedQuery parsed;
    std::wstring query;
    std::wstring word;
    std::wstring number;
    size_t start;
    size_t split;
    size_t end;

    *Value = 0;

    query = Query;
    start = query.find_first_not_of(L" \t");
    end = query.find_last_not_of(L" \t");
    if (start == std::wstring::npos)
    {
        return false;
    }

    query = query.substr(start, end - start + 1);

    split = query.find_first_of(L" \t:");
    if (split == std::wstring::npos)
    {
        return false;
    }

    word = query.substr(0, split);
    if ((_wcsicmp(word.c_str(), L"ioctl") != 0) && (_wcsicmp(word.c_str(), L"fsctl") != 0))
    {
        return false;
    }

    start = query.find_first_not_of(L" \t:", split);
    number = (start == std::wstring::npos) ? std::wstring() : query.substr(start);
    if (number.empty() || (number.find_first_of(L" \t") != std::wstring::npos))
    {
        return false;
    }

    parsed = ParseQuery(number.c_str());
    if (parsed.Numbers.empty() || !parsed.Names.empty())
    {
        return false;
    }

    //
    // IOCTLs are written in hex, so where eight digits read two ways, take
    // the hex one.
    //
    *Value = parsed.Numbers[0].Value;
    for (const ParsedNumber& reading : parsed.Numbers)
    {
        if (reading.Form == NumberForm::HexGuess)
        {
            *Value = reading.Value;
        }
    }

    return true;
}


/*!

    @brief Decodes a value as an IOCTL, named if a header names it.

    @details An unnamed one is called by the CTL_CODE that makes it, which
             is what you'd write to get it, and says everything the fields
             do.

    @param[in] Value - The value.

    @param[out] Match - Receives the decode.

*/
void
DecodeIoctl (
    _In_ uint32_t Value,
    _Out_ CodeMatch* Match
    )
{
    std::vector<CodeMatch> matches;
    std::wstring deviceType;
    FieldLayout fields;

    matches = LookupValue(Value);
    for (CodeMatch& match : matches)
    {
        if (match.Kind == CodeKind::Ioctl)
        {
            *Match = std::move(match);
            return;
        }
    }

    fields = IoctlFields(Value);
    deviceType = fields.FacilityName.empty() ? std::format(L"0x{:X}", fields.Facility) : fields.FacilityName;

    *Match = CodeMatch();
    Match->Kind = CodeKind::Ioctl;
    Match->Value = Value;
    Match->Names.push_back(std::format(L"CTL_CODE({}, 0x{:X}, {}, {})",
                                       deviceType,
                                       fields.Code,
                                       k_Methods[fields.Method],
                                       k_Accesses[fields.Access]));
    Match->Source = TextSource::None;
    Match->Fields = fields;
}
