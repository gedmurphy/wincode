/*!

    @file Core/Tables/IoctlDeviceTypes.g.cpp

    @brief IOCTL device types, generated from devioctl.h.

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
    { 0x00000001u, 0, "FILE_DEVICE_BEEP", nullptr },
    { 0x00000002u, 0, "FILE_DEVICE_CD_ROM", nullptr },
    { 0x00000003u, 0, "FILE_DEVICE_CD_ROM_FILE_SYSTEM", nullptr },
    { 0x00000004u, 0, "FILE_DEVICE_CONTROLLER", nullptr },
    { 0x00000005u, 0, "FILE_DEVICE_DATALINK", nullptr },
    { 0x00000006u, 0, "FILE_DEVICE_DFS", nullptr },
    { 0x00000007u, 0, "FILE_DEVICE_DISK", nullptr },
    { 0x00000008u, 0, "FILE_DEVICE_DISK_FILE_SYSTEM", nullptr },
    { 0x00000009u, 0, "FILE_DEVICE_FILE_SYSTEM", nullptr },
    { 0x0000000Au, 0, "FILE_DEVICE_INPORT_PORT", nullptr },
    { 0x0000000Bu, 0, "FILE_DEVICE_KEYBOARD", nullptr },
    { 0x0000000Cu, 0, "FILE_DEVICE_MAILSLOT", nullptr },
    { 0x0000000Du, 0, "FILE_DEVICE_MIDI_IN", nullptr },
    { 0x0000000Eu, 0, "FILE_DEVICE_MIDI_OUT", nullptr },
    { 0x0000000Fu, 0, "FILE_DEVICE_MOUSE", nullptr },
    { 0x00000010u, 0, "FILE_DEVICE_MULTI_UNC_PROVIDER", nullptr },
    { 0x00000011u, 0, "FILE_DEVICE_NAMED_PIPE", nullptr },
    { 0x00000012u, 0, "FILE_DEVICE_NETWORK", nullptr },
    { 0x00000013u, 0, "FILE_DEVICE_NETWORK_BROWSER", nullptr },
    { 0x00000014u, 0, "FILE_DEVICE_NETWORK_FILE_SYSTEM", nullptr },
    { 0x00000015u, 0, "FILE_DEVICE_NULL", nullptr },
    { 0x00000016u, 0, "FILE_DEVICE_PARALLEL_PORT", nullptr },
    { 0x00000017u, 0, "FILE_DEVICE_PHYSICAL_NETCARD", nullptr },
    { 0x00000018u, 0, "FILE_DEVICE_PRINTER", nullptr },
    { 0x00000019u, 0, "FILE_DEVICE_SCANNER", nullptr },
    { 0x0000001Au, 0, "FILE_DEVICE_SERIAL_MOUSE_PORT", nullptr },
    { 0x0000001Bu, 0, "FILE_DEVICE_SERIAL_PORT", nullptr },
    { 0x0000001Cu, 0, "FILE_DEVICE_SCREEN", nullptr },
    { 0x0000001Du, 0, "FILE_DEVICE_SOUND", nullptr },
    { 0x0000001Eu, 0, "FILE_DEVICE_STREAMS", nullptr },
    { 0x0000001Fu, 0, "FILE_DEVICE_TAPE", nullptr },
    { 0x00000020u, 0, "FILE_DEVICE_TAPE_FILE_SYSTEM", nullptr },
    { 0x00000021u, 0, "FILE_DEVICE_TRANSPORT", nullptr },
    { 0x00000022u, 0, "FILE_DEVICE_UNKNOWN", nullptr },
    { 0x00000023u, 0, "FILE_DEVICE_VIDEO", nullptr },
    { 0x00000024u, 0, "FILE_DEVICE_VIRTUAL_DISK", nullptr },
    { 0x00000025u, 0, "FILE_DEVICE_WAVE_IN", nullptr },
    { 0x00000026u, 0, "FILE_DEVICE_WAVE_OUT", nullptr },
    { 0x00000027u, 0, "FILE_DEVICE_8042_PORT", nullptr },
    { 0x00000028u, 0, "FILE_DEVICE_NETWORK_REDIRECTOR", nullptr },
    { 0x00000029u, 0, "FILE_DEVICE_BATTERY", nullptr },
    { 0x0000002Au, 0, "FILE_DEVICE_BUS_EXTENDER", nullptr },
    { 0x0000002Bu, 0, "FILE_DEVICE_MODEM", nullptr },
    { 0x0000002Cu, 0, "FILE_DEVICE_VDM", nullptr },
    { 0x0000002Du, 0, "FILE_DEVICE_MASS_STORAGE", nullptr },
    { 0x0000002Eu, 0, "FILE_DEVICE_SMB", nullptr },
    { 0x0000002Fu, 0, "FILE_DEVICE_KS", nullptr },
    { 0x00000030u, 0, "FILE_DEVICE_CHANGER", nullptr },
    { 0x00000031u, 0, "FILE_DEVICE_SMARTCARD", nullptr },
    { 0x00000032u, 0, "FILE_DEVICE_ACPI", nullptr },
    { 0x00000033u, 0, "FILE_DEVICE_DVD", nullptr },
    { 0x00000034u, 0, "FILE_DEVICE_FULLSCREEN_VIDEO", nullptr },
    { 0x00000035u, 0, "FILE_DEVICE_DFS_FILE_SYSTEM", nullptr },
    { 0x00000036u, 0, "FILE_DEVICE_DFS_VOLUME", nullptr },
    { 0x00000037u, 0, "FILE_DEVICE_SERENUM", nullptr },
    { 0x00000038u, 0, "FILE_DEVICE_TERMSRV", nullptr },
    { 0x00000039u, 0, "FILE_DEVICE_KSEC", nullptr },
    { 0x0000003Au, 0, "FILE_DEVICE_FIPS", nullptr },
    { 0x0000003Bu, 0, "FILE_DEVICE_INFINIBAND", nullptr },
    { 0x0000003Eu, 0, "FILE_DEVICE_VMBUS", nullptr },
    { 0x0000003Fu, 0, "FILE_DEVICE_CRYPT_PROVIDER", nullptr },
    { 0x00000040u, 0, "FILE_DEVICE_WPD", nullptr },
    { 0x00000041u, 0, "FILE_DEVICE_BLUETOOTH", nullptr },
    { 0x00000042u, 0, "FILE_DEVICE_MT_COMPOSITE", nullptr },
    { 0x00000043u, 0, "FILE_DEVICE_MT_TRANSPORT", nullptr },
    { 0x00000044u, 0, "FILE_DEVICE_BIOMETRIC", nullptr },
    { 0x00000045u, 0, "FILE_DEVICE_PMI", nullptr },
    { 0x00000046u, 0, "FILE_DEVICE_EHSTOR", nullptr },
    { 0x00000047u, 0, "FILE_DEVICE_DEVAPI", nullptr },
    { 0x00000048u, 0, "FILE_DEVICE_GPIO", nullptr },
    { 0x00000049u, 0, "FILE_DEVICE_USBEX", nullptr },
    { 0x00000050u, 0, "FILE_DEVICE_CONSOLE", nullptr },
    { 0x00000051u, 0, "FILE_DEVICE_NFP", nullptr },
    { 0x00000052u, 0, "FILE_DEVICE_SYSENV", nullptr },
    { 0x00000053u, 0, "FILE_DEVICE_VIRTUAL_BLOCK", nullptr },
    { 0x00000054u, 0, "FILE_DEVICE_POINT_OF_SERVICE", nullptr },
    { 0x00000055u, 0, "FILE_DEVICE_STORAGE_REPLICATION", nullptr },
    { 0x00000056u, 0, "FILE_DEVICE_TRUST_ENV", nullptr },
    { 0x00000057u, 0, "FILE_DEVICE_UCM", nullptr },
    { 0x00000058u, 0, "FILE_DEVICE_UCMTCPCI", nullptr },
    { 0x00000059u, 0, "FILE_DEVICE_PERSISTENT_MEMORY", nullptr },
    { 0x0000005Au, 0, "FILE_DEVICE_NVDIMM", nullptr },
    { 0x0000005Bu, 0, "FILE_DEVICE_HOLOGRAPHIC", nullptr },
    { 0x0000005Cu, 0, "FILE_DEVICE_SDFXHCI", nullptr },
    { 0x0000005Du, 0, "FILE_DEVICE_UCMUCSI", nullptr },
    { 0x0000005Eu, 0, "FILE_DEVICE_PRM", nullptr },
    { 0x0000005Fu, 0, "FILE_DEVICE_EVENT_COLLECTOR", nullptr },
    { 0x00000060u, 0, "FILE_DEVICE_USB4", nullptr },
    { 0x00000061u, 0, "FILE_DEVICE_SOUNDWIRE", nullptr },
    { 0x00000062u, 0, "FILE_DEVICE_FABRIC_NVME", nullptr },
    { 0x00000063u, 0, "FILE_DEVICE_SVM", nullptr },
    { 0x00000064u, 0, "FILE_DEVICE_HARDWARE_ACCELERATOR", nullptr },
    { 0x00000065u, 0, "FILE_DEVICE_I3C", nullptr },
    { 0x00000066u, 0, "FILE_DEVICE_MULTITIER_MEMORY", nullptr },
    { 0x00000067u, 0, "FILE_DEVICE_CXL_TYPE3", nullptr },
};

const TableSpan g_IoctlDeviceTypeTable =
{
    g_Entries,
    ARRAYSIZE(g_Entries),
    L"devioctl.h"
};
