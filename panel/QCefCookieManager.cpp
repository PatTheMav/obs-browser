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

#include "QCefCookieManager.hpp"
#include "CookieVisitor.hpp"

#include <obs-module.h>
#include <util/threading.h>

#include <include/cef_request_context.h>
#include <include/cef_request_context_handler.h>

#include <filesystem>

extern os_event_t *cef_started_event;

QCefCookieManagerImpl::QCefCookieManagerImpl(const std::filesystem::path &storagePath)
{
	if (os_event_try(cef_started_event) != 0) {
		throw "CEF not initialized";
	}

	setStoragePath(storagePath);
}

bool QCefCookieManagerImpl::deleteCookies(const std::string &url, const std::string &name)
{
	if (!cookieManager) {
		return false;
	}

	bool result = cookieManager->DeleteCookies(url, name, nullptr);

	return result;
}

void QCefCookieManagerImpl::setStoragePath(const std::filesystem::path &storagePath)
{
	if (!std::filesystem::exists(storagePath)) {
		try {
			std::filesystem::create_directory(storagePath);
		} catch (const std::filesystem::filesystem_error &error) {
			std::string exceptionMessage{"Unable to create cookie storage location: "};
			exceptionMessage.append(storagePath.u8string());

			throw exceptionMessage;
		}
	}

	CefRequestContextSettings settings;

#if CHROME_VERSION_BUILD <= 6533
	settings.persist_user_preferences = 1;
#endif
	CefString(&settings.cache_path).FromString(storagePath.u8string());

	CefRefPtr<CefRequestContext> context =
		CefRequestContext::CreateContext(settings, CefRefPtr<CefRequestContextHandler>());

	if (!context) {
		throw "Unable to create CefRequestContext";
	}

	requestContext = context;
	cookieManager = context->GetCookieManager(nullptr);
}

bool QCefCookieManagerImpl::flushStore()
{
	if (!cookieManager) {
		return false;
	}

	return cookieManager->FlushStore(nullptr);
}

void QCefCookieManagerImpl::checkForCookie(const std::string &url, const std::string &name,
					   std::function<void(bool)> callback)
{
	if (!cookieManager) {
		return;
	}

	auto cookieVisitor = std::make_unique<CookieVisitor>(name, callback);

	cookieManager->VisitUrlCookies(url, false, CefRefPtr<CookieVisitor>(cookieVisitor.get()));
}

// MARK: - Deprecated old interface methods

bool QCefCookieManagerImpl::DeleteCookies(const std::string &url, const std::string &name)
{
	return deleteCookies(url, name);
}

bool QCefCookieManagerImpl::SetStoragePath(const std::string &storage_path, bool)
{
	std::filesystem::path storagePath = std::filesystem::u8path(storage_path);

	setStoragePath(storagePath);

	return true;
}

bool QCefCookieManagerImpl::FlushStore()
{
	return flushStore();
}

void QCefCookieManagerImpl::CheckForCookie(const std::string &site, const std::string &cookie,
					   cookie_exists_cb callback)
{
	checkForCookie(site, cookie, callback);
}
