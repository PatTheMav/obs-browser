/******************************************************************************
 Copyright (C) 2025 by Patrick Heyer <PatTheMav@users.noreply.github.com>
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#pragma once

#include <util/c99defs.h>

#include <filesystem>
#include <string>

struct QCefCookieManagerInterface {
    virtual ~QCefCookieManagerInterface() {}
    
    virtual bool deleteCookies(const std::string &url, const std::string &name) = 0;
    virtual void setStoragePath(const std::filesystem::path &storagePath) = 0;
    virtual bool flushStore() = 0;
    
    virtual void checkForCookie(const std::string &url, const std::string &name, std::function<void(bool)> callback) = 0;
    
    // MARK: - Deprecated old interface methods
    OBS_DEPRECATED virtual bool DeleteCookies(const std::string &url, const std::string &name) = 0;
    OBS_DEPRECATED virtual bool SetStoragePath(const std::string &storage_path, bool persist_session_cookies = false) = 0;
    OBS_DEPRECATED virtual bool FlushStore() = 0;
    
    OBS_DEPRECATED typedef std::function<void(bool)> cookie_exists_cb;
    
    OBS_DEPRECATED virtual void CheckForCookie(const std::string &site, const std::string &cookie, cookie_exists_cb callback) = 0;
};

struct OBS_DEPRECATED QCefCookieManager : QCefCookieManagerInterface {};
