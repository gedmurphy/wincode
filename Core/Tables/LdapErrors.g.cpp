/*!

    @file Core/Tables/LdapErrors.g.cpp

    @brief LDAP result codes, generated from winldap.h.

    @details DO NOT EDIT. Regenerate with tools/generate_tables.py.

             Generated from Windows SDK 10.0.28000.0. Sorted by value, with
             the primary name for each value first, so a lookup can
             binary search to it and walk forward over the aliases.

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


static const TableEntry g_Entries[] =
{
    { 0x00000000u, 0, "LDAP_SUCCESS", nullptr },
    { 0x00000001u, 0, "LDAP_OPERATIONS_ERROR", nullptr },
    { 0x00000002u, 0, "LDAP_PROTOCOL_ERROR", nullptr },
    { 0x00000003u, 0, "LDAP_TIMELIMIT_EXCEEDED", nullptr },
    { 0x00000004u, 0, "LDAP_SIZELIMIT_EXCEEDED", nullptr },
    { 0x00000005u, 0, "LDAP_COMPARE_FALSE", nullptr },
    { 0x00000006u, 0, "LDAP_COMPARE_TRUE", nullptr },
    { 0x00000007u, 0, "LDAP_AUTH_METHOD_NOT_SUPPORTED", nullptr },
    { 0x00000008u, 0, "LDAP_STRONG_AUTH_REQUIRED", nullptr },
    { 0x00000009u, 0, "LDAP_REFERRAL_V2", nullptr },
    { 0x00000009u, k_TableEntryAlias, "LDAP_PARTIAL_RESULTS", nullptr },
    { 0x0000000Au, 0, "LDAP_REFERRAL", nullptr },
    { 0x0000000Bu, 0, "LDAP_ADMIN_LIMIT_EXCEEDED", nullptr },
    { 0x0000000Cu, 0, "LDAP_UNAVAILABLE_CRIT_EXTENSION", nullptr },
    { 0x0000000Du, 0, "LDAP_CONFIDENTIALITY_REQUIRED", nullptr },
    { 0x0000000Eu, 0, "LDAP_SASL_BIND_IN_PROGRESS", nullptr },
    { 0x00000010u, 0, "LDAP_NO_SUCH_ATTRIBUTE", nullptr },
    { 0x00000011u, 0, "LDAP_UNDEFINED_TYPE", nullptr },
    { 0x00000012u, 0, "LDAP_INAPPROPRIATE_MATCHING", nullptr },
    { 0x00000013u, 0, "LDAP_CONSTRAINT_VIOLATION", nullptr },
    { 0x00000014u, 0, "LDAP_ATTRIBUTE_OR_VALUE_EXISTS", nullptr },
    { 0x00000015u, 0, "LDAP_INVALID_SYNTAX", nullptr },
    { 0x00000020u, 0, "LDAP_NO_SUCH_OBJECT", nullptr },
    { 0x00000021u, 0, "LDAP_ALIAS_PROBLEM", nullptr },
    { 0x00000022u, 0, "LDAP_INVALID_DN_SYNTAX", nullptr },
    { 0x00000023u, 0, "LDAP_IS_LEAF", nullptr },
    { 0x00000024u, 0, "LDAP_ALIAS_DEREF_PROBLEM", nullptr },
    { 0x00000030u, 0, "LDAP_INAPPROPRIATE_AUTH", nullptr },
    { 0x00000031u, 0, "LDAP_INVALID_CREDENTIALS", nullptr },
    { 0x00000032u, 0, "LDAP_INSUFFICIENT_RIGHTS", nullptr },
    { 0x00000033u, 0, "LDAP_BUSY", nullptr },
    { 0x00000034u, 0, "LDAP_UNAVAILABLE", nullptr },
    { 0x00000035u, 0, "LDAP_UNWILLING_TO_PERFORM", nullptr },
    { 0x00000036u, 0, "LDAP_LOOP_DETECT", nullptr },
    { 0x0000003Cu, 0, "LDAP_SORT_CONTROL_MISSING", nullptr },
    { 0x0000003Du, 0, "LDAP_OFFSET_RANGE_ERROR", nullptr },
    { 0x00000040u, 0, "LDAP_NAMING_VIOLATION", nullptr },
    { 0x00000041u, 0, "LDAP_OBJECT_CLASS_VIOLATION", nullptr },
    { 0x00000042u, 0, "LDAP_NOT_ALLOWED_ON_NONLEAF", nullptr },
    { 0x00000043u, 0, "LDAP_NOT_ALLOWED_ON_RDN", nullptr },
    { 0x00000044u, 0, "LDAP_ALREADY_EXISTS", nullptr },
    { 0x00000045u, 0, "LDAP_NO_OBJECT_CLASS_MODS", nullptr },
    { 0x00000046u, 0, "LDAP_RESULTS_TOO_LARGE", nullptr },
    { 0x00000047u, 0, "LDAP_AFFECTS_MULTIPLE_DSAS", nullptr },
    { 0x0000004Cu, 0, "LDAP_VIRTUAL_LIST_VIEW_ERROR", nullptr },
    { 0x00000050u, 0, "LDAP_OTHER", nullptr },
    { 0x00000051u, 0, "LDAP_SERVER_DOWN", nullptr },
    { 0x00000052u, 0, "LDAP_LOCAL_ERROR", nullptr },
    { 0x00000053u, 0, "LDAP_ENCODING_ERROR", nullptr },
    { 0x00000054u, 0, "LDAP_DECODING_ERROR", nullptr },
    { 0x00000055u, 0, "LDAP_TIMEOUT", nullptr },
    { 0x00000056u, 0, "LDAP_AUTH_UNKNOWN", nullptr },
    { 0x00000057u, 0, "LDAP_FILTER_ERROR", nullptr },
    { 0x00000058u, 0, "LDAP_USER_CANCELLED", nullptr },
    { 0x00000059u, 0, "LDAP_PARAM_ERROR", nullptr },
    { 0x0000005Au, 0, "LDAP_NO_MEMORY", nullptr },
    { 0x0000005Bu, 0, "LDAP_CONNECT_ERROR", nullptr },
    { 0x0000005Cu, 0, "LDAP_NOT_SUPPORTED", nullptr },
    { 0x0000005Du, 0, "LDAP_CONTROL_NOT_FOUND", nullptr },
    { 0x0000005Eu, 0, "LDAP_NO_RESULTS_RETURNED", nullptr },
    { 0x0000005Fu, 0, "LDAP_MORE_RESULTS_TO_RETURN", nullptr },
    { 0x00000060u, 0, "LDAP_CLIENT_LOOP", nullptr },
    { 0x00000061u, 0, "LDAP_REFERRAL_LIMIT_EXCEEDED", nullptr },
};

const TableSpan g_LdapErrorTable =
{
    g_Entries,
    ARRAYSIZE(g_Entries),
    L"winldap.h"
};
