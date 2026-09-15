/*!

    @file Core/Tables/HResultsAudio.g.cpp

    @brief Audio client HRESULTs, generated from audioclient.h.

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
    { 0x08890001u, 0, "AUDCLNT_S_BUFFER_EMPTY", nullptr },
    { 0x08890002u, 0, "AUDCLNT_S_THREAD_ALREADY_REGISTERED", nullptr },
    { 0x08890003u, 0, "AUDCLNT_S_POSITION_STALLED", nullptr },
    { 0x88890001u, 0, "AUDCLNT_E_NOT_INITIALIZED", nullptr },
    { 0x88890002u, 0, "AUDCLNT_E_ALREADY_INITIALIZED", nullptr },
    { 0x88890003u, 0, "AUDCLNT_E_WRONG_ENDPOINT_TYPE", nullptr },
    { 0x88890004u, 0, "AUDCLNT_E_DEVICE_INVALIDATED", nullptr },
    { 0x88890005u, 0, "AUDCLNT_E_NOT_STOPPED", nullptr },
    { 0x88890006u, 0, "AUDCLNT_E_BUFFER_TOO_LARGE", nullptr },
    { 0x88890007u, 0, "AUDCLNT_E_OUT_OF_ORDER", nullptr },
    { 0x88890008u, 0, "AUDCLNT_E_UNSUPPORTED_FORMAT", nullptr },
    { 0x88890009u, 0, "AUDCLNT_E_INVALID_SIZE", nullptr },
    { 0x8889000Au, 0, "AUDCLNT_E_DEVICE_IN_USE", nullptr },
    { 0x8889000Bu, 0, "AUDCLNT_E_BUFFER_OPERATION_PENDING", nullptr },
    { 0x8889000Cu, 0, "AUDCLNT_E_THREAD_NOT_REGISTERED", nullptr },
    { 0x8889000Eu, 0, "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED", nullptr },
    { 0x8889000Fu, 0, "AUDCLNT_E_ENDPOINT_CREATE_FAILED", nullptr },
    { 0x88890010u, 0, "AUDCLNT_E_SERVICE_NOT_RUNNING", nullptr },
    { 0x88890011u, 0, "AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED", nullptr },
    { 0x88890012u, 0, "AUDCLNT_E_EXCLUSIVE_MODE_ONLY", nullptr },
    { 0x88890013u, 0, "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL", nullptr },
    { 0x88890014u, 0, "AUDCLNT_E_EVENTHANDLE_NOT_SET", nullptr },
    { 0x88890015u, 0, "AUDCLNT_E_INCORRECT_BUFFER_SIZE", nullptr },
    { 0x88890016u, 0, "AUDCLNT_E_BUFFER_SIZE_ERROR", nullptr },
    { 0x88890017u, 0, "AUDCLNT_E_CPUUSAGE_EXCEEDED", nullptr },
    { 0x88890018u, 0, "AUDCLNT_E_BUFFER_ERROR", nullptr },
    { 0x88890019u, 0, "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED", nullptr },
    { 0x88890020u, 0, "AUDCLNT_E_INVALID_DEVICE_PERIOD", nullptr },
    { 0x88890021u, 0, "AUDCLNT_E_INVALID_STREAM_FLAG", nullptr },
    { 0x88890022u, 0, "AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE", nullptr },
    { 0x88890023u, 0, "AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES", nullptr },
    { 0x88890024u, 0, "AUDCLNT_E_OFFLOAD_MODE_ONLY", nullptr },
    { 0x88890025u, 0, "AUDCLNT_E_NONOFFLOAD_MODE_ONLY", nullptr },
    { 0x88890026u, 0, "AUDCLNT_E_RESOURCES_INVALIDATED", nullptr },
    { 0x88890027u, 0, "AUDCLNT_E_RAW_MODE_UNSUPPORTED", nullptr },
    { 0x88890028u, 0, "AUDCLNT_E_ENGINE_PERIODICITY_LOCKED", nullptr },
    { 0x88890029u, 0, "AUDCLNT_E_ENGINE_FORMAT_LOCKED", nullptr },
    { 0x88890030u, 0, "AUDCLNT_E_HEADTRACKING_ENABLED", nullptr },
    { 0x88890040u, 0, "AUDCLNT_E_HEADTRACKING_UNSUPPORTED", nullptr },
    { 0x88890041u, 0, "AUDCLNT_E_EFFECT_NOT_AVAILABLE", nullptr },
    { 0x88890042u, 0, "AUDCLNT_E_EFFECT_STATE_READ_ONLY", nullptr },
    { 0x88890043u, 0, "AUDCLNT_E_POST_VOLUME_LOOPBACK_UNSUPPORTED", nullptr },
};

const TableSpan g_HResultAudioTable =
{
    g_Entries,
    ARRAYSIZE(g_Entries),
    L"audioclient.h"
};
