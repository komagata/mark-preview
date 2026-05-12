#pragma once

#include "Theme.h"

#include <wx/string.h>
#include <string>

namespace mp {

struct RenderResult {
    std::string html;
    wxString    base_url; // file:// URL to pass to wxWebView::SetPage as base
};

class Renderer {
public:
    // Convert markdown (UTF-8) to a complete HTML document using the embedded
    // template + GitHub CSS + KaTeX/Mermaid/highlight.js asset URLs.
    //
    // `file_path` is the absolute path to the markdown file (used to build
    //   base_url so relative images/links resolve against its directory).
    // `scroll_y` is restored after render (use 0 for fresh load).
    static RenderResult Render(const std::string& markdown_utf8,
                               const wxString&    file_path,
                               Theme              theme,
                               long               scroll_y);
};

} // namespace mp
