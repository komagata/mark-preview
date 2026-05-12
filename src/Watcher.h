#pragma once

#include <wx/event.h>
#include <wx/fswatcher.h>
#include <wx/timer.h>
#include <wx/string.h>
#include <memory>

namespace mp {

// Emitted (debounced) when the watched file is modified or replaced.
wxDECLARE_EVENT(MP_EVT_FILE_CHANGED, wxCommandEvent);

// Watches a single file and emits MP_EVT_FILE_CHANGED on its owner after
// activity has been quiet for `debounce_ms`. Handles atomic-save rename
// patterns that some editors use.
class FileWatcher : public wxEvtHandler {
public:
    FileWatcher(wxEvtHandler* owner, int debounce_ms = 150);
    ~FileWatcher() override;

    // Begin watching `path` (replaces any previous watch). Pass an empty
    // string to stop watching.
    void Watch(const wxString& path);

    // The path currently being watched (may be empty).
    const wxString& CurrentPath() const { return path_; }

private:
    void OnFsEvent(wxFileSystemWatcherEvent& evt);
    void OnDebounce(wxTimerEvent& evt);

    wxEvtHandler*                       owner_;
    int                                 debounce_ms_;
    wxString                            path_;
    std::unique_ptr<wxFileSystemWatcher> fsw_;
    wxTimer                              debounce_;
};

} // namespace mp
