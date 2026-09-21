"use strict";

// Renders bridge.markdown into #content. The bridge is the C++ PreviewBridge,
// reached over a QWebChannel; its properties arrive with every change.
(function () {
    let bridge = null;

    const katexOptions = {
        throwOnError: false,
        // Warnings about non-standard LaTeX would only fill the console.
        strict: "ignore",
    };

    // texmath lets inline $…$ run across other dollar signs, so in
    // "costs $5 and $10, or $x$" it swallows "5 and $10, or $x" as one
    // (broken) formula. Unescaped dollars cannot occur inside inline math, so
    // stop the content at the first one; \$ and other escapes still pass.
    const inlineDollar = window.texmath.rules.dollars.inline
        .find((rule) => rule.name === "math_inline");
    const mathChar = String.raw`(?:[^\s\\$]|\\[\s\S])`;
    inlineDollar.rex = new RegExp(String.raw`\$(${mathChar}|${mathChar}(?:[^$\\]|\\[\s\S])*?${mathChar})\$`, "gy");

    const md = window.markdownit({
        // Raw HTML is always shown as text (spec 4.4).
        html: false,
        linkify: true,
        typographer: false,
    });
    md.use(window.texmath, {
        engine: window.katex,
        delimiters: ["dollars", "brackets"],
        katexOptions: katexOptions,
    });

    // texmath falls back to plain text when KaTeX throws despite
    // throwOnError: false; give that path the same look as a parse error.
    const escapeHtml = (text) => md.utils.escapeHtml(text);
    window.texmath.render = function (tex, displayMode, options) {
        options.displayMode = displayMode;
        try {
            return window.katex.renderToString(tex, options);
        } catch (error) {
            return '<span class="katex-error" title="' + escapeHtml(String(error.message))
                + '">' + escapeHtml(tex) + "</span>";
        }
    };

    // Tag every top-level block with the first source line it came from, for
    // scroll sync (M2). Renderers that honour token attributes pick it up.
    md.core.ruler.push("source_lines", function (state) {
        for (const token of state.tokens) {
            if (token.level === 0 && token.map && token.nesting >= 0)
                token.attrSet("data-source-line", String(token.map[0]));
        }
    });
    // texmath's block templates ignore attributes, so add the line by hand.
    for (const name of ["math_block", "math_block_eqno"]) {
        const renderMath = md.renderer.rules[name];
        md.renderer.rules[name] = function (tokens, index, options, env, self) {
            const html = renderMath(tokens, index, options, env, self);
            const line = tokens[index].attrGet("data-source-line");
            return line === null
                ? html
                : html.replace(/^<section/, '<section data-source-line="' + line + '"');
        };
    }

    // GFM task lists: a list item whose text starts with "[ ] " or "[x] ".
    md.core.ruler.after("inline", "task_lists", function (state) {
        const tokens = state.tokens;
        for (let i = 2; i < tokens.length; i++) {
            const inline = tokens[i];
            if (inline.type !== "inline" || tokens[i - 1].type !== "paragraph_open"
                    || tokens[i - 2].type !== "list_item_open")
                continue;
            const match = /^\[([ xX])\][ \t]/.exec(inline.content);
            const first = inline.children[0];
            if (!match || !first || first.type !== "text" || !first.content.startsWith(match[0]))
                continue;
            first.content = first.content.slice(match[0].length);
            const checkbox = new state.Token("html_inline", "", 0);
            checkbox.content = '<input type="checkbox" disabled'
                + (match[1] === " " ? "" : " checked") + ">";
            inline.children.unshift(checkbox);
            tokens[i - 2].attrJoin("class", "task-list-item");
        }
    });

    // Relative image paths resolve against the document's folder. With no
    // folder (an unsaved document) they resolve to nothing; the request
    // interceptor refuses anything outside the folder either way.
    const renderImage = md.renderer.rules.image;
    md.renderer.rules.image = function (tokens, index, options, env, self) {
        const token = tokens[index];
        let source = "";
        try {
            source = new URL(token.attrGet("src"), env.baseUrl || undefined).href;
        } catch (error) {
            source = "";
        }
        token.attrSet("src", source);
        return renderImage(tokens, index, options, env, self);
    };

    const content = () => document.getElementById("content");

    function render() {
        // M1 replaces the whole document and keeps the scroll offset;
        // block-keyed patching arrives in M2.
        const scroller = document.scrollingElement;
        const scrollTop = scroller.scrollTop;
        content().innerHTML = md.render(bridge.markdown, { baseUrl: bridge.baseUrl });
        scroller.scrollTop = scrollTop;
    }

    function applyTheme() {
        const theme = bridge.theme || {};
        const style = document.documentElement.style;
        for (const key of ["bg", "fg", "accent", "selection", "muted"]) {
            if (theme[key])
                style.setProperty("--" + key, theme[key]);
        }
        style.colorScheme = theme.dark === false ? "light" : "dark";
    }

    function applyTextScale() {
        const scale = bridge.textScale > 0 ? bridge.textScale : 1;
        document.documentElement.style.setProperty("--base-size", 20 * scale + "px");
    }

    function applyFontFamily() {
        document.documentElement.dataset.font = bridge.fontFamily || "mono";
    }

    // The page never navigates: in-page anchors scroll, everything else is
    // handed to the bridge, which opens only http(s) and mailto externally.
    function handleLinkClick(event) {
        const link = event.target.closest && event.target.closest("a[href]");
        if (!link)
            return;
        event.preventDefault();
        const href = link.getAttribute("href");
        if (href.startsWith("#")) {
            const target = document.getElementById(decodeURIComponent(href.slice(1)));
            if (target)
                target.scrollIntoView();
            return;
        }
        if (event.type === "click" && bridge)
            bridge.openLink(link.href);
    }

    document.addEventListener("click", handleLinkClick);
    document.addEventListener("auxclick", handleLinkClick);

    function connect() {
        new QWebChannel(qt.webChannelTransport, function (channel) {
            bridge = channel.objects.bridge;
            applyTheme();
            applyTextScale();
            applyFontFamily();
            render();
            bridge.markdownChanged.connect(render);
            bridge.baseUrlChanged.connect(render);
            bridge.themeChanged.connect(applyTheme);
            bridge.textScaleChanged.connect(applyTextScale);
            bridge.fontFamilyChanged.connect(applyFontFamily);
            bridge.ready();
        });
    }

    if (document.readyState === "loading")
        document.addEventListener("DOMContentLoaded", connect);
    else
        connect();
})();
