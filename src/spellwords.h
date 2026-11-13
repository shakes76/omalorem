#pragma once

#include <QChar>
#include <QList>
#include <QString>

// Which parts of a line of Markdown are prose, and so worth spell checking
// (docs/SPEC.md §4.10).
//
// This is the fork's own file and holds every rule about what counts as a word.
// MarkdownHighlighter passes in the spans it has already worked out for its own
// formatting — fenced code, code spans, math and link URLs — and gets back the
// words to check. Keeping the rules here, as a pure function over a line,
// means they can be tested without a document, a dictionary or a window.

struct SpellWord {
    int start;
    int length;
};

// A range of the line that holds no prose.
struct SpellSkip {
    int start;
    int length;
};

inline bool spellWordCharacter(QChar character) {
    return character.isLetter() || character == u'\'' || character == u'’';
}

// A token that is not prose, however it is spelled: a URL, a path, an e-mail
// address, or anything carrying a digit. Checked on the whole token, before the
// apostrophes are trimmed, so "docs/SPEC.md" and "v1.9" are let through whole.
inline bool spellTokenIsProse(const QString &line, int start, int length) {
    bool sawLetter = false;
    for (int i = start; i < start + length; ++i) {
        const QChar character = line.at(i);
        if (character.isDigit())
            return false;
        if (character.isLetter())
            sawLetter = true;
    }
    if (!sawLetter)
        return false;

    // A token glued to a slash, a dot, an at sign or a colon is part of a URL,
    // a path or an address. Look at the characters either side of the word
    // rather than trying to parse the address itself.
    const int before = start - 1;
    const int after = start + length;
    const auto glued = [&](int index) {
        if (index < 0 || index >= line.size())
            return false;
        const QChar character = line.at(index);
        return character == u'/' || character == u'@' || character == u':'
            || character == u'\\';
    };
    if (glued(before) || glued(after))
        return false;
    // A dot only glues when a letter follows it, so "end." is prose and
    // "file.md" is not.
    if (after < line.size() - 1 && line.at(after) == u'.'
        && line.at(after + 1).isLetter()) {
        return false;
    }
    if (before > 0 && line.at(before) == u'.' && line.at(before - 1).isLetter())
        return false;
    return true;
}

// The words of a line that are prose, outside every skip range.
// Words of one letter are left alone: "a" and "I" are words, and a lone letter
// is more often a label or an initial than a misspelling.
inline QList<SpellWord> spellWords(const QString &line, const QList<SpellSkip> &skip) {
    QList<SpellWord> words;
    const auto skipped = [&](int start, int length) {
        for (const SpellSkip &range : skip) {
            if (start < range.start + range.length && range.start < start + length)
                return true;
        }
        return false;
    };

    int index = 0;
    while (index < line.size()) {
        if (!spellWordCharacter(line.at(index))) {
            ++index;
            continue;
        }
        const int start = index;
        while (index < line.size() && spellWordCharacter(line.at(index)))
            ++index;
        int length = index - start;

        if (!spellTokenIsProse(line, start, length))
            continue;

        // Leading and trailing apostrophes belong to the quoting, not the word:
        // 'word' is word, but don't stays whole.
        int wordStart = start;
        while (length > 0 && !line.at(wordStart).isLetter()) {
            ++wordStart;
            --length;
        }
        while (length > 0 && !line.at(wordStart + length - 1).isLetter())
            --length;

        if (length < 2)
            continue;
        if (skipped(wordStart, length))
            continue;
        words.append({wordStart, length});
    }
    return words;
}
