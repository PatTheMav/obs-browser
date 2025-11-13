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

#include <filesystem>
#include <util/c99defs.h>
#include <util/util.hpp>

class QCefWidget;
class QWidget;
class QObject;
struct QCefCookieManagerInterface;
struct QCefCookieManager;

struct QCefManagerInterface {
    virtual ~QCefManagerInterface() {}

    virtual bool initialize() = 0;
    virtual bool isInitialized() const = 0;
    virtual bool waitForInitialization() const = 0;

    static QCefWidget *createWidget(QWidget *parent, const std::string &url, QCefCookieManagerInterface *cookieManager = nullptr);
    static std::unique_ptr<QCefCookieManagerInterface> createCookieManager(const std::filesystem::path &storagePath);

    virtual std::filesystem::path getCookieLocation(const std::string &pathSuffix) = 0;
    
    virtual void allowPopupsForUrl(const std::string &url, QObject *obj) = 0;
    virtual void forcePopupsForUrl(const std::string &url, QObject *obj) = 0;
    
    // MARK: - Deprecated old interface methods
    OBS_DEPRECATED virtual bool init_browser() = 0;
    OBS_DEPRECATED virtual bool initialized() = 0;
    OBS_DEPRECATED virtual bool wait_for_browser_init() = 0;
    
    OBS_DEPRECATED virtual QCefWidget *create_widget(QWidget *parent, const std::string &url,
                                      QCefCookieManager *cookie_manager = nullptr) = 0;
    
    OBS_DEPRECATED  virtual QCefCookieManager *create_cookie_manager(const std::string &storage_path,
                                                     bool persist_session_cookies = false) = 0;
    
    OBS_DEPRECATED virtual BPtr<char> get_cookie_path(const std::string &storage_path) = 0;
    
    OBS_DEPRECATED virtual void add_popup_whitelist_url(const std::string &url, QObject *obj) = 0;
    OBS_DEPRECATED virtual void add_force_popup_url(const std::string &url, QObject *obj) = 0;
};

struct OBS_DEPRECATED QCef : QCefManagerInterface {};
