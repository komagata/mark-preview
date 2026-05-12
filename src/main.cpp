#include "MainFrame.h"
#include "Resources.h"

#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/string.h>
#include <cstdio>
#include <cstdlib>

namespace {
// On NVIDIA + XWayland (the common Omarchy/Hyprland setup), WebKit2GTK's
// DMABUF renderer fails to allocate GBM buffers and the WebView ends up
// rendering a blank surface. Disabling the DMABUF renderer (but keeping
// the accelerated compositor — needed for mouse-wheel scrolling to work)
// fixes both issues. Users can override these env vars themselves.
struct WebKitEnv {
    WebKitEnv() {
        setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", /*overwrite=*/0);
    }
} g_webkit_env;
} // namespace

namespace mp {

class App : public wxApp {
public:
    bool OnInit() override;
    int  OnExit() override;
    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;

private:
    wxString initial_file_;
    bool     show_version_ = false;
};

void App::OnInitCmdLine(wxCmdLineParser& parser) {
    static const wxCmdLineEntryDesc desc[] = {
        { wxCMD_LINE_SWITCH, "h", "help", "show this help",
          wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
        { wxCMD_LINE_SWITCH, "V", "version", "print version and exit",
          wxCMD_LINE_VAL_NONE, 0 },
        { wxCMD_LINE_PARAM, nullptr, nullptr, "markdown file",
          wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_NONE }
    };
    parser.SetDesc(desc);
    parser.SetSwitchChars("-");
}

bool App::OnCmdLineParsed(wxCmdLineParser& parser) {
    show_version_ = parser.Found("V");
    if (parser.GetParamCount() > 0) {
        initial_file_ = parser.GetParam(0);
    }
    return true;
}

bool App::OnInit() {
    SetAppName("mark-preview");
    SetAppDisplayName("mark-preview");

    if (!wxApp::OnInit()) return false;

    if (show_version_) {
        std::printf("mark-preview 0.1.0\n");
        std::exit(0);
    }

    if (!Resources::Init()) {
        wxLogError("Failed to extract embedded assets.");
        return false;
    }

    auto* frame = new MainFrame();
    frame->Show();

    if (!initial_file_.empty()) {
        wxString file = initial_file_;
        frame->CallAfter([frame, file]{ frame->OpenFile(file); });
    }
    return true;
}

int App::OnExit() {
    Resources::Cleanup();
    return wxApp::OnExit();
}

} // namespace mp

wxIMPLEMENT_APP(mp::App);
