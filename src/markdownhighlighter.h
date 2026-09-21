#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *document);

    void setDarkMode(bool darkMode);
    void setColors(const QString &background, const QString &foreground, const QString &accent);
    void setSearch(const QString &query, int currentMatchStart);

    struct Span {
        int start;
        int length;
    };

    enum class InlineKind { Bold, Italic, Link };

    struct InlineMarkup {
        InlineKind kind;
        Span content;
        Span markers[2];
    };

    // Single source of truth for inline markdown spans: the highlighter uses it
    // to style content and hide markers, and the editor uses it (via
    // Backend::hiddenRangesAt) to skip the caret over the hidden markers.
    static QList<InlineMarkup> inlineMarkup(const QString &text);

    // --- Omalorem editor math (docs/SPEC.md §5.6) ---------------------------
    // Math spans in one line: $…$, $$…$$, \(…\) and \[…\], found by the same
    // rules the preview's parser uses. Display math can run over several
    // lines, so a span may have no opening or closing marker on this line
    // (length 0), and the scan carries a block state from line to line.
    struct MathSpan {
        Span content;
        Span markers[2];
    };

    // Block states, as QSyntaxHighlighter stores them. Upstream never sets a
    // state, so every state belongs to the math rule. -1 (no state) is normal.
    enum MathState {
        MathNormal = 0,
        MathBacktickFence = 1,
        MathTildeFence = 2,
        MathDisplayDollars = 4,
        MathDisplayBrackets = 8,
    };

    // Scans a line that starts in previousState; *state receives the state
    // at its end. Fenced code and code spans hold no math.
    static QList<MathSpan> mathSpans(const QString &text, int previousState,
                                     int *state = nullptr);
    // Whether an inline item's markers fall inside math: `a_1 b_2` in a
    // formula is not emphasis, so those markers are neither hidden nor
    // skipped by the caret.
    static bool touchesMath(const InlineMarkup &item, const QList<MathSpan> &math);

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();
    void highlightMarkers(const QString &text);
    void highlightInline(const QString &text);
    void highlightSearch(const QString &text);
    void highlightMath();

    bool m_darkMode = true;
    QString m_customBackground;
    QString m_customForeground;
    QString m_customAccent;
    QTextCharFormat m_markerFormat;
    QTextCharFormat m_hiddenMarkerFormat;
    QTextCharFormat m_headingFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_quoteFormat;
    QTextCharFormat m_linkFormat;
    QString m_searchQuery;
    int m_currentMatchStart = -1;
    QTextCharFormat m_searchFormat;
    QTextCharFormat m_currentSearchFormat;
    QTextCharFormat m_mathFormat;
    // The math spans of the block being highlighted, for highlightInline.
    QList<MathSpan> m_blockMath;
};
