/*!

    @file Core/Describe.cpp

    @brief Turns parts of a match into text, the same way for every front end.

    @details These are the pieces the CLI and the GUI would otherwise each write
             for themselves: how a value of each kind is written, how fields
             read, and how a related code is described. Layout, such as columns,
             wrapping and fonts, stays with each front end.

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

#include <format>


std::wstring
FormatCodeValue (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    )
{
    switch (Kind)
    {
    case CodeKind::WinError:
        return std::to_wstring(Value);

    case CodeKind::WindowMessage:
        return std::format(L"0x{:04X}", Value);

    default:
        return std::format(L"0x{:08X}", Value);
    }
}


std::wstring
DescribeFields (
    _In_ const FieldLayout& Fields
    )
{
    std::wstring text;

    text = SeverityName(Fields);

    if (Fields.Customer)
    {
        text += L", customer";
    }

    if (Fields.NtBit)
    {
        text += L", N bit";
    }

    if (!Fields.FacilityName.empty())
    {
        text += std::format(L", {} ({})", Fields.FacilityName, Fields.Facility);
    }
    else
    {
        text += std::format(L", facility 0x{:X} ({})", Fields.Facility, Fields.Facility);
    }

    text += std::format(L", code 0x{:04X} ({})", Fields.Code, Fields.Code);
    return text;
}


PCWSTR
RelationLabel (
    _In_ RelationKind Relation
    )
{
    switch (Relation)
    {
    case RelationKind::AsHResult:
        return L"as HRESULT";

    case RelationKind::MapsTo:
        return L"maps to";

    case RelationKind::Wraps:
        return L"wraps";

    case RelationKind::MappedFrom:
        return L"mapped from";
    }

    return L"related";
}


std::wstring
DescribeRelated (
    _In_ const RelatedCode& Related
    )
{
    std::wstring text;

    if (Related.Relation != RelationKind::AsHResult)
    {
        text = CodeKindName(Related.Kind);
        text += L" ";
    }

    text += FormatCodeValue(Related.Kind, Related.Value);

    if (!Related.Name.empty())
    {
        text += L" " + Related.Name;
    }

    text += std::format(L" ({})", Related.Via);
    return text;
}
