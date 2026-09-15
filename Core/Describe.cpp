/*!

    @file Core/Describe.cpp

    @brief Turns parts of a match into text, the same way for every front end.

    @details The bits the CLI and GUI would otherwise both write, like how each
             kind of value's written, how fields read and how a related code's
             described. Layout (columns, wrapping, fonts) stays with each front
             end.

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


/*!

    @brief Formats a value the way its kind is usually written.

    @param[in] Kind - The kind of code.

    @param[in] Value - The value.

    @return Decimal for a Win32 error, four hex digits for a window message,
            eight for anything else.

*/
std::wstring
FormatCodeValue (
    _In_ CodeKind Kind,
    _In_ uint32_t Value
    )
{
    switch (Kind)
    {
    case CodeKind::WinError:
    case CodeKind::DeviceProblem:
    case CodeKind::LdapError:
        return std::to_wstring(Value);

    case CodeKind::WindowMessage:
        return std::format(L"0x{:04X}", Value);

    case CodeKind::KnownValue:
        return (Value <= 0xFFFF) ? std::format(L"0x{:04X}", Value) : std::format(L"0x{:08X}", Value);

    default:
        return std::format(L"0x{:08X}", Value);
    }
}


/*!

    @brief Describes the fields of an HRESULT, NTSTATUS or IOCTL, as in
           "failure, FACILITY_WIN32 (7), code 0x0005 (5)".

    @param[in] Fields - The fields.

    @return One line.

*/
std::wstring
DescribeFields (
    _In_ const FieldLayout& Fields
    )
{
    std::wstring text;

    if (Fields.Layout == FieldLayoutKind::Ioctl)
    {
        return DescribeIoctlFields(Fields);
    }

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


/*!

    @brief The label for how a code relates, in lower case, as in "maps to".

    @param[in] Relation - How it relates.

    @return A static, never null, label.

*/
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


/*!

    @brief Describes a related code, as in "Win32 error 5 ERROR_ACCESS_DENIED
           (HRESULT_FROM_WIN32)".

    @details The kind goes in front, except for an HRESULT form where the label
             already says it. Otherwise an unnamed code inside an HRESULT would
             just be a number.

    @param[in] Related - The related code.

    @return One line.

*/
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
