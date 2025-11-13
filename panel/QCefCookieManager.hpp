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

#include "QCefCookieManagerInterface.hpp"

#include <include/cef_base.h>

#include <filesystem>

class CefCookieManager;
class CefRequestContext;

struct QCefCookieManagerImpl : QCefCookieManagerInterface {
    CefRefPtr<CefCookieManager> cookieManager = nullptr;
    CefRefPtr<CefRequestContext> requestContext = nullptr;
    
    QCefCookieManagerImpl(const std::filesystem::path &storagePath);
    
    virtual bool deleteCookies(const std::string &url, const std::string &name) override;
    virtual void setStoragePath(const std::filesystem::path &storagePath) override;
    virtual bool flushStore() override;
    
    virtual void checkForCookie(const std::string &url, const std::string &name, std::function<void(bool)> callback) override;
    
    // MARK: - Deprecated old interface methods
    OBS_DEPRECATED virtual bool DeleteCookies(const std::string &url, const std::string &name) override;
    OBS_DEPRECATED virtual bool SetStoragePath(const std::string &storage_path, bool persist_session_cookies = false) override;
    OBS_DEPRECATED virtual bool FlushStore() override;
    
    OBS_DEPRECATED typedef std::function<void(bool)> cookie_exists_cb;
    
    OBS_DEPRECATED virtual void CheckForCookie(const std::string &site, const std::string &cookie, cookie_exists_cb callback) override;
};
