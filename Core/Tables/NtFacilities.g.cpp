/*!

    @file Core/Tables/NtFacilities.g.cpp

    @brief NTSTATUS facility codes, generated from ntstatus.h.

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
    { 0x00000001u, 0, "FACILITY_DEBUGGER", nullptr },
    { 0x00000002u, 0, "FACILITY_RPC_RUNTIME", nullptr },
    { 0x00000003u, 0, "FACILITY_RPC_STUBS", nullptr },
    { 0x00000004u, 0, "FACILITY_IO_ERROR_CODE", nullptr },
    { 0x00000006u, 0, "FACILITY_CODCLASS_ERROR_CODE", nullptr },
    { 0x00000007u, 0, "FACILITY_NTWIN32", nullptr },
    { 0x00000008u, 0, "FACILITY_NTCERT", nullptr },
    { 0x00000009u, 0, "FACILITY_NTSSPI", nullptr },
    { 0x0000000Au, 0, "FACILITY_TERMINAL_SERVER", nullptr },
    { 0x00000010u, 0, "FACILITY_USB_ERROR_CODE", nullptr },
    { 0x00000011u, 0, "FACILITY_HID_ERROR_CODE", nullptr },
    { 0x00000012u, 0, "FACILITY_FIREWIRE_ERROR_CODE", nullptr },
    { 0x00000013u, 0, "FACILITY_CLUSTER_ERROR_CODE", nullptr },
    { 0x00000014u, 0, "FACILITY_ACPI_ERROR_CODE", nullptr },
    { 0x00000015u, 0, "FACILITY_SXS_ERROR_CODE", nullptr },
    { 0x00000019u, 0, "FACILITY_TRANSACTION", nullptr },
    { 0x0000001Au, 0, "FACILITY_COMMONLOG", nullptr },
    { 0x0000001Bu, 0, "FACILITY_VIDEO", nullptr },
    { 0x0000001Cu, 0, "FACILITY_FILTER_MANAGER", nullptr },
    { 0x0000001Du, 0, "FACILITY_MONITOR", nullptr },
    { 0x0000001Eu, 0, "FACILITY_GRAPHICS_KERNEL", nullptr },
    { 0x0000001Fu, 0, "FACILITY_CAMERA", nullptr },
    { 0x00000020u, 0, "FACILITY_DRIVER_FRAMEWORK", nullptr },
    { 0x00000021u, 0, "FACILITY_FVE_ERROR_CODE", nullptr },
    { 0x00000022u, 0, "FACILITY_FWP_ERROR_CODE", nullptr },
    { 0x00000023u, 0, "FACILITY_NDIS_ERROR_CODE", nullptr },
    { 0x00000024u, 0, "FACILITY_QUIC_ERROR_CODE", nullptr },
    { 0x00000029u, 0, "FACILITY_TPM", nullptr },
    { 0x0000002Au, 0, "FACILITY_RTPM", nullptr },
    { 0x00000035u, 0, "FACILITY_HYPERVISOR", nullptr },
    { 0x00000036u, 0, "FACILITY_IPSEC", nullptr },
    { 0x00000037u, 0, "FACILITY_VIRTUALIZATION", nullptr },
    { 0x00000038u, 0, "FACILITY_VOLMGR", nullptr },
    { 0x00000039u, 0, "FACILITY_BCD_ERROR_CODE", nullptr },
    { 0x0000003Eu, 0, "FACILITY_WIN32K_NTUSER", nullptr },
    { 0x0000003Fu, 0, "FACILITY_WIN32K_NTGDI", nullptr },
    { 0x00000040u, 0, "FACILITY_RESUME_KEY_FILTER", nullptr },
    { 0x00000041u, 0, "FACILITY_RDBSS", nullptr },
    { 0x00000042u, 0, "FACILITY_BTH_ATT", nullptr },
    { 0x00000043u, 0, "FACILITY_SECUREBOOT", nullptr },
    { 0x00000044u, 0, "FACILITY_AUDIO_KERNEL", nullptr },
    { 0x00000045u, 0, "FACILITY_VSM", nullptr },
    { 0x00000046u, 0, "FACILITY_NT_IORING", nullptr },
    { 0x00000050u, 0, "FACILITY_VOLSNAP", nullptr },
    { 0x00000051u, 0, "FACILITY_SDBUS", nullptr },
    { 0x0000005Cu, 0, "FACILITY_SHARED_VHDX", nullptr },
    { 0x0000005Du, 0, "FACILITY_SMB", nullptr },
    { 0x0000005Eu, 0, "FACILITY_XVS", nullptr },
    { 0x00000099u, 0, "FACILITY_INTERIX", nullptr },
    { 0x000000E7u, 0, "FACILITY_SPACES", nullptr },
    { 0x000000E8u, 0, "FACILITY_SECURITY_CORE", nullptr },
    { 0x000000E9u, 0, "FACILITY_SYSTEM_INTEGRITY", nullptr },
    { 0x000000EAu, 0, "FACILITY_LICENSING", nullptr },
    { 0x000000EBu, 0, "FACILITY_PLATFORM_MANIFEST", nullptr },
    { 0x000000ECu, 0, "FACILITY_APP_EXEC", nullptr },
    { 0x000000EDu, 0, "FACILITY_UNIONFS", nullptr },
    { 0x000000EEu, 0, "FACILITY_PLATFORM_RUNTIME_MECHANISM", nullptr },
    { 0x000000EFu, 0, "FACILITY_WIN_ACCEL", nullptr },
    { 0x000000F0u, 0, "FACILITY_MAXIMUM_VALUE", nullptr },
};

const TableSpan g_NtFacilityTable =
{
    g_Entries,
    ARRAYSIZE(g_Entries),
    L"ntstatus.h"
};
