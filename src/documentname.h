#pragma once

#include <QChar>
#include <QString>

// The name Save suggests for an unsaved document, from its first line
// (docs/SPEC.md §4.8).
//
// Omawrite takes that line as it stands, and a first line is almost always a
// Markdown heading, so Save offers "# Spherical harmonics.md" — hash, space
// and all. Every rule about what gets dropped lives here, in the fork's own
// code; Omawrite's suggestedFileName() only calls it, and keeps its own rules
// about slashes, control characters, the length limit and Untitled.
//
// Markers are dropped from both ends, because a heading can be closed
// ("## Part two ##") and a title can be wrapped in emphasis ("**Notes**").
// Only the ends are touched, so "C# notes" keeps its hash.

inline bool isDocumentNameMarker(QChar character) {
    return character == u'#' || character == u'>' || character == u'-' || character == u'*'
        || character == u'+' || character == u'_' || character.isSpace();
}

inline QString documentNameFromFirstLine(const QString &line) {
    int start = 0;
    int end = line.size();
    while (start < end && isDocumentNameMarker(line.at(start)))
        ++start;
    while (end > start && isDocumentNameMarker(line.at(end - 1)))
        --end;
    // Nothing but markers is no name at all; the caller's Untitled rule
    // takes it from here.
    return line.mid(start, end - start);
}
