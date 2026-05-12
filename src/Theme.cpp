#include "Theme.h"

#include <wx/settings.h>

namespace mp {

Theme ThemeDetector::Detect() {
#if wxCHECK_VERSION(3, 1, 3)
    if (wxSystemSettings::GetAppearance().IsDark()) return Theme::Dark;
#else
    wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    int luminance = (bg.Red() * 299 + bg.Green() * 587 + bg.Blue() * 114) / 1000;
    if (luminance < 128) return Theme::Dark;
#endif
    return Theme::Light;
}

} // namespace mp
