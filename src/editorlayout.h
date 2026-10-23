#pragma once

#include <QtGlobal>

// The editor's column width, with Omalorem's wide-column mode
// (docs/SPEC.md §4.6).
//
// Omawrite caps the column at the 65-character reading measure it took from
// Typora, and otherwise reserves a gutter of 20 characters, 10 a side. Both
// waste a narrow window: the reserve is a flat 10 characters a side whatever
// the window does, and once the text is small enough for 65 characters to fit,
// everything else the window has becomes margin — so making the text smaller
// stops buying any more of it.
//
// The wide column answers both: the gutter drops to the 24 px the preview's
// #content already keeps, and the measure loosens from 65 characters to 80,
// the usual upper bound for technical prose. The preview follows the same two
// numbers, so the columns line up.
//
// Header-only and free of Qt GUI types, so the tests can call it directly and
// no_preview builds get it too.

// The preview's own side padding (preview.css), as a literal rather than a
// scaled size, because the preview's padding is a literal too.
inline constexpr int editorWideColumnGutter = 24;
inline constexpr int editorColumnCharacters = 65;
inline constexpr int editorWideColumnCharacters = 80;
inline constexpr int editorDefaultGutterCharacters = 20;
inline constexpr int editorColumnMinimum = 360;

// The measure the preview's #content should use, in characters.
inline int editorColumnCharactersFor(bool wide) {
    return wide ? editorWideColumnCharacters : editorColumnCharacters;
}

inline int editorColumnWidth(qreal characterWidth, qreal windowWidth, bool wide) {
    const int measure = qRound(characterWidth * editorColumnCharactersFor(wide));
    const int reserve = wide ? 2 * editorWideColumnGutter
                             : qRound(characterWidth * editorDefaultGutterCharacters);
    return qMin(measure, qMax(editorColumnMinimum, qRound(windowWidth) - reserve));
}
