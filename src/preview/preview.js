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
    function typeset(tex, displayMode, options) {
        options.displayMode = displayMode;
        try {
            return window.katex.renderToString(tex, options);
        } catch (error) {
            return '<span class="katex-error" title="' + escapeHtml(String(error.message))
                + '">' + escapeHtml(tex) + "</span>";
        }
    }

    // KaTeX is most of the cost of a render, and nearly every formula is the
    // same as last time. Keep what the previous render typeset; anything not
    // used for a whole render is dropped, so the cache stays the size of the
    // document while an expression is being typed.
    const mathCache = { current: new Map(), previous: new Map() };
    window.texmath.render = function (tex, displayMode, options) {
        const key = (displayMode ? "D" : "I") + tex;
        let html = mathCache.current.get(key);
        if (html === undefined)
            html = mathCache.previous.get(key);
        if (html === undefined)
            html = typeset(tex, displayMode, options);
        mathCache.current.set(key, html);
        return html;
    };

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

    // Headings get GitHub-style ids, so [link](#some-heading) scrolls to
    // them: the text lower-cased, punctuation dropped, spaces as hyphens,
    // and -1, -2… on repeats.
    md.core.ruler.push("heading_ids", function (state) {
        const used = new Map();
        const tokens = state.tokens;
        for (let i = 0; i < tokens.length - 1; i++) {
            if (tokens[i].type !== "heading_open")
                continue;
            const text = (tokens[i + 1].children || [])
                .filter((child) => ["text", "code_inline", "math_inline"].includes(child.type))
                .map((child) => child.content)
                .join("");
            const base = text.trim().toLowerCase()
                .replace(/[^\p{L}\p{N}\s_-]/gu, "")
                .replace(/\s/g, "-");
            const count = used.get(base) || 0;
            used.set(base, count + 1);
            tokens[i].attrSet("id", count ? base + "-" + count : base);
        }
    });

    // An image inside the document's folder (a relative path, or a file:
    // URL) is served through omaview-doc:, which maps only to that folder.
    // Returns a reason instead when the image is not shown.
    function documentImage(source, baseUrl) {
        if (!baseUrl)
            return { reason: "Save the document to show its images" };
        let url;
        try {
            url = new URL(source, baseUrl);
        } catch (error) {
            return { reason: "Not an image path" };
        }
        if (url.protocol !== "file:")
            return { reason: "Remote images are not shown" };
        // The URL parser has already resolved any `..`; the scheme handler
        // checks again, symlinks included.
        const folder = new URL(baseUrl).pathname;
        if (url.host !== "" || !url.pathname.startsWith(folder) || url.pathname === folder)
            return { reason: "Only images in the document's folder are shown" };
        return { src: "omaview-doc:/" + url.pathname.slice(folder.length) };
    }

    // markdown-it drops file: URLs outright, but spec 4.4 shows a file:
    // image inside the folder. Let them through: images go through the
    // rule below, and the bridge refuses file: links when clicked.
    const validateLink = md.validateLink;
    md.validateLink = (url) => /^file:/i.test(url.trim()) || validateLink(url);

    // Anything else is a small placeholder with the alt text (spec 4.4).
    const renderImage = md.renderer.rules.image;
    md.renderer.rules.image = function (tokens, index, options, env, self) {
        const token = tokens[index];
        const image = documentImage(token.attrGet("src"), env.baseUrl);
        if (image.src) {
            token.attrSet("src", image.src);
            return renderImage(tokens, index, options, env, self);
        }
        const alt = self.renderInlineAsText(token.children, options, env) || "image";
        return '<span class="image-placeholder" role="img" title="' + escapeHtml(image.reason)
            + '" aria-label="' + escapeHtml(alt) + '">' + escapeHtml(alt) + "</span>";
    };

    const content = () => document.getElementById("content");

    // Splits the token stream into top-level blocks, each with its source
    // lines and its HTML. A block is everything from a level-0 token to the
    // token that closes it; fences, math blocks and rules are one token.
    function renderBlocks(markdown, env) {
        const tokens = md.parse(markdown, env);
        const blocks = [];
        let start = 0;
        let depth = 0;
        for (let i = 0; i < tokens.length; i++) {
            depth += tokens[i].nesting;
            if (depth !== 0)
                continue;
            const blockTokens = tokens.slice(start, i + 1);
            const map = tokens[start].map || [0, 0];
            blocks.push({
                html: md.renderer.render(blockTokens, md.options, env),
                line: map[0],
                lineEnd: map[1],
            });
            start = i + 1;
        }
        return blocks;
    }

    // Parses the HTML of every new block in one go. The comment before each
    // block marks where it starts: raw HTML is escaped (html: false), so no
    // block can contain a comment of its own. A block that renders to other
    // than exactly one element is wrapped, so each block is one node.
    function createNodes(blocks) {
        const template = document.createElement("template");
        template.innerHTML = blocks.map((block) => "<!--block-->" + block.html).join("");
        const nodes = [];
        let parts = null;
        const finish = () => {
            if (parts === null)
                return;
            const elements = parts.filter((node) => node.nodeType === Node.ELEMENT_NODE);
            let node = elements[0];
            if (elements.length !== 1) {
                node = document.createElement("div");
                node.append(...parts);
            }
            nodes.push(node);
        };
        for (const node of [...template.content.childNodes]) {
            if (node.nodeType === Node.COMMENT_NODE) {
                finish();
                parts = [];
            } else {
                parts.push(node);
            }
        }
        finish();
        return nodes;
    }

    // Patches #content so it shows the new blocks. A block whose HTML is
    // unchanged keeps its DOM node, typeset math and all; the key leaves out
    // the source lines, which are set on the nodes afterwards, so typing
    // above a block doesn't count as changing it.
    function patch(container, blocks) {
        const unused = new Map();
        for (const node of container.children) {
            const queue = unused.get(node.omaviewKey);
            if (queue)
                queue.push(node);
            else
                unused.set(node.omaviewKey, [node]);
        }

        const nodes = blocks.map((block) => {
            const queue = unused.get(block.html);
            return queue && queue.length ? queue.shift() : null;
        });
        const fresh = blocks.filter((block, i) => nodes[i] === null);
        const created = createNodes(fresh);
        for (let i = 0, next = 0; i < nodes.length; i++) {
            if (nodes[i] === null) {
                nodes[i] = created[next++];
                nodes[i].omaviewKey = blocks[i].html;
            }
        }

        // Drop what went away first, so the walk below only ever inserts
        // the new blocks and never shuffles the ones that stay.
        for (const queue of unused.values()) {
            for (const node of queue)
                node.remove();
        }
        let cursor = container.firstChild;
        for (const node of nodes) {
            if (node === cursor)
                cursor = cursor.nextSibling;
            else
                container.insertBefore(node, cursor);
        }

        blocks.forEach((block, i) => {
            const data = nodes[i].dataset;
            if (data.sourceLine !== String(block.line))
                data.sourceLine = block.line;
            if (data.sourceLineEnd !== String(block.lineEnd))
                data.sourceLineEnd = block.lineEnd;
        });
        return { blocks: blocks.length, created: fresh.length };
    }

    // Timing and counts of the last render, for the tests and the
    // performance budget in spec 5.4.
    const stats = { lastRender: null, renders: 0 };
    window.omaviewPreview = stats;

    // Scroll sync (editor → preview). The page-y of a fractional source
    // line: inside a block, the same fraction of the way through its lines;
    // between blocks (blank lines), the same fraction of the gap.
    function positionOfLine(line) {
        const nodes = content().children;
        if (!nodes.length)
            return 0;
        // The last block that starts at or before the line.
        let low = 0;
        let high = nodes.length - 1;
        while (low < high) {
            const middle = (low + high + 1) >> 1;
            if (Number(nodes[middle].dataset.sourceLine) <= line)
                low = middle;
            else
                high = middle - 1;
        }
        const node = nodes[low];
        const start = Number(node.dataset.sourceLine);
        const end = Number(node.dataset.sourceLineEnd);
        const box = node.getBoundingClientRect();
        const offset = document.scrollingElement.scrollTop;
        if (line < start)
            return 0;
        if (line < end)
            return offset + box.top + (line - start) / Math.max(1, end - start) * box.height;
        const next = nodes[low + 1];
        if (!next)
            return offset + box.bottom;
        const nextStart = Number(next.dataset.sourceLine);
        const nextTop = next.getBoundingClientRect().top;
        const gap = (line - end) / Math.max(1, nextStart - end);
        return offset + box.bottom + Math.min(1, gap) * (nextTop - box.bottom);
    }

    // The preview follows the editor until the reader scrolls the preview
    // themselves, and again as soon as the editor scrolls. While following,
    // anything that moves the layout (a render, a resize, fonts arriving)
    // re-applies the sync; otherwise the page stays where the reader put it.
    let following = true;

    function applySourceLine() {
        const margin = parseFloat(getComputedStyle(content()).paddingTop) || 0;
        document.scrollingElement.scrollTop =
            Math.max(0, positionOfLine(bridge.sourceLine || 0) - margin);
    }

    function followEditor() {
        following = true;
        applySourceLine();
    }

    function resync() {
        if (following && bridge)
            applySourceLine();
    }

    const stopFollowing = () => { following = false; };
    const scrollKeys = ["ArrowUp", "ArrowDown", "PageUp", "PageDown", "Home", "End", " "];
    document.addEventListener("wheel", stopFollowing, { passive: true });
    document.addEventListener("touchstart", stopFollowing, { passive: true });
    document.addEventListener("pointerdown", stopFollowing);
    document.addEventListener("keydown", (event) => {
        if (scrollKeys.includes(event.key))
            stopFollowing();
    });
    window.addEventListener("resize", resync);
    document.fonts.addEventListener("loadingdone", resync);

    function render() {
        const started = performance.now();
        const scroller = document.scrollingElement;
        const scrollTop = scroller.scrollTop;
        mathCache.previous = mathCache.current;
        mathCache.current = new Map();

        const container = content();
        const blocks = renderBlocks(bridge.markdown, { baseUrl: bridge.baseUrl });
        const result = patch(container, blocks);
        if (following)
            applySourceLine();
        else
            scroller.scrollTop = scrollTop;
        // Reading layout here makes the timing include it, as the budget does.
        void container.offsetHeight;
        result.ms = performance.now() - started;
        stats.lastRender = result;
        stats.renders++;
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
            if (target) {
                // The reader moved the preview; don't pull it back.
                stopFollowing();
                target.scrollIntoView();
            }
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
            bridge.sourceLineChanged.connect(followEditor);
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
