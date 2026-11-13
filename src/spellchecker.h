#pragma once

#include <QHash>
#include <QSet>
#include <QObject>
#include <QStringList>

#include <memory>

class Hunspell;

// Omalorem spell check (docs/SPEC.md §4.10): Hunspell over the dictionaries the
// system already has.
//
// The dictionary is the expensive part — megabytes of affix rules — so nothing
// is read until the first word is checked, and nothing at all when spell check
// is off. Construction only works out which file would be used.
class SpellChecker : public QObject {
    Q_OBJECT

public:
    explicit SpellChecker(QObject *parent = nullptr);
    ~SpellChecker() override;

    // Where dictionaries are looked for, in order. Exposed so the tests can
    // state the search path rather than depend on what this machine has.
    static QStringList searchPaths();
    // The <base>.dic matching a locale in those paths, or an empty string.
    // "en_AU" is tried whole, then any en_*, then any dictionary at all.
    static QString findDictionary(const QString &language,
                                  const QStringList &paths = searchPaths());

    // Use this dictionary base path (without .dic/.aff). Empty means none, and
    // available() then stays false. Used by the tests and by the language
    // setting; otherwise the locale decides.
    void setDictionary(const QString &basePath);
    QString dictionary() const { return m_basePath; }
    // The dictionary's name, e.g. "en_AU". Shown when spell check is turned
    // on, because a desktop set to en_US will quietly mark British and
    // Australian spelling wrong and the writer should be told which one is
    // judging them.
    QString language() const;

    // False when no dictionary could be found or it would not load. The
    // message says which, in words meant for the status line.
    bool available() const;
    QString unavailableMessage() const;

    // True when the word is known, or when there is no dictionary at all:
    // without one, nothing is a misspelling.
    bool check(const QString &word);
    QStringList suggest(const QString &word);

    // Kept in ~/.local/share/omalorem/personal.dic, one word a line.
    void addToPersonal(const QString &word);
    static QString personalPath();
    // Known for this document only, and not written anywhere.
    void ignore(const QString &word);
    void clearIgnored();

signals:
    // The known words changed, so anything already underlined should be
    // highlighted again.
    void vocabularyChanged();

private:
    void ensureLoaded();

    QString m_basePath;
    bool m_basePathResolved = false;
    bool m_loadAttempted = false;
    QString m_unavailableMessage;
    std::unique_ptr<Hunspell> m_hunspell;
    QString m_encoding;
    QSet<QString> m_ignored;
    QHash<QString, bool> m_cache;
};
