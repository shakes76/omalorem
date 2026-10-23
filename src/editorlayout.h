#pragma once

#include <QtGlobal>

// The editor's column width, with Omalorem's narrow-margin mode
// (docs/SPEC.md §4.6).
//
// Omawrite caps the column at the 65-character reading measure it took from
// Typora, and otherwise reserves a gutter of 20 characters, 10 a side. That
// reserve never shrinks, so in a window too narrow for the measure it is a
// flat cost: half of a 1536 px screen leaves 43 characters with 120 px empty
// on either side. Narrow margins drops the reserve to the 24 px the preview's
// #content keeps, so the two columns line up, and keeps the cap, because a
// 128-character line reads worse than a wide margin looks.
//
// Header-only and free of Qt GUI types, so the tests can call it directly and
// no_preview builds get it too.

// The preview's own side padding (preview.css), as a literal rather than a
// scaled size, because the preview's padding is a literal too.
inline constexpr int editorNarrowGutter = 24;
inline constexpr int editorColumnCharacters = 65;
inline constexpr int editorWideGutterCharacters = 20;
inline constexpr int editorColumnMinimum = 360;

inline int editorColumnWidth(qreal characterWidth, qreal windowWidth, bool narrow) {
    const int measure = qRound(characterWidth * editorColumnCharacters);
    const int reserve = narrow ? 2 * editorNarrowGutter
                               : qRound(characterWidth * editorWideGutterCharacters);
    return qMin(measure, qMax(editorColumnMinimum, qRound(windowWidth) - reserve));
}
