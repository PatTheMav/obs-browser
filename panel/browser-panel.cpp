#include "browser-panel-internal.hpp"
#include "browser-panel-client.hpp"
#include "cef-headers.hpp"
#include "browser-app.hpp"

#include <QWindow>
#include <QApplication>

#ifdef ENABLE_BROWSER_QT_LOOP
#include <QEventLoop>
#include <QThread>
#endif

#ifdef __APPLE__
#include <objc/objc.h>
#endif

#include <obs-module.h>
#include <util/threading.h>
#include <util/base.h>
#include <thread>
#include <cmath>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#endif

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);
extern os_event_t *cef_started_event;

std::mutex popup_whitelist_mutex;
std::vector<PopupWhitelistInfo> popup_whitelist;
std::vector<PopupWhitelistInfo> forced_popups;

static int zoomLevels[] = {25, 33, 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400};

namespace {
void detachBrowserWindow(CefRefPtr<CefBrowserHost> host)
{
#ifdef _WIN32
	HWND hwnd = static_cast<HWND>(host->GetWindowHandle());
	if (hwnd) {
		ShowWindow(hwnd, SW_HIDE);
		SetParent(hwnd, nullptr);
	}
#elif __APPLE__
	SEL retain = sel_getUid("retain");
	SEL release = sel_getUid("release");
	SEL removeFromSuperview = sel_getUid("removeFromSuperview");
	void *(*msgSend)(id, SEL) = (void *(*)(id, SEL))objc_msgSend;

	id view = static_cast<id>(host->GetWindowHandle());
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	if (view && view->isa) {
		msgSend(view, retain);
		msgSend(view, removeFromSuperview);
		msgSend(view, release);
	}
#pragma clang diagnostic pop
#else
	UNUSED_PARAMETER(host);
#endif
}
} // namespace

QCefWidgetInternal::QCefWidgetInternal(QWidget *parent, const std::string &url,
				       CefRefPtr<CefRequestContext> requestContext)
	: QCefWidget(parent),
	  url_(url),
	  requestContext_(requestContext)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setFocusPolicy(Qt::ClickFocus);

#ifndef __APPLE__
	window_ = new QWindow();
	window_->setFlags(Qt::FramelessWindowHint);
	window_->setObjectName("QCefWidgetInternalWindow");
	window_->installEventFilter(this);
#endif
}

QCefWidgetInternal::~QCefWidgetInternal()
{
	closeBrowser();
}

bool QCefWidgetInternal::eventFilter(QObject *object, QEvent *event)
{
#ifdef __APPLE__
	UNUSED_PARAMETER(object);
	UNUSED_PARAMETER(event);

	return true;
#else
    // Return early event does not target the window wrapper or no browser instance is present
    if (object != window_ || !cefBrowser) {
        return true;
    }

    // Also return early if the event is not a "FocusIn" event, only necessary to check if the wrapper is targeted
    if (event->type() != QEvent::FocusIn) {
        return true;
    }

	CefRefPtr<CefBrowserHost> host{cefBrowser_->GetHost()};

	if (host) {
		host->SetFocus(true);
	}

	return true;
#endif
}

void QCefWidgetInternal::closeBrowser()
{
	if (!cefBrowser_) {
		return;
	}

	CefRefPtr<CefBrowserHost> host{cefBrowser_->GetHost()};

	if (!host) {
		return;
	}

	QEventLoop browserCloseLoop;

	// Ensure that the native window used by CEF is not attached to the widget view hierarchy while the browser
	// is closed.
	//
	// If the host window is not considered "destroyed" by the time CEF destroys the web contents of the associated
	// browser object, it will close the host window itself. The "host" window in this case would be OBS Studio's
	// main window however. So to ensure this cannot happen, the native window needs to be detached from the Qt
	// view hierarchy so there is no associated host window to close.
	auto preCloseBrowser = [&host]() {
		detachBrowserWindow(host);
	};

	auto closeBrowser = [&host]() {
		host->CloseBrowser(true);
	};

	connect(this, &QCefWidgetInternal::readyToClose, &browserCloseLoop, &QEventLoop::quit);

	QTimer::singleShot(0, &browserCloseLoop, preCloseBrowser);
	QTimer::singleShot(0, &browserCloseLoop, closeBrowser);
	QTimer::singleShot(1000, &browserCloseLoop, &QEventLoop::quit);

	browserCloseLoop.exec();

	CefRefPtr<CefClient> client{host->GetClient()};

	if (client) {
		QCefBrowserClient *browserClient{static_cast<QCefBrowserClient *>(client.get())};
		browserClient->widget = nullptr;
	}

	cefBrowser_ = nullptr;
}

CefRefPtr<CefBrowser> QCefWidgetInternal::createBrowser(WId handle, QSize size)
{
	CefWindowInfo windowInfo;

#if CHROME_VERSION_BUILD >= 6533
	windowInfo.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
#endif

	windowInfo.SetAsChild(reinterpret_cast<CefWindowHandle>(handle), CefRect(0, 0, size.width(), size.height()));

	auto browserClient{std::make_unique<QCefBrowserClient>(this, script_, allowAllPopups_)};

	CefBrowserSettings settings;

	auto hostInstance{CefBrowserHost::CreateBrowserSync(windowInfo, browserClient.release(), url_, settings,
							    CefRefPtr<CefDictionaryValue>(), requestContext_)};

	return hostInstance;
}

#ifdef __linux__
static bool XWindowHasAtom(Display *display, Window w, Atom a)
{
	Atom type;
	int format;
	unsigned long nItems;
	unsigned long bytesAfter;
	unsigned char *data = NULL;

	if (XGetWindowProperty(display, w, a, 0, LONG_MAX, False, AnyPropertyType, &type, &format, &nItems, &bytesAfter,
			       &data) != Success) {
		return false;
	}

	if (data) {
		XFree(data);
	}

	return type != None;
}

/* On Linux / X11, CEF sets the XdndProxy of the toplevel window
 * it's attached to, so that it can read drag events. When this
 * toplevel happens to be OBS Studio's main window (e.g. when a
 * browser panel is docked into to the main window), setting the
 * XdndProxy atom ends up breaking DnD of sources and scenes. Thus,
 * we have to manually unset this atom.
 */
void QCefWidgetInternal::unsetToplevelXdndProxy()
{
	if (!cefBrowser_) {
		return;
	}

	CefWindowHandle browserHandle{cefBrowser_->GetHost()->GetWindowHandle()};
	Display *xDisplay = cef_get_xdisplay();
	Window toplevel, root, parent, *children;
	unsigned int nChildren;
	bool found = false;

	toplevel = browserHandle;

	// Find the toplevel
	Atom netWmPidAtom = XInternAtom(xDisplay, "_NET_WM_PID", False);
	do {
		if (XQueryTree(xDisplay, toplevel, &root, &parent, &children, &nChildren) == 0) {
			return;
		}

		if (children) {
			XFree(children);
		}

		if (root == parent || !XWindowHasAtom(xDisplay, parent, netWmPidAtom)) {
			found = true;
			break;
		}
		toplevel = parent;
	} while (true);

	if (!found) {
		return;
	}

	// Check if the XdndProxy property is set
	Atom xDndProxyAtom = XInternAtom(xDisplay, "XdndProxy", False);
	if (needsDeleteXdndProxy && !XWindowHasAtom(xDisplay, toplevel, xDndProxyAtom)) {
		QueueCEFTask([this]() { unsetToplevelXdndProxy(); });
		return;
	}

	XDeleteProperty(xDisplay, toplevel, xDndProxyAtom);
	needsDeleteXdndProxy = false;
}
#endif

QCefTaskResult QCefWidgetInternal::tryCreateBrowser()
{
#ifdef __APPLE__
	WId handle = winId();

	bool taskPosted = QueueCEFTask([this, handle]() {
		if (cefBrowser_) {
			return;
		}

		QSize size = this->size();

		cefBrowser_ = createBrowser(handle, size);
	});
#else
	WId handle = window->winId();
	QSize size = this->size();
	size *= devicePixelRatioF();

	bool taskPosted = QueueCEFTask([this, handle, size]() {
		if (cefBrowser_) {
			return;
		}

		cefBrowser_ = createBrowserInstance(handle, size);
	});

	if (taskPosted) {
		if (cefBrowser_ && !container_) {
			container_ = QWidget::createWindowContainer(window, this);
			container_->show();
		}

		Resize();
	}
#endif
#ifdef __linux__
	bool proxyTaskPosted = false;
	if (taskPosted) {
		xndProxyTaskPosted = QueueCEFTask([this]() { unsetToplevelXdndProxy(); });
	}

	QCefTaskResult result = (taskPosted && xndProxyTaskPosted) ? QCefTaskResult::Success : QCefTaskResult::Failure;
#else
	QCefTaskResult result = (taskPosted) ? QCefTaskResult::Success : QCefTaskResult::Failure;
#endif

	return result;
}

void QCefWidgetInternal::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
#ifndef __APPLE__
	tryResize();
#endif
}

void QCefWidgetInternal::tryResize()
{
#ifdef __APPLE__
	return;
#else
	QSize size = this->size() * devicePixelRatioF();

	bool taskPosted = QueueCEFTask([this, size]() {
		if (!cefBrowser_) {
			return;
		}

		CefWindowHandle handle{cefBrowser_->GetHost()->GetWindowHandle()};

		if (!handle) {
			return;
		}

#ifdef _WIN32
		SetWindowPos((HWND)handle, nullptr, 0, 0, size.width(), size.height(),
			     SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		SendMessage((HWND)handle, WM_SIZE, 0, MAKELPARAM(size.width(), size.height()));
#else
		Display *xDisplay = cef_get_xdisplay();

		if (!xDisplay) {
			return;
		}

		XWindowChanges changes = {0};
		changes.x = 0;
		changes.y = 0;
		changes.width = size.width();
		changes.height = size.height();
		XConfigureWindow(xDisplay, (Window)handle, CWX | CWY | CWHeight | CWWidth, &changes);
#if CHROME_VERSION_BUILD >= 4638
		XSync(xDisplay, false);
#endif
#endif
	});

	if (taskPosted && container_) {
		container_->resize(size.width(), size.height());
	}
#endif
}

void QCefWidgetInternal::handleTitleChange(CefRefPtr<CefBrowser> browser, const std::string &title)
{
	if (!cefBrowser_->IsSame(browser)) {
		return;
	}

	QString titleUtf16 = QString::fromStdString(title);

	QMetaObject::invokeMethod(this, "titleChanged", Q_ARG(QString, titleUtf16));
}

std::string QCefWidgetInternal::getScript() const
{
	return script_;
}

void QCefWidgetInternal::finishCloseBrowser()
{
	emit readyToClose();
}

void QCefWidgetInternal::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);

	if (!cefBrowser_) {
		obs_browser_initialize();

		QCefTaskResult result = tryCreateBrowser();

		// Theoretically the "OperationsController" handling the single threaded task runners used by CEF should
		// be initialized when "obs_browser_initialize" returns and thus accept new tasks. If it does not accept
		// tasks, initialization might take longer than expected. This provides a "second chance" for the
		// OperationsController after a grace period of 500ms.
		if (result == QCefTaskResult::Failure) {
			QTimer::singleShot(500, this, [this]() { tryCreateBrowser(); });
		}
	}
}

QPaintEngine *QCefWidgetInternal::paintEngine() const
{
	return nullptr;
}

void QCefWidgetInternal::setURL(const std::string &url)
{
	url_ = url;

	if (cefBrowser_) {
		cefBrowser_->GetMainFrame()->LoadURL(url);
	}
}

void QCefWidgetInternal::reloadPage()
{
	if (cefBrowser_) {
		cefBrowser_->ReloadIgnoreCache();
	}
}

void QCefWidgetInternal::setStartupScript(const std::string &script)
{
	script_ = script;
}

void QCefWidgetInternal::executeJavaScript(const std::string &script)
{
	if (!cefBrowser_) {
		return;
	}

	CefRefPtr<CefFrame> frame = cefBrowser_->GetMainFrame();
	std::string url = frame->GetURL();
	frame->ExecuteJavaScript(script, url, 0);
}

void QCefWidgetInternal::allowAllPopups(bool allow)
{
	allowAllPopups_ = allow;
}

bool QCefWidgetInternal::zoomPage(int direction)
{
	if (!cefBrowser_ || direction < -1 || direction > 1) {
		return false;
	}

	CefRefPtr<CefBrowserHost> host{cefBrowser_->GetHost()};

	if (direction == 0) {
		// Reset zoom
		host->SetZoomLevel(0);
		return true;
	}

	int currentZoomPercent = round(pow(1.2, host->GetZoomLevel()) * 100.0);
	int zoomCount = sizeof(zoomLevels) / sizeof(zoomLevels[0]);
	int zoomIdx = 0;

	while (zoomIdx < zoomCount) {
		if (zoomLevels[zoomIdx] == currentZoomPercent) {
			break;
		}
		zoomIdx++;
	}

	if (zoomIdx == zoomCount) {
		return false;
	}

	int newZoomIdx = zoomIdx;
	if (direction == -1 && zoomIdx > 0) {
		// Zoom out
		newZoomIdx -= 1;
	} else if (direction == 1 && zoomIdx >= 0 && zoomIdx < zoomCount - 1) {
		// Zoom in
		newZoomIdx += 1;
	}

	if (newZoomIdx != zoomIdx) {
		int newZoomLvl = zoomLevels[newZoomIdx];
		// SetZoomLevel only accepts a zoomLevel, not a percentage
		host->SetZoomLevel(log(newZoomLvl / 100.0) / log(1.2));
		return true;
	}
	return false;
}
