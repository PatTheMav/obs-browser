#pragma once

#include <QTimer>
#include <QPointer>
#include "browser-panel.hpp"
#include "cef-headers.hpp"

#include <vector>
#include <mutex>

struct PopupWhitelistInfo {
	std::string url;
	QPointer<QObject> obj;

	inline PopupWhitelistInfo(const std::string &url_, QObject *obj_) : url(url_), obj(obj_) {}
};

extern std::mutex popup_whitelist_mutex;
extern std::vector<PopupWhitelistInfo> popup_whitelist;
extern std::vector<PopupWhitelistInfo> forced_popups;

/* ------------------------------------------------------------------------- */

enum class QCefTaskResult {
    Invalid,
    Failure,
    Success
};

class QCefWidgetInternal : public QCefWidget {
	Q_OBJECT
    
private:
    CefRefPtr<CefBrowser> createBrowser(WId handle, QSize size);
    
    virtual bool eventFilter(QObject *object, QEvent *event) override;

    bool allowAllPopups_ = false;

    CefRefPtr<CefBrowser> cefBrowser_;
    std::string url_;
    std::string script_;
    CefRefPtr<CefRequestContext> requestContext_;
#ifndef __APPLE__
    QPointer<QWindow> window_;
    QPointer<QWidget> container_;
#endif
    
public:
	QCefWidgetInternal(QWidget *parent, const std::string &url, CefRefPtr<CefRequestContext> requestContext);
	~QCefWidgetInternal();

	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void showEvent(QShowEvent *event) override;
	virtual QPaintEngine *paintEngine() const override;

	virtual void setURL(const std::string &url) override;
	virtual void setStartupScript(const std::string &script) override;
	virtual void allowAllPopups(bool allow) override;
	virtual void closeBrowser() override;
	virtual void reloadPage() override;
	virtual bool zoomPage(int direction) override;
	virtual void executeJavaScript(const std::string &script) override;
    
    QCefTaskResult tryCreateBrowser();
	void finishCloseBrowser();
	void tryResize();

    void handleTitleChange(CefRefPtr<CefBrowser> browser, const std::string &title);
    
    std::string getScript() const;
#ifdef __linux__
private:
	bool needsDeleteXdndProxy = true;
	void unsetToplevelXdndProxy();
#endif

signals:
	void readyToClose();
};
