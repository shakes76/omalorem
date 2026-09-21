#include "markdownhighlighter.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QTextDocument>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document) {
    rebuildFormats();
}

void MarkdownHighlighter::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    rebuildFormats();
    rehighlight();
}

void MarkdownHighlighter::setColors(const QString &background, const QString &foreground,
                                    const QString &accent) {
    if (m_customBackground == background && m_customForeground == foreground
            && m_customAccent == accent)
        return;

    m_customBackground = background;
    m_customForeground = foreground;
    m_customAccent = accent;
    rebuildFormats();
    rehighlight();
}

void MarkdownHighlighter::setSearch(const QString &query, int currentMatchStart) {
    if (m_searchQuery == query && m_currentMatchStart == currentMatchStart)
        return;
    m_searchQuery = query;
    m_currentMatchStart = currentMatchStart;
    rehighlight();
}

void MarkdownHighlighter::rebuildFormats() {
    const QColor marker = m_darkMode ? QColor(QStringLiteral("#4f525a"))
                                     : QColor(QStringLiteral("#aeb1b5"));
    const QColor background = !m_customBackground.isEmpty() ? QColor(m_customBackground)
        : (m_darkMode ? QColor(QStringLiteral("#101010")) : QColor(QStringLiteral("#ffffff")));
    const QColor text = !m_customForeground.isEmpty() ? QColor(m_customForeground)
        : (m_darkMode ? QColor(QStringLiteral("#eeeeee")) : QColor(QStringLiteral("#222324")));
    const QColor link = !m_customAccent.isEmpty() ? QColor(m_customAccent)
        : (m_darkMode ? QColor(QStringLiteral("#5584aa")) : QColor(QStringLiteral("#2077b2")));
    const QColor quote = marker;
    const QColor codeBackground = m_darkMode ? QColor(QStringLiteral("#1c1a1a"))
                                             : QColor(QStringLiteral("#f8f8f8"));

    m_markerFormat = QTextCharFormat();
    m_markerFormat.setForeground(marker);

    // A sub-pixel font size combined with a stretch factor used to make these
    // markers occupy (close to) zero space, but that combination deadlocks Qt's
    // font metrics engine on some platforms. Instead, use a normal font size and
    // cancel out its advance width with negative absolute letter-spacing.
    m_hiddenMarkerFormat = QTextCharFormat();
    m_hiddenMarkerFormat.setForeground(background);
    m_hiddenMarkerFormat.setFontPointSize(1.0);

    QFont hiddenFont = document() ? document()->defaultFont() : QFont();
    hiddenFont.setPointSizeF(1.0);
    const qreal charWidth = QFontMetricsF(hiddenFont).horizontalAdvance(QLatin1Char('['));

    m_hiddenMarkerFormat.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    m_hiddenMarkerFormat.setFontLetterSpacing(-charWidth);

    m_headingFormat = QTextCharFormat();
    m_headingFormat.setForeground(text);
    m_headingFormat.setFontWeight(QFont::Bold);

    m_boldFormat = QTextCharFormat();
    m_boldFormat.setFontWeight(QFont::Bold);
    m_boldFormat.setForeground(text);

    m_italicFormat = QTextCharFormat();
    m_italicFormat.setFontItalic(true);
    m_italicFormat.setForeground(text);

    m_codeFormat = QTextCharFormat();
    m_codeFormat.setForeground(text);
    m_codeFormat.setBackground(codeBackground);

    m_quoteFormat = QTextCharFormat();
    m_quoteFormat.setForeground(quote);
    m_quoteFormat.setFontItalic(true);

    m_linkFormat = QTextCharFormat();
    m_linkFormat.setForeground(link);
    m_linkFormat.setFontUnderline(true);

    // Math in the accent colour at reduced opacity, so formulas stand out
    // from prose without competing with links; delimiters are muted markers.
    QColor math = link;
    math.setAlphaF(0.8);
    m_mathFormat = QTextCharFormat();
    m_mathFormat.setForeground(math);

    m_searchFormat = QTextCharFormat();
    m_searchFormat.setBackground(m_darkMode ? QColor(QStringLiteral("#725b18"))
                                            : QColor(QStringLiteral("#ffe58a")));
    m_currentSearchFormat = QTextCharFormat();
    m_currentSearchFormat.setBackground(m_darkMode ? QColor(QStringLiteral("#b36b20"))
                                                   : QColor(QStringLiteral("#ffad42")));
}

void MarkdownHighlighter::highlightBlock(const QString &text) {
    int mathState = MathNormal;
    m_blockMath = mathSpans(text, previousBlockState(), &mathState);
    setCurrentBlockState(mathState);
    if (!text.isEmpty()) {
        highlightMarkers(text);
        if (text.contains(QLatin1Char('`')) || text.contains(QLatin1Char('*'))
            || text.contains(QLatin1Char('_')) || text.contains(QLatin1Char('['))) {
            highlightInline(text);
        }
    }
    highlightMath();
    highlightSearch(text);
}

void MarkdownHighlighter::highlightSearch(const QString &text) {
    if (m_searchQuery.isEmpty())
        return;

    int from = 0;
    while ((from = text.indexOf(m_searchQuery, from, Qt::CaseInsensitive)) >= 0) {
        const int documentStart = currentBlock().position() + from;
        QTextCharFormat format = this->format(from);
        format.setBackground(documentStart == m_currentMatchStart
                                 ? m_currentSearchFormat.background()
                                 : m_searchFormat.background());
        setFormat(from, m_searchQuery.length(), format);
        from += qMax(1, m_searchQuery.length());
    }
}

void MarkdownHighlighter::highlightMarkers(const QString &text) {
    int first = 0;
    while (first < text.length() && text.at(first).isSpace())
        ++first;
    if (first >= text.length())
        return;

    const QChar firstChar = text.at(first);
    if (first == 0 && firstChar == QLatin1Char('#')) {
        static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})(\\s+)(.*)$"));
        const QRegularExpressionMatch heading = headingRe.match(text);
        if (heading.hasMatch()) {
            setFormat(0, heading.capturedLength(1) + heading.capturedLength(2),
                      m_markerFormat);
            setFormat(heading.capturedStart(3), heading.capturedLength(3),
                      m_headingFormat);
            return;
        }
    }

    if (firstChar == QLatin1Char('>')) {
        static const QRegularExpression quoteRe(QStringLiteral("^(\\s*>+\\s?)(.*)$"));
        const QRegularExpressionMatch quote = quoteRe.match(text);
        if (quote.hasMatch()) {
            setFormat(0, quote.capturedLength(1), m_markerFormat);
            setFormat(quote.capturedStart(2), quote.capturedLength(2), m_quoteFormat);
        }
    }

    if (firstChar == QLatin1Char('-') || firstChar == QLatin1Char('+')
            || firstChar == QLatin1Char('*') || firstChar.isDigit()) {
        static const QRegularExpression listRe(
            QStringLiteral("^(\\s*(?:[-+*]|\\d+[.)])\\s+)(.*)$"));
        const QRegularExpressionMatch list = listRe.match(text);
        if (list.hasMatch())
            setFormat(0, list.capturedLength(1), m_markerFormat);
    }

    if (firstChar == QLatin1Char('-') || firstChar == QLatin1Char('*')
            || firstChar == QLatin1Char('_')) {
        static const QRegularExpression ruleRe(QStringLiteral("^\\s{0,3}([-*_])(?:\\s*\\1){2,}\\s*$"));
        const QRegularExpressionMatch rule = ruleRe.match(text);
        if (rule.hasMatch())
            setFormat(0, text.length(), m_markerFormat);
    }
}

void MarkdownHighlighter::highlightInline(const QString &text) {
    if (text.contains(QLatin1Char('`'))) {
        static const QRegularExpression codeRe(QStringLiteral("`([^`]+)`"));
        QRegularExpressionMatchIterator codeMatches = codeRe.globalMatch(text);
        while (codeMatches.hasNext()) {
            const QRegularExpressionMatch match = codeMatches.next();
            setFormat(match.capturedStart(0), match.capturedLength(0), m_codeFormat);
        }
    }

    const QList<InlineMarkup> markup = inlineMarkup(text);
    for (const InlineMarkup &item : markup) {
        if (touchesMath(item, m_blockMath))
            continue;
        const QTextCharFormat &contentFormat =
            item.kind == InlineKind::Bold ? m_boldFormat
            : item.kind == InlineKind::Italic ? m_italicFormat
                                              : m_linkFormat;
        setFormat(item.content.start, item.content.length, contentFormat);
        for (const Span &marker : item.markers)
            setFormat(marker.start, marker.length, m_hiddenMarkerFormat);
    }
}

QList<MarkdownHighlighter::InlineMarkup> MarkdownHighlighter::inlineMarkup(const QString &text) {
    QList<InlineMarkup> markup;
    if (!text.contains(QLatin1Char('*')) && !text.contains(QLatin1Char('_'))
            && !text.contains(QLatin1Char('['))) {
        return markup;
    }

    const auto span = [](const QRegularExpressionMatch &match, int group) {
        return Span{int(match.capturedStart(group)), int(match.capturedLength(group))};
    };

    static const QRegularExpression boldRe(QStringLiteral("(\\*\\*|__)(.+?)(\\1)"));
    QRegularExpressionMatchIterator boldMatches = boldRe.globalMatch(text);
    while (boldMatches.hasNext()) {
        const QRegularExpressionMatch match = boldMatches.next();
        markup.append({InlineKind::Bold, span(match, 2),
                       {span(match, 1), span(match, 3)}});
    }

    static const QRegularExpression italicRe(
        QStringLiteral("(?<!\\*)\\*([^*\\n]+)\\*(?!\\*)|(?<!_)_([^_\\n]+)_(?!_)"));
    QRegularExpressionMatchIterator italicMatches = italicRe.globalMatch(text);
    while (italicMatches.hasNext()) {
        const QRegularExpressionMatch match = italicMatches.next();
        const Span whole = span(match, 0);
        const int contentIndex = match.capturedStart(1) >= 0 ? 1 : 2;
        markup.append({InlineKind::Italic, span(match, contentIndex),
                       {{whole.start, 1}, {whole.start + whole.length - 1, 1}}});
    }

    static const QRegularExpression linkRe(
        QStringLiteral("\\[([^\\]]+)\\]\\(((?:\\\\.|[^)])+)\\)"));
    QRegularExpressionMatchIterator linkMatches = linkRe.globalMatch(text);
    while (linkMatches.hasNext()) {
        const QRegularExpressionMatch match = linkMatches.next();
        const Span whole = span(match, 0);
        const Span content = span(match, 1);
        const int contentEnd = content.start + content.length;
        markup.append({InlineKind::Link, content,
                       {{whole.start, 1},
                        {contentEnd, whole.start + whole.length - contentEnd}}});
    }

    return markup;
}

// --- Omalorem editor math (docs/SPEC.md §5.6) -------------------------------

void MarkdownHighlighter::highlightMath() {
    for (const MathSpan &span : std::as_const(m_blockMath)) {
        setFormat(span.content.start, span.content.length, m_mathFormat);
        for (const Span &marker : span.markers)
            setFormat(marker.start, marker.length, m_markerFormat);
    }
}

bool MarkdownHighlighter::touchesMath(const InlineMarkup &item, const QList<MathSpan> &math) {
    for (const MathSpan &span : math) {
        const int start = span.markers[0].length ? span.markers[0].start : span.content.start;
        const int end = span.markers[1].length ? span.markers[1].start + span.markers[1].length
                                               : span.content.start + span.content.length;
        for (const Span &marker : item.markers) {
            if (marker.start < end && marker.start + marker.length > start)
                return true;
        }
    }
    return false;
}

namespace {

bool isDigit(const QString &text, int index) {
    return index >= 0 && index < text.length() && text.at(index).isDigit();
}

// The first `needle` at or after `from` that isn't escaped by a backslash.
// Views, not mid(): this runs for every math line on every rehighlight.
int findUnescaped(QStringView text, QStringView needle, int from) {
    for (int i = from; i + needle.length() <= text.length(); ++i) {
        if (text.at(i) == QLatin1Char('\\')) {
            if (needle.startsWith(QLatin1Char('\\')) && text.sliced(i, needle.length()) == needle)
                return i;
            ++i;
            continue;
        }
        if (text.sliced(i, needle.length()) == needle)
            return i;
    }
    return -1;
}

} // namespace

QList<MarkdownHighlighter::MathSpan> MarkdownHighlighter::mathSpans(const QString &text,
                                                                    int previousState,
                                                                    int *state) {
    QList<MathSpan> spans;
    int current = previousState < 0 ? MathNormal : previousState;
    const auto add = [&spans](int start, int length, Span opening, Span closing) {
        spans.append(MathSpan{{start, length}, {opening, closing}});
    };
    const auto finish = [&]() {
        if (state)
            *state = current;
        return spans;
    };

    // Inside fenced code nothing is math; the fence closes on a line of the
    // same character only.
    static const QRegularExpression fenceRe(QStringLiteral("^ {0,3}(`{3,}|~{3,})"));
    const QRegularExpressionMatch fence = fenceRe.match(text);
    if (current & (MathBacktickFence | MathTildeFence)) {
        const QChar fenceChar = current & MathBacktickFence ? QLatin1Char('`') : QLatin1Char('~');
        if (fence.hasMatch() && fence.captured(1).at(0) == fenceChar
                && text.mid(fence.capturedEnd(0)).trimmed().isEmpty())
            current = MathNormal;
        return finish();
    }

    // Most lines hold no math at all; skip the scan for them.
    if (current == MathNormal && !fence.hasMatch() && !text.contains(QLatin1Char('$'))
            && !text.contains(QLatin1Char('\\')))
        return finish();

    int i = 0;
    // A display formula still open from an earlier line.
    if (current & (MathDisplayDollars | MathDisplayBrackets)) {
        const QString closing = current & MathDisplayDollars ? QStringLiteral("$$")
                                                             : QStringLiteral("\\]");
        const int close = findUnescaped(text, closing, 0);
        if (close < 0) {
            add(0, int(text.length()), {0, 0}, {int(text.length()), 0});
            return finish();
        }
        add(0, close, {0, 0}, {close, int(closing.length())});
        i = close + closing.length();
        current = MathNormal;
    } else if (fence.hasMatch()) {
        current = fence.captured(1).at(0) == QLatin1Char('`') ? MathBacktickFence : MathTildeFence;
        return finish();
    }

    // The preview's inline rule (preview.js): no space just inside either
    // dollar, and no unescaped dollar within.
    static const QRegularExpression inlineRe(QStringLiteral(
        "\\$((?:[^\\s\\\\$]|\\\\[\\s\\S])(?:(?:[^$\\\\]|\\\\[\\s\\S])*?(?:[^\\s\\\\$]|\\\\[\\s\\S]))?)\\$"));

    const int length = text.length();
    while (i < length) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('`')) {
            // A code span: its content is never math.
            int run = 1;
            while (i + run < length && text.at(i + run) == QLatin1Char('`'))
                ++run;
            const QString ticks(run, QLatin1Char('`'));
            int close = text.indexOf(ticks, i + run);
            while (close >= 0 && close + run < length && text.at(close + run) == QLatin1Char('`'))
                close = text.indexOf(ticks, close + run + 1);
            i = close < 0 ? i + run : close + run;
            continue;
        }
        if (c == QLatin1Char('\\') && i + 1 < length) {
            const QChar next = text.at(i + 1);
            if (next == QLatin1Char('(')) {
                const int close = findUnescaped(text, QStringLiteral("\\)"), i + 2);
                if (close > i + 2) {
                    add(i + 2, close - i - 2, {i, 2}, {close, 2});
                    i = close + 2;
                    continue;
                }
            } else if (next == QLatin1Char('[')) {
                const int close = findUnescaped(text, QStringLiteral("\\]"), i + 2);
                if (close < 0) {
                    add(i + 2, length - i - 2, {i, 2}, {length, 0});
                    current = MathDisplayBrackets;
                    return finish();
                }
                add(i + 2, close - i - 2, {i, 2}, {close, 2});
                i = close + 2;
                continue;
            }
            // Any other backslash escapes the next character, \$ included.
            i += 2;
            continue;
        }
        if (c == QLatin1Char('$') && !isDigit(text, i - 1)) {
            if (i + 1 < length && text.at(i + 1) == QLatin1Char('$')) {
                const int close = findUnescaped(text, QStringLiteral("$$"), i + 2);
                if (close < 0) {
                    add(i + 2, length - i - 2, {i, 2}, {length, 0});
                    current = MathDisplayDollars;
                    return finish();
                }
                if (close > i + 2 && !isDigit(text, close + 2)) {
                    add(i + 2, close - i - 2, {i, 2}, {close, 2});
                    i = close + 2;
                    continue;
                }
                i += 2;
                continue;
            }
            const QRegularExpressionMatch match =
                inlineRe.match(text, i, QRegularExpression::NormalMatch,
                               QRegularExpression::AnchorAtOffsetMatchOption);
            if (match.hasMatch() && !isDigit(text, match.capturedEnd(0))) {
                const int end = match.capturedEnd(0);
                add(i + 1, end - i - 2, {i, 1}, {end - 1, 1});
                i = end;
                continue;
            }
        }
        ++i;
    }
    return finish();
}
