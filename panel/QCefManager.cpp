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

#include "QCefManager.hpp"
#include "QCefCookieManager.hpp"
#include "browser-panel-internal.hpp"

#include <obs-module.h>
#include <util/threading.h>

extern std::mutex popup_whitelist_mutex;
extern std::vector<PopupWhitelistInfo> popup_whitelist;
extern std::vector<PopupWhitelistInfo> forced_popups;

extern os_event_t *cef_started_event;
extern "C" void obs_browser_initialize(void);

static constexpr int BROWSER_PANEL_VERSION = 3;

QCefWidget *QCefManagerInterface::createWidget(QWidget *parent, const std::string &url,
					       QCefCookieManagerInterface *cookieManager)
{
	CefRefPtr<CefRequestContext> requestContext = nullptr;

	if (cookieManager) {
		requestContext = static_cast<QCefCookieManagerImpl *>(cookieManager)->requestContext;
	}

	auto widget = std::make_unique<QCefWidgetInternal>(parent, url, requestContext);

	return widget.release();
}

std::unique_ptr<QCefCookieManagerInterface>
QCefManagerInterface::createCookieManager(const std::filesystem::path &storagePath)
{
	try {
		auto cookieManager = std::make_unique<QCefCookieManagerImpl>(storagePath);

		return cookieManager;
	} catch (const char *exceptionMessage) {
		blog(LOG_ERROR, "Failed to create cookie manager: %s", exceptionMessage);
	}

	return nullptr;
}

bool QCefManager::initialize()
{
	if (os_event_try(cef_started_event) == 0) {
		return true;
	}

	obs_browser_initialize();

	return false;
}

bool QCefManager::isInitialized() const
{
	return os_event_try(cef_started_event) == 0;
}

bool QCefManager::waitForInitialization() const
{
	return os_event_wait(cef_started_event) == 0;
}

std::filesystem::path QCefManager::getCookieLocation(const std::string &pathSuffix)
{
	const char *configPathString = obs_module_config_path(pathSuffix.c_str());

	if (!configPathString) {
		blog(LOG_ERROR, "Failed to get config path string with path suffix '%s'", pathSuffix.c_str());
	}

	std::filesystem::path absoluteStoragePath = std::filesystem::u8path(configPathString);

	return absoluteStoragePath;
}

void QCefManager::allowPopupsForUrl(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock{popup_whitelist_mutex};

	popup_whitelist.emplace_back(url, obj);
}

void QCefManager::forcePopupsForUrl(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock{popup_whitelist_mutex};

	forced_popups.emplace_back(url, obj);
}

// MARK: - Deprecated old interface methods

bool QCefManager::init_browser()
{
	return initialize();
}

bool QCefManager::wait_for_browser_init()
{
	return waitForInitialization();
}

bool QCefManager::initialized()
{
	return isInitialized();
}

QCefWidget *QCefManager::create_widget(QWidget *parent, const std::string &url, QCefCookieManager *cookie_manager)
{
	return QCefManagerInterface::createWidget(parent, url, cookie_manager);
}

QCefCookieManager *QCefManager::create_cookie_manager(const std::string &storage_path, bool)
{
	const char *configPathString = obs_module_config_path(storage_path.c_str());

	if (!configPathString) {
		blog(LOG_ERROR, "Invalid cookie storage path returned from OBS module API");
		return nullptr;
	}

	std::filesystem::path storagePath = std::filesystem::u8path(configPathString);
	std::filesystem::path absoluteStoragePath = std::filesystem::absolute(storagePath);

	auto cookieManager = QCefManagerInterface::createCookieManager(absoluteStoragePath);

	return static_cast<QCefCookieManager *>(cookieManager.release());
}

BPtr<char> QCefManager::get_cookie_path(const std::string &storage_path)
{
	std::filesystem::path cookiePath = getCookieLocation(storage_path);
	std::string cookiePathString = cookiePath.u8string();

	auto cookiePathCString = std::make_unique<char>(cookiePathString.size() + 1);

	std::strncpy(cookiePathCString.get(), cookiePathString.c_str(), cookiePathString.size() + 1);

	BPtr<char> obsCookieString = cookiePathCString.release();

	return obsCookieString;
}

void QCefManager::add_popup_whitelist_url(const std::string &url, QObject *obj)
{
	allowPopupsForUrl(url, obj);
}

void QCefManager::add_force_popup_url(const std::string &url, QObject *obj)
{
	forcePopupsForUrl(url, obj);
}

extern "C" EXPORT QCefManagerInterface *obs_browser_create_qcef(void)
{
	auto qcefInstance = std::make_unique<QCefManager>();

	return static_cast<QCefManagerInterface *>(qcefInstance.release());
}

extern "C" EXPORT int obs_browser_qcef_version_export(void)
{
	return BROWSER_PANEL_VERSION;
}
