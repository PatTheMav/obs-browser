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

#include "QCefManagerInterface.hpp"

#include <filesystem>

struct QCefManager : QCefManagerInterface {
    ~QCefManager() {}
    
    virtual bool initialize() override;
    virtual bool isInitialized() const override;
    virtual bool waitForInitialization() const override;
    
    //    static QCefWidget *createWidget(QWidget *parent, const std::string &url, QCefCookieManager *cookieManager = nullptr);
    //    static QCefCookieManager *createCookieManager(const std::filesystem::path &storagePath);
    virtual std::filesystem::path getCookieLocation(const std::string &pathSuffix) override;
    
    virtual void allowPopupsForUrl(const std::string &url, QObject *obj) override;
    virtual void forcePopupsForUrl(const std::string &url, QObject *obj) override;
    
    // MARK: - Deprecated old interface methods
    OBS_DEPRECATED virtual bool init_browser() override;
    OBS_DEPRECATED virtual bool initialized(void) override;
    OBS_DEPRECATED virtual bool wait_for_browser_init(void) override;
    
    OBS_DEPRECATED  virtual QCefWidget *create_widget(QWidget *parent, const std::string &url,
                                                      QCefCookieManager *cookie_manager = nullptr) override;
    
    OBS_DEPRECATED  virtual QCefCookieManager *create_cookie_manager(const std::string &storage_path,
                                                                     bool persist_session_cookies = false) override;
    
    OBS_DEPRECATED virtual BPtr<char> get_cookie_path(const std::string &storage_path) override;
    
    OBS_DEPRECATED virtual void add_popup_whitelist_url(const std::string &url, QObject *obj) override;
    OBS_DEPRECATED virtual void add_force_popup_url(const std::string &url, QObject *obj) override;

};
