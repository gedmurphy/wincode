/*!

    @file Core/Tables/HResultFacilities.g.cpp

    @brief HRESULT facility codes, generated from winerror.h.

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
    { 0x00000000u, 0, "FACILITY_NULL", nullptr },
    { 0x00000001u, 0, "FACILITY_RPC", nullptr },
    { 0x00000002u, 0, "FACILITY_DISPATCH", nullptr },
    { 0x00000003u, 0, "FACILITY_STORAGE", nullptr },
    { 0x00000004u, 0, "FACILITY_ITF", nullptr },
    { 0x00000007u, 0, "FACILITY_WIN32", nullptr },
    { 0x00000008u, 0, "FACILITY_WINDOWS", nullptr },
    { 0x00000009u, 0, "FACILITY_SSPI", nullptr },
    { 0x00000009u, k_TableEntryAlias, "FACILITY_SECURITY", nullptr },
    { 0x0000000Au, 0, "FACILITY_CONTROL", nullptr },
    { 0x0000000Bu, 0, "FACILITY_CERT", nullptr },
    { 0x0000000Cu, 0, "FACILITY_INTERNET", nullptr },
    { 0x0000000Du, 0, "FACILITY_MEDIASERVER", nullptr },
    { 0x0000000Eu, 0, "FACILITY_MSMQ", nullptr },
    { 0x0000000Fu, 0, "FACILITY_SETUPAPI", nullptr },
    { 0x00000010u, 0, "FACILITY_SCARD", nullptr },
    { 0x00000011u, 0, "FACILITY_COMPLUS", nullptr },
    { 0x00000012u, 0, "FACILITY_AAF", nullptr },
    { 0x00000013u, 0, "FACILITY_URT", nullptr },
    { 0x00000014u, 0, "FACILITY_ACS", nullptr },
    { 0x00000015u, 0, "FACILITY_DPLAY", nullptr },
    { 0x00000016u, 0, "FACILITY_UMI", nullptr },
    { 0x00000017u, 0, "FACILITY_SXS", nullptr },
    { 0x00000018u, 0, "FACILITY_WINDOWS_CE", nullptr },
    { 0x00000019u, 0, "FACILITY_HTTP", nullptr },
    { 0x0000001Au, 0, "FACILITY_USERMODE_COMMONLOG", nullptr },
    { 0x0000001Bu, 0, "FACILITY_WER", nullptr },
    { 0x0000001Fu, 0, "FACILITY_USERMODE_FILTER_MANAGER", nullptr },
    { 0x00000020u, 0, "FACILITY_BACKGROUNDCOPY", nullptr },
    { 0x00000021u, 0, "FACILITY_CONFIGURATION", nullptr },
    { 0x00000021u, k_TableEntryAlias, "FACILITY_WIA", nullptr },
    { 0x00000022u, 0, "FACILITY_STATE_MANAGEMENT", nullptr },
    { 0x00000023u, 0, "FACILITY_METADIRECTORY", nullptr },
    { 0x00000024u, 0, "FACILITY_WINDOWSUPDATE", nullptr },
    { 0x00000025u, 0, "FACILITY_DIRECTORYSERVICE", nullptr },
    { 0x00000026u, 0, "FACILITY_GRAPHICS", nullptr },
    { 0x00000027u, 0, "FACILITY_SHELL", nullptr },
    { 0x00000027u, k_TableEntryAlias, "FACILITY_NAP", nullptr },
    { 0x00000028u, 0, "FACILITY_TPM_SERVICES", nullptr },
    { 0x00000029u, 0, "FACILITY_TPM_SOFTWARE", nullptr },
    { 0x0000002Au, 0, "FACILITY_UI", nullptr },
    { 0x0000002Bu, 0, "FACILITY_XAML", nullptr },
    { 0x0000002Cu, 0, "FACILITY_ACTION_QUEUE", nullptr },
    { 0x00000030u, 0, "FACILITY_PLA", nullptr },
    { 0x00000030u, k_TableEntryAlias, "FACILITY_WINDOWS_SETUP", nullptr },
    { 0x00000031u, 0, "FACILITY_FVE", nullptr },
    { 0x00000032u, 0, "FACILITY_FWP", nullptr },
    { 0x00000033u, 0, "FACILITY_WINRM", nullptr },
    { 0x00000034u, 0, "FACILITY_NDIS", nullptr },
    { 0x00000035u, 0, "FACILITY_USERMODE_HYPERVISOR", nullptr },
    { 0x00000036u, 0, "FACILITY_CMI", nullptr },
    { 0x00000037u, 0, "FACILITY_USERMODE_VIRTUALIZATION", nullptr },
    { 0x00000038u, 0, "FACILITY_USERMODE_VOLMGR", nullptr },
    { 0x00000039u, 0, "FACILITY_BCD", nullptr },
    { 0x0000003Au, 0, "FACILITY_USERMODE_VHD", nullptr },
    { 0x0000003Bu, 0, "FACILITY_USERMODE_HNS", nullptr },
    { 0x0000003Cu, 0, "FACILITY_SDIAG", nullptr },
    { 0x0000003Du, 0, "FACILITY_WEBSERVICES", nullptr },
    { 0x0000003Du, k_TableEntryAlias, "FACILITY_WINPE", nullptr },
    { 0x0000003Eu, 0, "FACILITY_WPN", nullptr },
    { 0x0000003Fu, 0, "FACILITY_WINDOWS_STORE", nullptr },
    { 0x00000040u, 0, "FACILITY_INPUT", nullptr },
    { 0x00000041u, 0, "FACILITY_QUIC", nullptr },
    { 0x00000042u, 0, "FACILITY_EAP", nullptr },
    { 0x00000046u, 0, "FACILITY_IORING", nullptr },
    { 0x00000050u, 0, "FACILITY_WINDOWS_DEFENDER", nullptr },
    { 0x00000051u, 0, "FACILITY_OPC", nullptr },
    { 0x00000052u, 0, "FACILITY_XPS", nullptr },
    { 0x00000053u, 0, "FACILITY_RAS", nullptr },
    { 0x00000054u, 0, "FACILITY_MBN", nullptr },
    { 0x00000054u, k_TableEntryAlias, "FACILITY_POWERSHELL", nullptr },
    { 0x00000055u, 0, "FACILITY_EAS", nullptr },
    { 0x00000062u, 0, "FACILITY_P2P_INT", nullptr },
    { 0x00000063u, 0, "FACILITY_P2P", nullptr },
    { 0x00000064u, 0, "FACILITY_DAF", nullptr },
    { 0x00000065u, 0, "FACILITY_BLUETOOTH_ATT", nullptr },
    { 0x00000066u, 0, "FACILITY_AUDIO", nullptr },
    { 0x00000067u, 0, "FACILITY_STATEREPOSITORY", nullptr },
    { 0x0000006Du, 0, "FACILITY_VISUALCPP", nullptr },
    { 0x00000070u, 0, "FACILITY_SCRIPT", nullptr },
    { 0x00000071u, 0, "FACILITY_PARSE", nullptr },
    { 0x00000078u, 0, "FACILITY_BLB", nullptr },
    { 0x00000079u, 0, "FACILITY_BLB_CLI", nullptr },
    { 0x0000007Au, 0, "FACILITY_WSBAPP", nullptr },
    { 0x00000080u, 0, "FACILITY_BLBUI", nullptr },
    { 0x00000081u, 0, "FACILITY_USN", nullptr },
    { 0x00000082u, 0, "FACILITY_USERMODE_VOLSNAP", nullptr },
    { 0x00000083u, 0, "FACILITY_TIERING", nullptr },
    { 0x00000085u, 0, "FACILITY_WSB_ONLINE", nullptr },
    { 0x00000086u, 0, "FACILITY_ONLINE_ID", nullptr },
    { 0x00000087u, 0, "FACILITY_DEVICE_UPDATE_AGENT", nullptr },
    { 0x00000088u, 0, "FACILITY_DRVSERVICING", nullptr },
    { 0x00000099u, 0, "FACILITY_DLS", nullptr },
    { 0x000000A0u, 0, "FACILITY_SOS", nullptr },
    { 0x000000ADu, 0, "FACILITY_OCP_UPDATE_AGENT", nullptr },
    { 0x000000B0u, 0, "FACILITY_DEBUGGERS", nullptr },
    { 0x000000D0u, 0, "FACILITY_DELIVERY_OPTIMIZATION", nullptr },
    { 0x000000E7u, 0, "FACILITY_USERMODE_SPACES", nullptr },
    { 0x000000E8u, 0, "FACILITY_USER_MODE_SECURITY_CORE", nullptr },
    { 0x000000EAu, 0, "FACILITY_USERMODE_LICENSING", nullptr },
    { 0x00000100u, 0, "FACILITY_SPP", nullptr },
    { 0x00000100u, k_TableEntryAlias, "FACILITY_RESTORE", nullptr },
    { 0x00000100u, k_TableEntryAlias, "FACILITY_DMSERVER", nullptr },
    { 0x00000101u, 0, "FACILITY_DEPLOYMENT_SERVICES_SERVER", nullptr },
    { 0x00000102u, 0, "FACILITY_DEPLOYMENT_SERVICES_IMAGING", nullptr },
    { 0x00000103u, 0, "FACILITY_DEPLOYMENT_SERVICES_MANAGEMENT", nullptr },
    { 0x00000104u, 0, "FACILITY_DEPLOYMENT_SERVICES_UTIL", nullptr },
    { 0x00000105u, 0, "FACILITY_DEPLOYMENT_SERVICES_BINLSVC", nullptr },
    { 0x00000107u, 0, "FACILITY_DEPLOYMENT_SERVICES_PXE", nullptr },
    { 0x00000108u, 0, "FACILITY_DEPLOYMENT_SERVICES_TFTP", nullptr },
    { 0x00000110u, 0, "FACILITY_DEPLOYMENT_SERVICES_TRANSPORT_MANAGEMENT", nullptr },
    { 0x00000116u, 0, "FACILITY_DEPLOYMENT_SERVICES_DRIVER_PROVISIONING", nullptr },
    { 0x00000121u, 0, "FACILITY_DEPLOYMENT_SERVICES_MULTICAST_SERVER", nullptr },
    { 0x00000122u, 0, "FACILITY_DEPLOYMENT_SERVICES_MULTICAST_CLIENT", nullptr },
    { 0x00000125u, 0, "FACILITY_DEPLOYMENT_SERVICES_CONTENT_PROVIDER", nullptr },
    { 0x00000128u, 0, "FACILITY_HSP_SERVICES", nullptr },
    { 0x00000129u, 0, "FACILITY_HSP_SOFTWARE", nullptr },
    { 0x00000131u, 0, "FACILITY_LINGUISTIC_SERVICES", nullptr },
    { 0x00000375u, 0, "FACILITY_WEB", nullptr },
    { 0x00000376u, 0, "FACILITY_WEB_SOCKET", nullptr },
    { 0x00000446u, 0, "FACILITY_AUDIOSTREAMING", nullptr },
    { 0x000005D2u, 0, "FACILITY_TTD", nullptr },
    { 0x00000600u, 0, "FACILITY_ACCELERATOR", nullptr },
    { 0x00000701u, 0, "FACILITY_MOBILE", nullptr },
    { 0x000007AFu, 0, "FACILITY_SQLITE", nullptr },
    { 0x000007B0u, 0, "FACILITY_SERVICE_FABRIC", nullptr },
    { 0x000007C5u, 0, "FACILITY_UTC", nullptr },
    { 0x000007CCu, 0, "FACILITY_WMAAECMA", nullptr },
    { 0x00000801u, 0, "FACILITY_WEP", nullptr },
    { 0x00000802u, 0, "FACILITY_SYNCENGINE", nullptr },
    { 0x00000878u, 0, "FACILITY_DIRECTMUSIC", nullptr },
    { 0x00000879u, 0, "FACILITY_DIRECT3D10", nullptr },
    { 0x0000087Au, 0, "FACILITY_DXGI", nullptr },
    { 0x0000087Bu, 0, "FACILITY_DXGI_DDI", nullptr },
    { 0x0000087Cu, 0, "FACILITY_DIRECT3D11", nullptr },
    { 0x0000087Du, 0, "FACILITY_DIRECT3D11_DEBUG", nullptr },
    { 0x0000087Eu, 0, "FACILITY_DIRECT3D12", nullptr },
    { 0x0000087Fu, 0, "FACILITY_DIRECT3D12_DEBUG", nullptr },
    { 0x00000880u, 0, "FACILITY_DXCORE", nullptr },
    { 0x00000881u, 0, "FACILITY_PRESENTATION", nullptr },
    { 0x00000888u, 0, "FACILITY_LEAP", nullptr },
    { 0x00000889u, 0, "FACILITY_AUDCLNT", nullptr },
    { 0x00000890u, 0, "FACILITY_WINML", nullptr },
    { 0x00000898u, 0, "FACILITY_WINCODEC_DWRITE_DWM", nullptr },
    { 0x00000899u, 0, "FACILITY_DIRECT2D", nullptr },
    { 0x00000900u, 0, "FACILITY_DEFRAG", nullptr },
    { 0x00000901u, 0, "FACILITY_USERMODE_SDBUS", nullptr },
    { 0x00000902u, 0, "FACILITY_JSCRIPT", nullptr },
    { 0x00000923u, 0, "FACILITY_XBOX", nullptr },
    { 0x00000924u, 0, "FACILITY_GAME", nullptr },
    { 0x00000925u, 0, "FACILITY_USERMODE_UNIONFS", nullptr },
    { 0x00000926u, 0, "FACILITY_USERMODE_PRM", nullptr },
    { 0x00000927u, 0, "FACILITY_USERMODE_WIN_ACCEL", nullptr },
    { 0x00000928u, 0, "FACILITY_PPF", nullptr },
    { 0x00000A01u, 0, "FACILITY_PIDGENX", nullptr },
    { 0x00000ABCu, 0, "FACILITY_PIX", nullptr },
};

const TableSpan g_HResultFacilityTable =
{
    g_Entries,
    ARRAYSIZE(g_Entries),
    L"winerror.h"
};
