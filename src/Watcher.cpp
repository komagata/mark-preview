#include "Watcher.h"

#include <wx/filename.h>

namespace mp {

wxDEFINE_EVENT(MP_EVT_FILE_CHANGED, wxCommandEvent);

FileWatcher::FileWatcher(wxEvtHandler* owner, int debounce_ms)
    : owner_(owner), debounce_ms_(debounce_ms), debounce_(this) {
    Bind(wxEVT_TIMER, &FileWatcher::OnDebounce, this);
}

FileWatcher::~FileWatcher() {
    if (debounce_.IsRunning()) debounce_.Stop();
}

void FileWatcher::Watch(const wxString& path) {
    if (fsw_) {
        fsw_->RemoveAll();
        fsw_.reset();
    }
    path_ = path;
    if (path_.empty()) return;

    fsw_ = std::make_unique<wxFileSystemWatcher>();
    fsw_->SetOwner(this);
    Bind(wxEVT_FSWATCHER, &FileWatcher::OnFsEvent, this);

    wxFileName fn(path_);
    // Watch the parent directory so atomic-save (rename) is captured.
    wxString dir = fn.GetPath();
    if (dir.empty()) dir = wxGetCwd();
    wxFileName d;
    d.AssignDir(dir);
    fsw_->Add(d, wxFSW_EVENT_MODIFY | wxFSW_EVENT_RENAME | wxFSW_EVENT_CREATE);
}

void FileWatcher::OnFsEvent(wxFileSystemWatcherEvent& evt) {
    if (path_.empty()) return;

    wxString changed = evt.GetPath().GetFullPath();
    wxString new_path = evt.GetNewPath().GetFullPath();
    if (changed != path_ && new_path != path_) return;

    debounce_.Stop();
    debounce_.StartOnce(debounce_ms_);
}

void FileWatcher::OnDebounce(wxTimerEvent& /*evt*/) {
    if (!owner_ || path_.empty()) return;
    wxCommandEvent event(MP_EVT_FILE_CHANGED);
    event.SetString(path_);
    wxPostEvent(owner_, event);
}

} // namespace mp
