#pragma once

#include <wx/string.h>
#include <string>

namespace mp {

// Extracts embedded third-party assets (KaTeX, Mermaid, highlight.js, CSS)
// to a per-process temporary directory at startup, and exposes URLs/paths
// for the renderer to reference them.
class Resources {
public:
    // Extract all assets to a fresh temp dir. Returns false on I/O failure.
    static bool Init();

    // Remove the temp dir. Safe to call multiple times.
    static void Cleanup();

    // file:// URL prefix for the assets dir (no trailing slash).
    static wxString GetAssetsBaseUrl();

    // Absolute filesystem path of the assets dir.
    static wxString GetAssetsDir();

    // Contents of template.html as a UTF-8 string.
    static const std::string& GetTemplate();
};

} // namespace mp
