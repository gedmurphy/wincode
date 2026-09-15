/*!

    @file Core/Tables/DeviceProblems.g.cpp

    @brief Device Manager problem codes, generated from cfg.h.

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
    { 0x00000001u, 0, "CM_PROB_NOT_CONFIGURED", "No config for device." },
    { 0x00000002u, 0, "CM_PROB_DEVLOADER_FAILED", "Service load failed." },
    { 0x00000003u, 0, "CM_PROB_OUT_OF_MEMORY", "Out of memory." },
    { 0x00000004u, 0, "CM_PROB_ENTRY_IS_WRONG_TYPE", nullptr },
    { 0x00000005u, 0, "CM_PROB_LACKED_ARBITRATOR", nullptr },
    { 0x00000006u, 0, "CM_PROB_BOOT_CONFIG_CONFLICT", "Boot config conflict." },
    { 0x00000007u, 0, "CM_PROB_FAILED_FILTER", nullptr },
    { 0x00000008u, 0, "CM_PROB_DEVLOADER_NOT_FOUND", "Devloader not found." },
    { 0x00000009u, 0, "CM_PROB_INVALID_DATA", "Invalid ID." },
    { 0x0000000Au, 0, "CM_PROB_FAILED_START", nullptr },
    { 0x0000000Bu, 0, "CM_PROB_LIAR", nullptr },
    { 0x0000000Cu, 0, "CM_PROB_NORMAL_CONFLICT", "Config conflict." },
    { 0x0000000Du, 0, "CM_PROB_NOT_VERIFIED", nullptr },
    { 0x0000000Eu, 0, "CM_PROB_NEED_RESTART", "Requires restart." },
    { 0x0000000Fu, 0, "CM_PROB_REENUMERATION", nullptr },
    { 0x00000010u, 0, "CM_PROB_PARTIAL_LOG_CONF", nullptr },
    { 0x00000011u, 0, "CM_PROB_UNKNOWN_RESOURCE", "Unknown res type." },
    { 0x00000012u, 0, "CM_PROB_REINSTALL", nullptr },
    { 0x00000013u, 0, "CM_PROB_REGISTRY", nullptr },
    { 0x00000014u, 0, "CM_PROB_VXDLDR", "WINDOWS 95 ONLY." },
    { 0x00000015u, 0, "CM_PROB_WILL_BE_REMOVED", "Devinst will remove." },
    { 0x00000016u, 0, "CM_PROB_DISABLED", "Devinst is disabled." },
    { 0x00000017u, 0, "CM_PROB_DEVLOADER_NOT_READY", "Devloader not ready." },
    { 0x00000018u, 0, "CM_PROB_DEVICE_NOT_THERE", "Device doesn't exist." },
    { 0x00000019u, 0, "CM_PROB_MOVED", nullptr },
    { 0x0000001Au, 0, "CM_PROB_TOO_EARLY", nullptr },
    { 0x0000001Bu, 0, "CM_PROB_NO_VALID_LOG_CONF", "No valid log config." },
    { 0x0000001Cu, 0, "CM_PROB_FAILED_INSTALL", "Install failed." },
    { 0x0000001Du, 0, "CM_PROB_HARDWARE_DISABLED", "Device disabled." },
    { 0x0000001Eu, 0, "CM_PROB_CANT_SHARE_IRQ", "Can't share IRQ." },
    { 0x0000001Fu, 0, "CM_PROB_FAILED_ADD", "Driver failed add." },
    { 0x00000020u, 0, "CM_PROB_DISABLED_SERVICE", "Service's Start = 4." },
    { 0x00000021u, 0, "CM_PROB_TRANSLATION_FAILED", "Resource translation failed." },
    { 0x00000022u, 0, "CM_PROB_NO_SOFTCONFIG", "No soft config." },
    { 0x00000023u, 0, "CM_PROB_BIOS_TABLE", "Device missing in BIOS table." },
    { 0x00000024u, 0, "CM_PROB_IRQ_TRANSLATION_FAILED", "IRQ translator failed." },
    { 0x00000025u, 0, "CM_PROB_FAILED_DRIVER_ENTRY", "DriverEntry() failed." },
    { 0x00000026u, 0, "CM_PROB_DRIVER_FAILED_PRIOR_UNLOAD", "Driver should have unloaded." },
    { 0x00000027u, 0, "CM_PROB_DRIVER_FAILED_LOAD", "Driver load unsuccessful." },
    { 0x00000028u, 0, "CM_PROB_DRIVER_SERVICE_KEY_INVALID", "Error accessing driver's service key." },
    { 0x00000029u, 0, "CM_PROB_LEGACY_SERVICE_NO_DEVICES", "Loaded legacy service created no devices." },
    { 0x0000002Au, 0, "CM_PROB_DUPLICATE_DEVICE", "Two devices were discovered with the same name." },
    { 0x0000002Bu, 0, "CM_PROB_FAILED_POST_START", "The drivers set the device state to failed." },
    { 0x0000002Cu, 0, "CM_PROB_HALTED", "This device was failed post start via usermode." },
    { 0x0000002Du, 0, "CM_PROB_PHANTOM", "The devinst currently exists only in the registry." },
    { 0x0000002Eu, 0, "CM_PROB_SYSTEM_SHUTDOWN", "The system is shutting down." },
    { 0x0000002Fu, 0, "CM_PROB_HELD_FOR_EJECT", "The device is offline awaiting removal." },
    { 0x00000030u, 0, "CM_PROB_DRIVER_BLOCKED", "One or more drivers is blocked from loading." },
    { 0x00000031u, 0, "CM_PROB_REGISTRY_TOO_LARGE", "System hive has grown too large." },
    { 0x00000032u, 0, "CM_PROB_SETPROPERTIES_FAILED", "Failed to apply one or more registry properties." },
    { 0x00000033u, 0, "CM_PROB_WAITING_ON_DEPENDENCY", "Device is stalled waiting on a dependency to start." },
    { 0x00000034u, 0, "CM_PROB_UNSIGNED_DRIVER", "Failed load driver due to unsigned image." },
    { 0x00000035u, 0, "CM_PROB_USED_BY_DEBUGGER", "Device is being used by kernel debugger." },
    { 0x00000036u, 0, "CM_PROB_DEVICE_RESET", "Device is being reset." },
    { 0x00000037u, 0, "CM_PROB_CONSOLE_LOCKED", "Device is blocked while console is locked." },
    { 0x00000038u, 0, "CM_PROB_NEED_CLASS_CONFIG", "Device needs extended class configuration to start." },
    { 0x00000039u, 0, "CM_PROB_GUEST_ASSIGNMENT_FAILED", "Assignment to guest partition failed." },
};

const TableSpan g_DeviceProblemTable =
{
    g_Entries,
    ARRAYSIZE(g_Entries),
    L"cfg.h"
};
