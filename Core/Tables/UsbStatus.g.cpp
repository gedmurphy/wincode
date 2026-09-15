/*!

    @file Core/Tables/UsbStatus.g.cpp

    @brief USB request status codes, generated from usb.h.

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
    { 0x00000000u, 0, "USBD_STATUS_SUCCESS", nullptr },
    { 0x00000001u, 0, "USBD_STATUS_PORT_OPERATION_PENDING", nullptr },
    { 0x40000000u, 0, "USBD_STATUS_PENDING", nullptr },
    { 0x80000200u, 0, "USBD_STATUS_INVALID_URB_FUNCTION", nullptr },
    { 0x80000300u, 0, "USBD_STATUS_INVALID_PARAMETER", nullptr },
    { 0x80000400u, 0, "USBD_STATUS_ERROR_BUSY", nullptr },
    { 0x80000600u, 0, "USBD_STATUS_INVALID_PIPE_HANDLE", nullptr },
    { 0x80000700u, 0, "USBD_STATUS_NO_BANDWIDTH", nullptr },
    { 0x80000800u, 0, "USBD_STATUS_INTERNAL_HC_ERROR", nullptr },
    { 0x80000900u, 0, "USBD_STATUS_ERROR_SHORT_TRANSFER", nullptr },
    { 0xC0000001u, 0, "USBD_STATUS_CRC", nullptr },
    { 0xC0000002u, 0, "USBD_STATUS_BTSTUFF", nullptr },
    { 0xC0000003u, 0, "USBD_STATUS_DATA_TOGGLE_MISMATCH", nullptr },
    { 0xC0000004u, 0, "USBD_STATUS_STALL_PID", nullptr },
    { 0xC0000005u, 0, "USBD_STATUS_DEV_NOT_RESPONDING", nullptr },
    { 0xC0000006u, 0, "USBD_STATUS_PID_CHECK_FAILURE", nullptr },
    { 0xC0000007u, 0, "USBD_STATUS_UNEXPECTED_PID", nullptr },
    { 0xC0000008u, 0, "USBD_STATUS_DATA_OVERRUN", nullptr },
    { 0xC0000009u, 0, "USBD_STATUS_DATA_UNDERRUN", nullptr },
    { 0xC000000Au, 0, "USBD_STATUS_RESERVED1", nullptr },
    { 0xC000000Bu, 0, "USBD_STATUS_RESERVED2", nullptr },
    { 0xC000000Cu, 0, "USBD_STATUS_BUFFER_OVERRUN", nullptr },
    { 0xC000000Du, 0, "USBD_STATUS_BUFFER_UNDERRUN", nullptr },
    { 0xC000000Fu, 0, "USBD_STATUS_NOT_ACCESSED", nullptr },
    { 0xC0000010u, 0, "USBD_STATUS_FIFO", nullptr },
    { 0xC0000011u, 0, "USBD_STATUS_XACT_ERROR", nullptr },
    { 0xC0000012u, 0, "USBD_STATUS_BABBLE_DETECTED", nullptr },
    { 0xC0000013u, 0, "USBD_STATUS_DATA_BUFFER_ERROR", nullptr },
    { 0xC0000014u, 0, "USBD_STATUS_NO_PING_RESPONSE", nullptr },
    { 0xC0000015u, 0, "USBD_STATUS_INVALID_STREAM_TYPE", nullptr },
    { 0xC0000016u, 0, "USBD_STATUS_INVALID_STREAM_ID", nullptr },
    { 0xC0000030u, 0, "USBD_STATUS_ENDPOINT_HALTED", nullptr },
    { 0xC0000A00u, 0, "USBD_STATUS_BAD_START_FRAME", nullptr },
    { 0xC0000B00u, 0, "USBD_STATUS_ISOCH_REQUEST_FAILED", nullptr },
    { 0xC0000C00u, 0, "USBD_STATUS_FRAME_CONTROL_OWNED", nullptr },
    { 0xC0000D00u, 0, "USBD_STATUS_FRAME_CONTROL_NOT_OWNED", nullptr },
    { 0xC0000E00u, 0, "USBD_STATUS_NOT_SUPPORTED", nullptr },
    { 0xC0000F00u, 0, "USBD_STATUS_INAVLID_CONFIGURATION_DESCRIPTOR", nullptr },
    { 0xC0000F00u, k_TableEntryAlias, "USBD_STATUS_INVALID_CONFIGURATION_DESCRIPTOR", nullptr },
    { 0xC0001000u, 0, "USBD_STATUS_INSUFFICIENT_RESOURCES", nullptr },
    { 0xC0002000u, 0, "USBD_STATUS_SET_CONFIG_FAILED", nullptr },
    { 0xC0003000u, 0, "USBD_STATUS_BUFFER_TOO_SMALL", nullptr },
    { 0xC0004000u, 0, "USBD_STATUS_INTERFACE_NOT_FOUND", nullptr },
    { 0xC0005000u, 0, "USBD_STATUS_INAVLID_PIPE_FLAGS", nullptr },
    { 0xC0006000u, 0, "USBD_STATUS_TIMEOUT", nullptr },
    { 0xC0007000u, 0, "USBD_STATUS_DEVICE_GONE", nullptr },
    { 0xC0008000u, 0, "USBD_STATUS_STATUS_NOT_MAPPED", nullptr },
    { 0xC0009000u, 0, "USBD_STATUS_HUB_INTERNAL_ERROR", nullptr },
    { 0xC0010000u, 0, "USBD_STATUS_CANCELED", nullptr },
    { 0xC0020000u, 0, "USBD_STATUS_ISO_NOT_ACCESSED_BY_HW", nullptr },
    { 0xC0030000u, 0, "USBD_STATUS_ISO_TD_ERROR", nullptr },
    { 0xC0040000u, 0, "USBD_STATUS_ISO_NA_LATE_USBPORT", nullptr },
    { 0xC0050000u, 0, "USBD_STATUS_ISO_NOT_ACCESSED_LATE", nullptr },
    { 0xC0100000u, 0, "USBD_STATUS_BAD_DESCRIPTOR", nullptr },
    { 0xC0100001u, 0, "USBD_STATUS_BAD_DESCRIPTOR_BLEN", nullptr },
    { 0xC0100002u, 0, "USBD_STATUS_BAD_DESCRIPTOR_TYPE", nullptr },
    { 0xC0100003u, 0, "USBD_STATUS_BAD_INTERFACE_DESCRIPTOR", nullptr },
    { 0xC0100004u, 0, "USBD_STATUS_BAD_ENDPOINT_DESCRIPTOR", nullptr },
    { 0xC0100005u, 0, "USBD_STATUS_BAD_INTERFACE_ASSOC_DESCRIPTOR", nullptr },
    { 0xC0100006u, 0, "USBD_STATUS_BAD_CONFIG_DESC_LENGTH", nullptr },
    { 0xC0100007u, 0, "USBD_STATUS_BAD_NUMBER_OF_INTERFACES", nullptr },
    { 0xC0100008u, 0, "USBD_STATUS_BAD_NUMBER_OF_ENDPOINTS", nullptr },
    { 0xC0100009u, 0, "USBD_STATUS_BAD_ENDPOINT_ADDRESS", nullptr },
};

const TableSpan g_UsbStatusTable =
{
    g_Entries,
    ARRAYSIZE(g_Entries),
    L"usb.h"
};
