#include "MainFrame.h"
#include "Resources.h"

#include <wx/sizer.h>
#include <wx/statusbr.h>

#ifdef __WXGTK__
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <webkit2/webkit2.h>
#endif
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/utils.h>
#include <wx/datetime.h>
#include <wx/uri.h>
#include <wx/log.h>
#include <wx/accel.h>

namespace mp {

namespace {

constexpr const char* kAppName = "mark-preview";

bool ReadFileUtf8(const wxString& path, std::string& out) {
    wxFile f;
    if (!f.Open(path, wxFile::read)) return false;
    wxFileOffset size = f.Length();
    if (size < 0) return false;
    out.resize(static_cast<size_t>(size));
    if (size == 0) return true;
    ssize_t n = f.Read(out.data(), static_cast<size_t>(size));
    if (n < 0) return false;
    out.resize(static_cast<size_t>(n));
    return true;
}

bool IsMarkdownPath(const wxString& path) {
    wxString ext = path.AfterLast('.').Lower();
    return ext == "md" || ext == "markdown" || ext == "mdown" || ext == "mkd" || ext == "mkdn";
}

std::string WelcomeHtml() {
    return R"(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>mark-preview</title>
<style>
html, body { height: 100%; margin: 0; font-family: system-ui, -apple-system, sans-serif; }
body { display: grid; place-items: center; color: #6e7781; background: #ffffff; }
@media (prefers-color-scheme: dark) {
  body { color: #8b949e; background: #0d1117; }
}
.box { text-align: center; opacity: 0.9; }
.box h1 { font-weight: 600; font-size: 18px; margin: 0 0 8px; }
.box p { font-size: 13px; margin: 4px 0; }
kbd { font-family: ui-monospace, monospace; background: rgba(127,127,127,0.15);
      border-radius: 4px; padding: 1px 6px; }
</style></head><body>
<div class="box">
<h1>mark-preview</h1>
<p>Drop a <code>.md</code> file here, or use <kbd>Ctrl</kbd>+<kbd>O</kbd>.</p>
</div></body></html>)";
}

} // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, kAppName, wxDefaultPosition, wxSize(1000, 800)) {
    BuildAccelerators();
    CreateStatusBar(2);
    int widths[] = { -3, -1 };
    SetStatusWidths(2, widths);

    webview_ = wxWebView::New(this, wxID_ANY);
    if (!webview_) {
        wxLogError("Failed to create wxWebView. Is webkit2gtk installed?");
    } else {
        webview_->EnableContextMenu(false);
        Bind(wxEVT_WEBVIEW_NAVIGATING, &MainFrame::OnNavigating, this, webview_->GetId());
        Bind(wxEVT_WEBVIEW_LOADED,     &MainFrame::OnLoaded,     this, webview_->GetId());
        // WebKit2GTK occasionally drops mouse wheel events on certain
        // monitor/scaling/tiling combinations under XWayland. Forward wheel
        // events to the page via JS scrollBy as a robust fallback.
        webview_->Bind(wxEVT_MOUSEWHEEL, &MainFrame::OnMouseWheel, this);
        Bind(wxEVT_MOUSEWHEEL,           &MainFrame::OnMouseWheel, this);

#ifdef __WXGTK__
        // WebKit2GTK blocks file:// pages from loading other file:// resources
        // (CSS/JS/fonts) by default. Allow it so the bundled assets in our
        // temp dir are reachable from rendered markdown pages.
        if (auto* native = static_cast<WebKitWebView*>(webview_->GetNativeBackend())) {
            WebKitSettings* settings = webkit_web_view_get_settings(native);
            webkit_settings_set_allow_file_access_from_file_urls(settings, TRUE);
            webkit_settings_set_allow_universal_access_from_file_urls(settings, TRUE);

            // WebKit2GTK swallows GTK scroll-event before wxWidgets sees them,
            // and on some monitor / DPI / tiling combinations its own scroll
            // handling stops working. Hook GTK scroll-event directly and
            // delegate to JS window.scrollBy.
            gtk_widget_add_events(GTK_WIDGET(native), GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
            g_signal_connect(native, "scroll-event",
                             G_CALLBACK(&MainFrame::OnGtkScrollEvent), this);
        }
#endif
    }

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    if (webview_) sizer->Add(webview_, 1, wxEXPAND);
    SetSizer(sizer);

    // Set drop target on both the frame and the webview. wxWebView on GTK
    // tends to consume drop events before they reach the frame.
    SetDropTarget(new MarkdownDropTarget(this));
    if (webview_) webview_->SetDropTarget(new MarkdownDropTarget(this));

    Bind(wxEVT_MENU, &MainFrame::OnOpen,       this, ID_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnReload,     this, ID_RELOAD);
    Bind(wxEVT_MENU, &MainFrame::OnCloseFile,  this, ID_CLOSE_FILE);
    Bind(wxEVT_MENU, &MainFrame::OnQuit,       this, ID_QUIT);
    Bind(wxEVT_MENU, &MainFrame::OnZoomIn,     this, ID_ZOOM_IN);
    Bind(wxEVT_MENU, &MainFrame::OnZoomOut,    this, ID_ZOOM_OUT);
    Bind(wxEVT_MENU, &MainFrame::OnZoomReset,  this, ID_ZOOM_RESET);
    Bind(wxEVT_MENU, &MainFrame::OnAbout,      this, ID_ABOUT);
    Bind(MP_EVT_FILE_CHANGED, &MainFrame::OnFileChanged, this);
    Bind(wxEVT_SYS_COLOUR_CHANGED, &MainFrame::OnSysColourChanged, this);

    theme_ = ThemeDetector::Detect();
    watcher_ = std::make_unique<FileWatcher>(this);

    UpdateTitle();
    UpdateStatusFile();
    // Defer the first SetPage until after the frame is shown and the
    // WebKitWebView has been realized on screen. Otherwise WebKit2GTK
    // silently drops the load.
    CallAfter([this]{ if (current_path_.empty()) ShowWelcome(); });
}

void MainFrame::BuildAccelerators() {
    // No menu bar — keep the viewer chrome minimal like Omarchy's image
    // preview. All commands are reachable via keyboard shortcuts.
    wxAcceleratorEntry entries[] = {
        { wxACCEL_CTRL, (int)'O', ID_OPEN },
        { wxACCEL_CTRL, (int)'R', ID_RELOAD },
        { wxACCEL_CTRL, (int)'W', ID_CLOSE_FILE },
        { wxACCEL_CTRL, (int)'Q', ID_QUIT },
        { wxACCEL_CTRL, (int)'=', ID_ZOOM_IN },       // bare +
        { wxACCEL_CTRL | wxACCEL_SHIFT, (int)'=', ID_ZOOM_IN }, // Ctrl+Shift+= == Ctrl++
        { wxACCEL_CTRL, WXK_NUMPAD_ADD,      ID_ZOOM_IN },
        { wxACCEL_CTRL, (int)'-', ID_ZOOM_OUT },
        { wxACCEL_CTRL, WXK_NUMPAD_SUBTRACT, ID_ZOOM_OUT },
        { wxACCEL_CTRL, (int)'0', ID_ZOOM_RESET },
        { wxACCEL_NORMAL, WXK_F1, ID_ABOUT },
    };
    constexpr int n = sizeof(entries) / sizeof(entries[0]);
    SetAcceleratorTable(wxAcceleratorTable(n, entries));
}

void MainFrame::OpenFile(const wxString& path) {
    if (path.empty()) {
        current_path_.clear();
        watcher_->Watch("");
        ShowWelcome();
        UpdateTitle();
        UpdateStatusFile();
        return;
    }

    wxFileName fn(path);
    fn.MakeAbsolute();
    wxString abs = fn.GetFullPath();

    if (!wxFileName::FileExists(abs)) {
        wxMessageBox(wxString::Format("File not found:\n%s", abs),
                     "mark-preview", wxOK | wxICON_ERROR, this);
        return;
    }

    std::string md;
    if (!ReadFileUtf8(abs, md)) {
        wxMessageBox(wxString::Format("Cannot read file:\n%s", abs),
                     "mark-preview", wxOK | wxICON_ERROR, this);
        return;
    }
    if (!md.empty()) {
        // Quick UTF-8 sanity check: try to decode through wxString.
        wxString tmp = wxString::FromUTF8(md.data(), md.size());
        if (tmp.empty() && md.size() > 0) {
            wxMessageBox("Not a valid UTF-8 text file.",
                         "mark-preview", wxOK | wxICON_ERROR, this);
            return;
        }
    }

    current_path_ = abs;
    watcher_->Watch(abs);
    DoRender(md, abs, 0);
    UpdateTitle();
    UpdateStatusFile();
}

void MainFrame::Reload(bool keep_scroll) {
    if (current_path_.empty()) return;
    std::string md;
    if (!ReadFileUtf8(current_path_, md)) {
        SetStatusText("File missing", 0);
        return;
    }
    long y = keep_scroll ? GetCurrentScrollY() : 0;
    DoRender(md, current_path_, y);
    UpdateStatusReloadTime();
}

void MainFrame::LoadHtmlPage(const std::string& html) {
    if (!webview_) return;
    // Write the page to a file under the assets temp dir, then LoadURL it.
    // Loading from the same directory as bundled CSS/JS/fonts means WebKit
    // treats them as same-origin and avoids file:// access restrictions.
    wxString assets_dir = Resources::GetAssetsDir();
    if (assets_dir.empty()) {
        webview_->SetPage(html, "");
        return;
    }
    wxString page_path = assets_dir + wxFileName::GetPathSeparator() + "_page.html";
    wxFile out;
    if (out.Create(page_path, /*overwrite=*/true)) {
        out.Write(html.data(), html.size());
        out.Close();
        webview_->LoadURL("file://" + page_path);
    } else {
        webview_->SetPage(html, "");
    }
}

void MainFrame::ShowWelcome() {
    LoadHtmlPage(WelcomeHtml());
}

void MainFrame::DoRender(const std::string& md, const wxString& file_path, long scroll_y) {
    if (!webview_) return;
    RenderResult r = Renderer::Render(md, file_path, theme_, scroll_y);
    LoadHtmlPage(r.html);
}

long MainFrame::GetCurrentScrollY() {
    if (!webview_) return 0;
    wxString out;
    if (webview_->RunScript("String(window.scrollY || 0)", &out)) {
        long v = 0;
        if (out.ToLong(&v)) return v;
    }
    return 0;
}

void MainFrame::UpdateTitle() {
    if (current_path_.empty()) {
        SetTitle(kAppName);
    } else {
        wxString name = wxFileName(current_path_).GetFullName();
        SetTitle(name + " - " + kAppName);
    }
}

void MainFrame::UpdateStatusFile() {
    SetStatusText(current_path_.empty() ? "" : current_path_, 0);
    SetStatusText("", 1);
}

void MainFrame::UpdateStatusReloadTime() {
    SetStatusText(wxDateTime::Now().Format("Reloaded %H:%M:%S"), 1);
}

void MainFrame::OnOpen(wxCommandEvent&) {
    wxFileDialog dlg(this, "Open Markdown file", "", "",
                     "Markdown (*.md;*.markdown;*.mdown;*.mkd)|*.md;*.markdown;*.mdown;*.mkd|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        OpenFile(dlg.GetPath());
    }
}

void MainFrame::OnReload(wxCommandEvent&) { Reload(true); }

void MainFrame::OnCloseFile(wxCommandEvent&) { OpenFile(""); }

void MainFrame::OnQuit(wxCommandEvent&) { Close(true); }

void MainFrame::OnZoomIn(wxCommandEvent&) {
    if (!webview_) return;
    zoom_ = std::min(zoom_ + 0.1f, 3.0f);
    webview_->SetZoomFactor(zoom_);
}

void MainFrame::OnZoomOut(wxCommandEvent&) {
    if (!webview_) return;
    zoom_ = std::max(zoom_ - 0.1f, 0.3f);
    webview_->SetZoomFactor(zoom_);
}

void MainFrame::OnZoomReset(wxCommandEvent&) {
    if (!webview_) return;
    zoom_ = 1.0f;
    webview_->SetZoomFactor(zoom_);
}

void MainFrame::OnAbout(wxCommandEvent&) {
    wxMessageBox("mark-preview — simple Markdown viewer\n"
                 "Built with wxWidgets, cmark-gfm, KaTeX, Mermaid, highlight.js.\n",
                 "About mark-preview", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnFileChanged(wxCommandEvent&) { Reload(true); }

void MainFrame::OnNavigating(wxWebViewEvent& evt) {
    wxString url = evt.GetURL();

    // Allow internal navigations (about:blank, the initial SetPage URL).
    if (url.StartsWith("about:")) return;

    // Asset URLs (in our temp dir) — let them through for CSS/JS/font loads.
    wxString assets = Resources::GetAssetsBaseUrl();
    if (!assets.empty() && url.StartsWith(assets)) return;

    // file:// targeting another .md → reopen in this viewer.
    if (url.StartsWith("file://")) {
        wxURI uri(url);
        wxString local;
        if (uri.HasPath()) {
            local = wxURI::Unescape(uri.GetPath());
        }
        if (IsMarkdownPath(local)) {
            evt.Veto();
            OpenFile(local);
            return;
        }
        // Other local files (images, etc.) — allow the WebView to load.
        return;
    }

    // External http(s)/mailto/etc. → hand off to OS default browser.
    if (url.StartsWith("http://") || url.StartsWith("https://") ||
        url.StartsWith("mailto:") || url.StartsWith("ftp://")) {
        evt.Veto();
        wxLaunchDefaultBrowser(url);
        return;
    }

    // Unknown schemes (javascript:, data:, etc.) — block.
    if (!url.StartsWith("file://")) {
        evt.Veto();
    }
}

void MainFrame::OnLoaded(wxWebViewEvent& /*evt*/) {
    // Re-apply zoom in case the new page reset it.
    if (webview_) webview_->SetZoomFactor(zoom_);
}

void MainFrame::ScrollWebViewBy(long dx, long dy) {
    if (!webview_) return;
    wxString js = wxString::Format("window.scrollBy(%ld, %ld);", dx, dy);
    webview_->RunScript(js);
}

#ifdef __WXGTK__
int MainFrame::OnGtkScrollEvent(void* /*widget*/, void* gdk_event, void* user_data) {
    auto* frame = static_cast<MainFrame*>(user_data);
    auto* evt   = static_cast<GdkEvent*>(gdk_event);
    if (!frame || !evt) return FALSE;

    GdkModifierType mods{};
    if (gdk_event_get_state(evt, &mods) && (mods & GDK_CONTROL_MASK)) {
        return FALSE; // Ctrl+wheel → zoom; let WebKit handle.
    }

    double dx = 0, dy = 0;
    if (gdk_event_get_scroll_deltas(evt, &dx, &dy)) {
        dx *= 60; dy *= 60;
    } else {
        GdkScrollDirection dir = GDK_SCROLL_SMOOTH;
        if (gdk_event_get_scroll_direction(evt, &dir)) {
            switch (dir) {
                case GDK_SCROLL_UP:    dy = -120; break;
                case GDK_SCROLL_DOWN:  dy =  120; break;
                case GDK_SCROLL_LEFT:  dx = -120; break;
                case GDK_SCROLL_RIGHT: dx =  120; break;
                default: break;
            }
        }
    }
    frame->ScrollWebViewBy(static_cast<long>(dx), static_cast<long>(dy));
    return TRUE;
}
#endif

// wxEVT_MOUSEWHEEL on the wxFrame / wxWebView almost never fires in practice
// because WebKit2GTK consumes scroll events at the GTK signal level (see
// OnGtkScrollEvent). Kept as a safety net for non-GTK builds.
void MainFrame::OnMouseWheel(wxMouseEvent& evt) {
    if (!webview_) { evt.Skip(); return; }
    if (evt.ControlDown()) { evt.Skip(); return; }
    int rotation = evt.GetWheelRotation();
    int delta    = evt.GetWheelDelta() ? evt.GetWheelDelta() : 120;
    int lines    = evt.GetLinesPerAction() > 0 ? evt.GetLinesPerAction() : 3;
    long dy      = -static_cast<long>(rotation) * lines * 16 / delta;
    long dx      = 0;
    if (evt.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL) { dx = -dy; dy = 0; }
    ScrollWebViewBy(dx, dy);
}

void MainFrame::OnSysColourChanged(wxSysColourChangedEvent& evt) {
    Theme t = ThemeDetector::Detect();
    if (t != theme_) {
        theme_ = t;
        if (!current_path_.empty()) Reload(true);
    }
    evt.Skip();
}

bool MarkdownDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& files) {
    if (files.empty() || !frame_) return false;
    frame_->OpenFile(files[0]);
    return true;
}

} // namespace mp
