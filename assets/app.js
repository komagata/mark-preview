(function () {
  var cfg = window.__MARK_PREVIEW__ || { theme: 'light', scrollY: 0 };

  // Convert <pre><code class="language-mermaid"> to <div class="mermaid">.
  var mermaidCodes = document.querySelectorAll('pre code.language-mermaid, pre code.lang-mermaid');
  mermaidCodes.forEach(function (code) {
    var div = document.createElement('div');
    div.className = 'mermaid';
    div.textContent = code.textContent;
    var pre = code.parentNode;
    pre.parentNode.replaceChild(div, pre);
  });

  // Syntax-highlight remaining code blocks.
  if (window.hljs) {
    document.querySelectorAll('pre code').forEach(function (block) {
      try { window.hljs.highlightElement(block); } catch (e) { /* ignore */ }
    });
  }

  // Mermaid.
  if (window.mermaid) {
    try {
      window.mermaid.initialize({
        startOnLoad: false,
        theme: cfg.theme === 'dark' ? 'dark' : 'default',
        securityLevel: 'loose'
      });
      window.mermaid.run();
    } catch (e) { /* ignore */ }
  }

  // KaTeX auto-render.
  if (window.renderMathInElement) {
    try {
      window.renderMathInElement(document.body, {
        delimiters: [
          { left: '$$', right: '$$', display: true },
          { left: '$', right: '$', display: false },
          { left: '\\(', right: '\\)', display: false },
          { left: '\\[', right: '\\]', display: true }
        ],
        throwOnError: false
      });
    } catch (e) { /* ignore */ }
  }

  // Restore scroll position passed from host on reload.
  if (typeof cfg.scrollY === 'number' && cfg.scrollY > 0) {
    window.requestAnimationFrame(function () {
      window.scrollTo(0, cfg.scrollY);
    });
  }

  // Belt-and-braces wheel handler. WebKit2GTK under XWayland sometimes fails
  // to scroll the document on its own; explicitly call scrollBy here so the
  // viewer scrolls regardless of native behavior. preventDefault suppresses
  // the duplicate scroll if WebKit was going to handle it too.
  window.addEventListener('wheel', function (e) {
    if (e.ctrlKey) return; // let zoom pass through
    var dy = e.deltaY;
    var dx = e.deltaX;
    if (e.deltaMode === 1) { dy *= 16; dx *= 16; } // LINE
    else if (e.deltaMode === 2) {                  // PAGE
      dy *= window.innerHeight;
      dx *= window.innerWidth;
    }
    window.scrollBy(dx, dy);
    e.preventDefault();
  }, { passive: false });
})();
