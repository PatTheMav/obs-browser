#pragma once

#include "QCefManagerInterface.hpp"
#include "QCefCookieManagerInterface.hpp"

#include <obs-module.h>
#ifdef ENABLE_WAYLAND
#include <obs-nix-platform.h>
#endif
#include <util/platform.h>
#include <util/util.hpp>

#include <QWidget>

#include <functional>
#include <string>

#if defined(__APPLE__)
#include <dlfcn.h>
#endif

class QCefWidget : public QWidget {
	Q_OBJECT

protected:
	inline QCefWidget(QWidget *parent) : QWidget(parent) {}
private:
    virtual bool eventFilter(QObject *object, QEvent *event) = 0;

public:
	virtual void setURL(const std::string &url) = 0;
	virtual void setStartupScript(const std::string &script) = 0;
	virtual void allowAllPopups(bool allow) = 0;
	virtual void closeBrowser() = 0;
	virtual void reloadPage() = 0;
	virtual bool zoomPage(int direction) = 0;
	virtual void executeJavaScript(const std::string &script) = 0;
    
signals:
	void titleChanged(const QString &title);
	void urlChanged(const QString &url);
};

/* ------------------------------------------------------------------------- */

static inline void *get_browser_lib()
{
	// Disable panels on Wayland for now
	bool isWayland = false;
#ifdef ENABLE_WAYLAND
	isWayland = obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND;
#endif
    if (isWayland) {
        return nullptr;
    }

	obs_module_t *browserModule = obs_get_module("obs-browser");

    if (!browserModule) {
        return nullptr;
    }

	return obs_get_module_lib(browserModule);
}

PRAGMA_WARN_PUSH
PRAGMA_WARN_DEPRECATION
static inline QCef *obs_browser_init_panel(void)
{
	void *lib = get_browser_lib();
    QCef *(*create_qcef)(void) = nullptr;

    if (!lib) {
        return nullptr;
    }

	create_qcef = (decltype(create_qcef))os_dlsym(lib, "obs_browser_create_qcef");

    if (!create_qcef) {
        return nullptr;
    }

	return create_qcef();
}
PRAGMA_WARN_POP

static inline int obs_browser_qcef_version(void)
{
	void *lib = get_browser_lib();
	int (*qcef_version)(void) = nullptr;

    if (!lib) {
        return 0;
    }

	qcef_version = (decltype(qcef_version))os_dlsym(lib, "obs_browser_qcef_version_export");

    if (!qcef_version) {
        return 0;
    }

	return qcef_version();
}
