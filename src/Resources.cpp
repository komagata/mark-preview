#include "Resources.h"

#include "EmbeddedAssetsTable.h"

#include <wx/filename.h>
#include <wx/file.h>
#include <wx/stdpaths.h>
#include <wx/log.h>
#include <wx/utils.h>
#include <wx/dir.h>
#include <wx/filesys.h>

#include <string>

namespace mp {

namespace {

wxString g_assets_dir;
std::string g_template;

bool WriteAssetToFile(const embedded::AssetEntry& entry, const wxString& dir) {
    wxString rel = wxString::FromUTF8(entry.rel_path);
    rel.Replace("/", wxString(wxFileName::GetPathSeparator()));
    wxString full = dir + wxFileName::GetPathSeparator() + rel;
    wxFileName fn = wxFileName::FileName(full);
    wxString parent = fn.GetPath();
    if (!parent.empty() && !wxFileName::DirExists(parent)) {
        if (!wxFileName::Mkdir(parent, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
            wxLogError("Failed to create directory: %s", parent);
            return false;
        }
    }
    wxFile out;
    if (!out.Create(fn.GetFullPath(), /*overwrite=*/true)) {
        wxLogError("Failed to create asset file: %s", fn.GetFullPath());
        return false;
    }
    if (out.Write(entry.data, entry.len) != entry.len) {
        wxLogError("Short write for asset: %s", fn.GetFullPath());
        return false;
    }
    return true;
}

} // namespace

bool Resources::Init() {
    // Build a fresh per-process temp directory. wxWidgets lacks mkdtemp, so
    // emulate by appending PID + a counter.
    wxString base = wxStandardPaths::Get().GetTempDir();
    wxString try_dir;
    for (int i = 0; i < 1000; ++i) {
        try_dir = wxString::Format("%s/mark-preview-%ld-%d",
                                   base, static_cast<long>(wxGetProcessId()), i);
        if (!wxFileName::DirExists(try_dir)) {
            if (wxFileName::Mkdir(try_dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
                g_assets_dir = try_dir;
                break;
            }
        }
    }
    if (g_assets_dir.empty()) {
        wxLogError("Could not create temp asset directory under %s", base);
        return false;
    }

    for (unsigned int i = 0; i < embedded::kAssetTableSize; ++i) {
        const auto& entry = embedded::kAssetTable[i];
        if (std::string(entry.rel_path) == "template.html") {
            g_template.assign(reinterpret_cast<const char*>(entry.data), entry.len);
            continue; // keep in-memory only
        }
        if (!WriteAssetToFile(entry, g_assets_dir)) {
            return false;
        }
    }
    return true;
}

void Resources::Cleanup() {
    if (g_assets_dir.empty()) return;
    if (wxFileName::DirExists(g_assets_dir)) {
        wxDir::Remove(g_assets_dir, wxPATH_RMDIR_RECURSIVE);
    }
    g_assets_dir.clear();
    g_template.clear();
}

wxString Resources::GetAssetsBaseUrl() {
    if (g_assets_dir.empty()) return {};
    wxFileName fn;
    fn.AssignDir(g_assets_dir);
    wxString url = wxFileSystem::FileNameToURL(fn);
    while (url.EndsWith("/")) url.RemoveLast();
    return url;
}

wxString Resources::GetAssetsDir() { return g_assets_dir; }

const std::string& Resources::GetTemplate() { return g_template; }

} // namespace mp
