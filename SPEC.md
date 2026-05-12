# mark-preview 仕様書

Omarchy（Arch Linux + Hyprland）上で動く、シンプルな Markdown 閲覧ビューア。

## 1. 目的とスコープ

### 1.1 目的
- ローカルの `.md` ファイルを GUI で開いて読むためのスタンドアロンビューア。
- GitHub Flavored Markdown（GFM）に加えて、数式（KaTeX）・図表（Mermaid）・コードハイライト（highlight.js）を表示できること。
- Omarchy デスクトップ環境に違和感なく溶け込むこと。

### 1.2 非スコープ（やらないこと）
- Markdown の編集機能。本ツールは **viewer** であり、編集は外部エディタ（Neovim 等）に委ねる。
- 2 ペイン（ソース + プレビュー）UI。
- ファイルツリー・サイドバー・目次・本文検索。
- 印刷／PDF/HTML エクスポート。
- 外部 URL リンクの埋め込みプレビュー（クリックでブラウザに委譲する）。

### 1.3 移植性方針
- 初版の動作確認対象は **Linux のみ**。ただし設計・実装は **macOS / Windows でも将来そのまま動かせる**ことを前提とする（詳細は §2、§14）。
- OS 固有 API を直接呼ばず、wxWidgets と C++ 標準ライブラリだけで賄えるものはそちらを使う。POSIX 専用関数（`fork` / `inotify_*` / `nl_langinfo` 等）も避ける。
- パス区切りは `std::filesystem` で統一。文字コードはファイル I/O も含めて UTF-8 固定（Windows 側では `_wfopen` 経由などにせず、wxWidgets の `wxFileName` / `wxFile` を通す）。

## 2. プラットフォーム要件

### 2.1 初版ターゲット
- **Linux (Omarchy 3.5+, Arch ベース)**、Wayland (Hyprland) で動作確認すること。
- 共通要件: wxWidgets 3.2 系、wxWebView、cmark-gfm、C++17、CMake 3.20+。

### 2.2 将来ターゲット（設計上だけ織り込み、初版では未検証でよい）
- **macOS 12+**（Apple Silicon / Intel 両対応）
- **Windows 10 / 11**

### 2.3 プラットフォーム別バックエンド
wxWidgets が抽象化する。**OS 固有 API を直接呼ばないこと**を原則とする。

| 機能 | Linux | macOS | Windows |
| --- | --- | --- | --- |
| GUI | wxGTK (GTK3) | wxOSX (Cocoa) | wxMSW (Win32) |
| ブラウザ | wxWebView + WebKit2GTK | wxWebView + WKWebView | wxWebView + Edge WebView2 |
| ファイル監視 | wxFileSystemWatcher (inotify) | wxFileSystemWatcher (FSEvents) | wxFileSystemWatcher (ReadDirectoryChangesW) |
| 外部ブラウザ起動 | `wxLaunchDefaultBrowser` (xdg-open) | 同 (open) | 同 (ShellExecute) |
| ダークテーマ判定 | GTK の prefer-dark | `NSAppearance` → wxSystemSettings | レジストリ `AppsUseLightTheme` → wxSystemSettings |

初版実装では wxWidgets の API のみを使い、`#ifdef __WXGTK__` / `__WXMAC__` / `__WXMSW__` の分岐は最小限（必要ならテーマ判定とアイコンパス程度）に留める。

## 3. 起動方法

| 経路 | 動作 |
| --- | --- |
| `mark-preview` | ファイル未指定で起動。空のウィンドウに「Open a Markdown file…」と表示。 |
| `mark-preview path/to/foo.md` | 指定ファイルを開いた状態で起動。 |
| `mark-preview --help` / `-h` | 使い方を標準出力に表示して終了。 |
| `mark-preview --version` / `-V` | バージョン番号を出力して終了。 |
| GUI: File > Open… | `wxFileDialog` で `.md` / `.markdown` を選択。 |
| GUI: ドラッグ＆ドロップ | ウィンドウへ `.md` をドロップで開く。 |

複数ファイル指定は受け付けない（最初の 1 つだけを開く）。複数同時閲覧したい場合は本ツールを複数プロセス起動する想定。

## 4. UI 構成

### 4.1 ウィンドウ
- 単一ウィンドウ・単一ペイン。中央に wxWebView を 100% 充填。
- 初期サイズ: 1000×800（DPI 非依存、wxWidgets が HiDPI を扱う）。
- ウィンドウタイトル: `<basename> — mark-preview`（未指定時は `mark-preview`）。

### 4.2 メニュー
```
File
  Open…           Ctrl+O
  Reload          Ctrl+R
  Close           Ctrl+W
  Quit            Ctrl+Q
View
  Zoom In         Ctrl++
  Zoom Out        Ctrl+-
  Reset Zoom      Ctrl+0
Help
  About
```

### 4.3 キーバインド
- `Ctrl+O` / `Ctrl+R` / `Ctrl+W` / `Ctrl+Q` … メニューと同じ。
- `Ctrl+ +` / `Ctrl+ -` / `Ctrl+ 0` … ズーム（wxWebView のスケール）。
- `Up/Down/PgUp/PgDn/Space/Home/End` … wxWebView 既定のスクロール。
- マウスホイール … スクロール。`Ctrl+ホイール` … ズーム。

実装上は `wxACCEL_CTRL`（wxWidgets のアクセラレータ定数）を使う。これは macOS では自動的に `Cmd` にマップされる。`Ctrl+` 表記はドキュメント上の表現であり、macOS 版では UI 上 `⌘` として表示される。

### 4.4 ステータスバー
- 左: 現在開いているファイルの絶対パス（未指定時は空）。
- 右: 最終再描画時刻（自動再読み込みが動いたことが分かるように）。

## 5. レンダリング方式

### 5.1 パイプライン
```
.md ファイル
  │ 読み込み（UTF-8 固定）
  ▼
cmark-gfm （C ライブラリを直接 link）
  │ HTML 文字列生成
  ▼
テンプレート HTML に埋め込み
  │ + KaTeX.js + Mermaid.js + highlight.js + theme CSS
  ▼
wxWebView::SetPage(html, base_url) で表示
  │ base_url = "file:///<開いたファイルのディレクトリ>/"
  ▼
JS が onload で数式 / Mermaid / コードを後処理
```

### 5.2 cmark-gfm 拡張の有効化
以下の GFM 拡張をすべて有効にする:
- `table`
- `strikethrough`
- `autolink`
- `tagfilter`
- `tasklist`

CommonMark オプション:
- `CMARK_OPT_UNSAFE` は **無効**（生 HTML を出さない。ローカルファイルとはいえ XSS 含み外部 MD を踏むことがあるため安全側に倒す）。
- `CMARK_OPT_SMART` は無効（タイポグラフィ変換は GitHub と揃えない）。

### 5.3 数式
- 区切り: `$...$`（インライン）、`$$...$$`（ブロック）。
- cmark-gfm 通過後、JS 側で `auto-render.min.js`（KaTeX 同梱）が DOM を走査して変換。
- KaTeX のオプション: `throwOnError: false`、`delimiters` に `$`/`$$`/`\(...\)`/`\[...\]` を登録。

### 5.4 図表
- 三連バッククォート + `mermaid` 言語タグのコードブロックを Mermaid.js が認識して描画。
- 初期化は `mermaid.initialize({ startOnLoad: true, theme: <ライト/ダーク連動> })`。

### 5.5 コードハイライト
- 三連バッククォート + 言語タグのコードブロック（mermaid 以外）を highlight.js が処理。
- 主要言語（ruby/go/c/cpp/python/javascript/typescript/bash/html/css/json/yaml/sql/markdown/diff）の言語パックを同梱。
- それ以外は `hljs.highlightAuto` でフォールバック。

## 6. テーマ

### 6.1 同梱する CSS
- `assets/github-light.css` … github-markdown-css のライト版相当。
- `assets/github-dark.css` … 同・ダーク版相当。
- いずれも本リポジトリにベンダリングして再配布する（MIT、要 NOTICE 表記）。

### 6.2 切替ルール
- 起動時に `wxSystemSettings::GetAppearance().IsDark()`（wxWidgets 3.1.3+）でダーク判定する。これにより Linux / macOS / Windows いずれでも統一 API で済む。
- HTML 側に両 CSS を埋め込み、`<html data-theme="dark|light">` を切り替えるだけで反映できる構造にする。
- ウィンドウ生存中にシステムテーマが変わった場合の追従は **best effort**。可能ならプラットフォーム共通の `wxEVT_SYS_COLOUR_CHANGED` を購読する。取れなくても許容。
- Mermaid と highlight.js のテーマ（CSS 配色）もライト／ダークで切り替える。

## 7. ファイル監視と再読み込み

### 7.1 監視対象
- 現在開いているファイル 1 本のみ。

### 7.2 実装
- `wxFileSystemWatcher` を使う（Linux では inotify 上の薄いラッパ）。
- 検知イベント: `wxFSW_EVENT_MODIFY` および `wxFSW_EVENT_RENAME`（エディタの atomic save 対策）。
- 再読み込みは「最後のイベントから 150 ms 静止したら 1 回だけ」発火するデバウンス処理を入れる。
- 再読み込み後、wxWebView の現在のスクロール位置を可能な範囲で復元する（`window.scrollY` を JS で取得→セット）。

### 7.3 手動再読み込み
- `Ctrl+R` または File > Reload。デバウンスを待たず即時実行。

## 8. リンクと外部リソース

| 対象 | 動作 |
| --- | --- |
| `http://` `https://` リンク | クリック時、wxWebView の遷移をブロックして `wxLaunchDefaultBrowser` で OS 既定ブラウザに渡す（Linux=xdg-open / macOS=open / Windows=ShellExecute、wxWidgets が自動分岐）。 |
| `#anchor`（同一文書内アンカー） | wxWebView 内で通常スクロール。 |
| 相対パスの `.md` リンク | クリック時、本ビューアで開き直す（=現在のファイルを切り替える）。履歴 stack は持たない。 |
| 相対パスの画像 | `base_url` 経由でローカル読み込み。動く。 |
| 絶対パス / `file://` 画像 | 表示する。 |
| `javascript:` 等の他スキーム | 開かない（無視）。 |

外部 URL の自動取得（OGP プレビュー等）は行わない。

## 9. エラー処理

| シナリオ | 表示 |
| --- | --- |
| CLI で指定したファイルが存在しない | 標準エラーに 1 行出して exit 1。GUI は起動しない。 |
| GUI で開いたファイルが UTF-8 として読めない | エラーダイアログ「Not a valid UTF-8 text file」。 |
| パース中の cmark-gfm エラー | 起こり得ない設計だが、起きたらエラーダイアログを出して空ページ。 |
| ファイル読み込み中に削除された | ステータスバーに「File missing」、本文は最後にレンダリングした内容を残す。 |

## 10. ディレクトリ構成（想定）

```
mark-preview/
├── CMakeLists.txt
├── SPEC.md                  ← 本書
├── README.md
├── LICENSE                  ← MIT
├── src/
│   ├── main.cpp             ← wxApp / コマンドライン解析
│   ├── MainFrame.{h,cpp}    ← ウィンドウ・メニュー・D&D
│   ├── Renderer.{h,cpp}     ← cmark-gfm 呼び出し & HTML テンプレ合成
│   ├── Watcher.{h,cpp}      ← wxFileSystemWatcher + デバウンス
│   ├── Theme.{h,cpp}        ← GTK のダーク判定
│   └── Resources.{h,cpp}    ← 埋め込みアセットのアクセサ
├── assets/
│   ├── template.html        ← レンダリング用テンプレ
│   ├── github-light.css
│   ├── github-dark.css
│   ├── katex/               ← KaTeX 一式（CSS, JS, fonts）
│   ├── mermaid/             ← Mermaid.min.js
│   └── highlight/           ← highlight.min.js + 言語パック + テーマ CSS
└── third_party/
    └── cmark-gfm/           ← サブモジュール or システム link（CMake で選択）
```

アセットは CMake で C++ ヘッダに埋め込み（`xxd -i` 相当）、ランタイムでパス解決を不要にする。配布バイナリ 1 本で完結させる。

## 11. ビルドと配布

### 11.1 ビルド（共通）
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
CMake は `find_package(wxWidgets REQUIRED COMPONENTS core base webview)` と `find_package(cmark-gfm)` で依存を解決する。アセット（KaTeX/Mermaid/highlight.js/CSS）は `target_embed_assets()` 相当の自前マクロで生成ヘッダに焼き込む（プラットフォーム非依存）。

### 11.2 依存パッケージ
| 環境 | コマンド |
| --- | --- |
| Arch (Omarchy) | `sudo pacman -S wxwidgets-gtk3 webkit2gtk cmark-gfm cmake pkgconf` |
| macOS (Homebrew) | `brew install wxwidgets cmark-gfm cmake` （WebKit は OS 同梱）|
| Windows | vcpkg で `wxwidgets[webview]` と `cmark-gfm` を入れる。WebView2 ランタイムは Win11 標準同梱、Win10 は要インストール |

### 11.3 配布
- 初版は GitHub にソース公開のみ。
- Linux: 将来 AUR (`mark-preview-git`) を出す。
- macOS: 将来 `.app` バンドル + 公証 + Homebrew Cask 配布を想定。
- Windows: 将来 zip ポータブル配布 + WebView2 Runtime 同梱を想定。
- Flatpak / AppImage / MSIX は当面作らない。

### 11.4 CI（将来）
- GitHub Actions の `ubuntu-latest` / `macos-latest` / `windows-latest` の 3 マトリクスでビルドが通ることを継続的に確認する。初版から空の CI 設定ファイルだけ用意し、Linux ジョブだけ enable して始める。

## 12. ライセンス
- 本体: MIT。
- 同梱物: KaTeX (MIT)、Mermaid (MIT)、highlight.js (BSD-3-Clause)、github-markdown-css (MIT)、cmark-gfm (BSD-2-Clause)。
- すべて MIT / BSD 互換のため、NOTICE に出典と原ライセンス全文を列挙して再配布する。

## 13. マイルストーン

| Phase | ゴール |
| --- | --- |
| M0 | wxWidgets で空ウィンドウ → wxWebView で固定 HTML を表示する Hello World。 |
| M1 | CLI 引数で `.md` を受け取り、cmark-gfm で素の GFM を HTML 化して表示。テーマは GitHub Light のみ。 |
| M2 | File > Open / Drag & Drop / Reload / Quit を実装。ステータスバー表示。 |
| M3 | wxFileSystemWatcher + デバウンス。スクロール位置復元。 |
| M4 | KaTeX / Mermaid / highlight.js を組み込み。ライト/ダーク同梱とシステム追従。 |
| M5 | リンククリックのハンドリング（外部=xdg-open、内部=開き直し）。 |
| M6 | エラー処理整備・README 整備・初版リリース。 |
| M7 (将来) | macOS / Windows のビルド通過と最低限の動作確認。CI 3 マトリクス有効化。 |

## 14. クロスプラットフォーム実装ガイドライン

初版は Linux しか動かさないが、以下を **最初から守って書く**ことで将来の移植コストを下げる。

### 14.1 やる
- ファイルパスは `std::filesystem::path` または `wxFileName` を経由する。文字列連結で `/` を付けない。
- ファイル I/O は `wxFile` / `wxFFile` を使う（Windows のワイド文字パスを自動処理）。生の `fopen` / `std::ifstream(const char*)` は使わない。
- 起動時の引数解析は `wxApp::OnCmdLineParsed` を使う（Windows の `wmain` 相当も wxWidgets が抽象化）。
- アセット埋め込みは CMake で生成したヘッダから取り出す。`/usr/share/mark-preview/` のような Linux 固有の実行時パス参照はしない。
- リソースは UTF-8 で書き、ソースコードは BOM なし UTF-8 で保存する。
- 外部プロセス起動が必要になったら `wxExecute` か `wxLaunchDefaultBrowser` を使う。`system("xdg-open …")` は禁止。
- 改行コード差異はパース側（cmark-gfm）が CRLF / LF どちらも処理するため、こちらでは正規化しない。

### 14.2 やらない
- `#include <unistd.h>` / `<sys/inotify.h>` などの POSIX 専用ヘッダ。
- `fork()` / `execvp()` / `pipe()` / `signal()`。子プロセスは `wxExecute` のみ。
- `getenv("HOME")`。設定ファイル置き場は `wxStandardPaths::Get().GetUserConfigDir()`。
- `dlopen` 系の動的ロード。
- `__attribute__((…))` や GCC 固有拡張。プラットフォーム差は `#ifdef __WXMAC__` 等の wxWidgets マクロで分ける。

### 14.3 グレーゾーン（実装時に判断）
- フォント指定: ライト/ダーク CSS では `system-ui` と汎用ファミリだけにする。`SF Pro` / `Segoe UI` などのプラットフォーム個別フォントを直接指定しない。
- 数式・コードの等幅フォント: `ui-monospace, "SFMono-Regular", "Cascadia Mono", "Consolas", "DejaVu Sans Mono", monospace` の順でフォールバック。
- ファイル監視: `wxFileSystemWatcher` は全 OS で動くが、エディタの atomic save 検知挙動は OS で差がある。デバウンス + `RENAME` イベント拾いを徹底すれば吸収できる範囲とする。
- HiDPI: wxWidgets 3.2 はデフォルトで対応するが、CMake で `wxUSE_DPI_AWARE_MANIFEST=per_monitor` 相当を有効化する。Windows 用のマニフェスト指定が将来必要。
