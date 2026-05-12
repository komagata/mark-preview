#pragma once

#include "Renderer.h"
#include "Theme.h"
#include "Watcher.h"

#include <wx/frame.h>
#include <wx/string.h>
#include <wx/webview.h>
#include <wx/dnd.h>

#include <memory>

namespace mp {

class MainFrame : public wxFrame {
public:
    MainFrame();

    // Open `path` and render it. If `path` is empty, show the welcome page.
    void OpenFile(const wxString& path);

private:
    enum {
        ID_OPEN = wxID_OPEN,
        ID_RELOAD = wxID_REFRESH,
        ID_CLOSE_FILE = wxID_CLOSE,
        ID_QUIT = wxID_EXIT,
        ID_ZOOM_IN = wxID_ZOOM_IN,
        ID_ZOOM_OUT = wxID_ZOOM_OUT,
        ID_ZOOM_RESET = wxID_ZOOM_100,
        ID_ABOUT = wxID_ABOUT,
    };

    void BuildAccelerators();
    void Reload(bool keep_scroll = true);
    void ShowWelcome();
    void LoadHtmlPage(const std::string& html);
    void DoRender(const std::string& md, const wxString& file_path, long scroll_y);
    void OnOpen(wxCommandEvent&);
    void OnReload(wxCommandEvent&);
    void OnCloseFile(wxCommandEvent&);
    void OnQuit(wxCommandEvent&);
    void OnZoomIn(wxCommandEvent&);
    void OnZoomOut(wxCommandEvent&);
    void OnZoomReset(wxCommandEvent&);
    void OnAbout(wxCommandEvent&);
    void OnFileChanged(wxCommandEvent&);
    void OnNavigating(wxWebViewEvent& evt);
    void OnLoaded(wxWebViewEvent& evt);
    void OnSysColourChanged(wxSysColourChangedEvent& evt);
    void OnMouseWheel(wxMouseEvent& evt);

#ifdef __WXGTK__
    // GTK callback signature: gboolean(GtkWidget*, GdkEvent*, gpointer).
    // Declared with C linkage and friended below.
    static int OnGtkScrollEvent(void* widget, void* gdk_event, void* user_data);
#endif

    void ScrollWebViewBy(long dx, long dy);
    void UpdateTitle();
    void UpdateStatusFile();
    void UpdateStatusReloadTime();
    long GetCurrentScrollY();

    wxWebView*                       webview_ = nullptr;
    wxString                         current_path_;
    Theme                            theme_ = Theme::Light;
    float                            zoom_ = 1.0f;
    std::unique_ptr<FileWatcher>     watcher_;
};

// Drag and drop target: opens the first dropped file.
class MarkdownDropTarget : public wxFileDropTarget {
public:
    explicit MarkdownDropTarget(MainFrame* frame) : frame_(frame) {}
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& files) override;
private:
    MainFrame* frame_;
};

} // namespace mp
