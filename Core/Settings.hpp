/*!

    @file Core/Settings.hpp

    @brief WinCode's settings file, settings.json, which every front end reads.

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

#pragma once

#include "WinCode.hpp"
#include "Json.hpp"


void
SettingsLoad (
    void
    );

bool
SettingsSave (
    void
    );

std::wstring
SettingsPath (
    void
    );

bool
SettingsIsUnreadable (
    void
    );

JsonValue&
SettingsRoot (
    void
    );

//
// Typed access by dotted path, as in "window.width". A missing value, or one
// of the wrong type, reads as the default.
//
int SettingsGetInt(_In_ PCWSTR Path, _In_ int Default);
void SettingsSetInt(_In_ PCWSTR Path, _In_ int Value);
bool SettingsGetBool(_In_ PCWSTR Path, _In_ bool Default);
void SettingsSetBool(_In_ PCWSTR Path, _In_ bool Value);
std::wstring SettingsGetString(_In_ PCWSTR Path, _In_ PCWSTR Default);
void SettingsSetString(_In_ PCWSTR Path, _In_ PCWSTR Value);
