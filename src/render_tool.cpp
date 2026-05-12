// Console-only renderer: prints the full HTML document for a given .md file
// to stdout. Used by the test script to verify the markdown→HTML pipeline
// without requiring a display.

#include "Renderer.h"
#include "Resources.h"
#include "Theme.h"

#include <wx/app.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <wx/string.h>

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s <markdown-file>\n", argv[0]);
        return 2;
    }

    wxInitializer init(argc, argv);
    if (!init.IsOk()) {
        std::fprintf(stderr, "wxInitializer failed\n");
        return 1;
    }

    if (!mp::Resources::Init()) {
        std::fprintf(stderr, "Resources::Init failed\n");
        return 1;
    }

    wxString path = wxString::FromUTF8(argv[1]);
    wxFile f;
    if (!f.Open(path, wxFile::read)) {
        std::fprintf(stderr, "Cannot open %s\n", argv[1]);
        return 1;
    }
    wxFileOffset size = f.Length();
    std::string md(static_cast<size_t>(size), '\0');
    f.Read(md.data(), size);

    wxFileName fn(path);
    fn.MakeAbsolute();

    // Console build: skip display-dependent theme detection.
    mp::RenderResult r = mp::Renderer::Render(md, fn.GetFullPath(),
                                              mp::Theme::Light, 0);
    std::fwrite(r.html.data(), 1, r.html.size(), stdout);
    std::fflush(stdout);

    mp::Resources::Cleanup();
    return 0;
}
