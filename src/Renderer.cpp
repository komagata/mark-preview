#include "Renderer.h"
#include "Resources.h"

#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>

#include <wx/filename.h>
#include <wx/filesys.h>

#include <cctype>
#include <cstdlib>
#include <string>

namespace mp {

namespace {

bool g_extensions_registered = false;

void EnsureExtensions() {
    if (g_extensions_registered) return;
    cmark_gfm_core_extensions_ensure_registered();
    g_extensions_registered = true;
}

void AttachExtension(cmark_parser* parser, const char* name) {
    cmark_syntax_extension* ext = cmark_find_syntax_extension(name);
    if (ext) cmark_parser_attach_syntax_extension(parser, ext);
}

bool NeedsRewrite(const char* url) {
    if (!url || !*url) return false;
    if (url[0] == '#') return false;       // in-page anchor
    if (url[0] == '/') return false;       // absolute path — let WebKit handle
    // URL with a scheme (http:, https:, file:, data:, mailto:, etc.)
    const char* p = url;
    while (*p && (isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.')) ++p;
    if (*p == ':') return false;
    return true;
}

// Rewrite relative image/link URLs in the AST to absolute file:// URLs
// (resolved against base_prefix). WebKit's file:// same-origin policy treats
// the page file (loaded from /tmp/.../_page.html) and the markdown's local
// assets as distinct origins; absolutizing here side-steps the issue.
void RewriteUrls(cmark_node* root, const std::string& base_prefix) {
    if (base_prefix.empty()) return;
    cmark_iter* iter = cmark_iter_new(root);
    cmark_event_type ev;
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev != CMARK_EVENT_ENTER) continue;
        cmark_node* node = cmark_iter_get_node(iter);
        cmark_node_type t = cmark_node_get_type(node);
        if (t != CMARK_NODE_LINK && t != CMARK_NODE_IMAGE) continue;
        const char* url = cmark_node_get_url(node);
        if (!NeedsRewrite(url)) continue;
        std::string absurl = base_prefix + url;
        cmark_node_set_url(node, absurl.c_str());
    }
    cmark_iter_free(iter);
}

std::string MarkdownToHtml(const std::string& md, const std::string& base_prefix) {
    EnsureExtensions();

    // No CMARK_OPT_GITHUB_PRE_LANG: highlight.js and Mermaid expect
    // `<pre><code class="language-X">`, not `<pre lang="X">`.
    //
    // CMARK_OPT_UNSAFE: required so cmark-gfm doesn't strip file:// URLs as
    // "unsafe". The `tagfilter` extension still strips <script>, <iframe>,
    // <style>, etc. for the small XSS surface area we care about (viewing
    // local Markdown files written by the user).
    const int options = CMARK_OPT_DEFAULT
                      | CMARK_OPT_UNSAFE
                      | CMARK_OPT_FOOTNOTES
                      | CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES;

    cmark_parser* parser = cmark_parser_new(options);
    AttachExtension(parser, "table");
    AttachExtension(parser, "strikethrough");
    AttachExtension(parser, "autolink");
    AttachExtension(parser, "tagfilter");
    AttachExtension(parser, "tasklist");

    cmark_parser_feed(parser, md.data(), md.size());
    cmark_node* doc = cmark_parser_finish(parser);

    RewriteUrls(doc, base_prefix);

    cmark_llist* extensions = cmark_parser_get_syntax_extensions(parser);
    char* html_c = cmark_render_html(doc, options, extensions);
    std::string html = html_c ? html_c : "";
    free(html_c);

    cmark_node_free(doc);
    cmark_parser_free(parser);

    return html;
}

std::string EscapeHtml(const wxString& in) {
    std::string out;
    auto utf8 = in.utf8_string();
    out.reserve(utf8.size());
    for (char c : utf8) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;";  break;
            case '>':  out += "&gt;";  break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default:   out += c; break;
        }
    }
    return out;
}

void ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

} // namespace

RenderResult Renderer::Render(const std::string& markdown_utf8,
                              const wxString&    file_path,
                              Theme              theme,
                              long               scroll_y) {
    wxString base_url;
    if (!file_path.empty()) {
        wxFileName fn(file_path);
        wxString dir = fn.GetPath();
        if (!dir.empty()) {
            wxFileName d;
            d.AssignDir(dir);
            base_url = wxFileSystem::FileNameToURL(d);
            if (!base_url.EndsWith("/")) base_url += "/";
        }
    }

    std::string body = MarkdownToHtml(markdown_utf8, base_url.utf8_string());

    std::string tmpl = Resources::GetTemplate();

    wxString title = file_path.empty() ? "mark-preview" : wxFileName(file_path).GetFullName();
    const char* theme_name = ThemeName(theme);
    const char* ghmd_css = (theme == Theme::Dark) ? "github-dark.css" : "github-light.css";
    const char* hljs_css = (theme == Theme::Dark) ? "github-dark.min.css" : "github.min.css";

    ReplaceAll(tmpl, "__CONTENT__", body);
    ReplaceAll(tmpl, "__THEME__", theme_name);
    ReplaceAll(tmpl, "__TITLE__", EscapeHtml(title));
    ReplaceAll(tmpl, "__BASE_URL__", EscapeHtml(base_url));
    ReplaceAll(tmpl, "__ASSETS_URL__", EscapeHtml(Resources::GetAssetsBaseUrl()));
    ReplaceAll(tmpl, "__GHMD_CSS__", ghmd_css);
    ReplaceAll(tmpl, "__HLJS_CSS__", hljs_css);
    ReplaceAll(tmpl, "__SCROLL_Y__", std::to_string(scroll_y));

    return { std::move(tmpl), base_url };
}

} // namespace mp
