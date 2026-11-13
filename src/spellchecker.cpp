#include "spellchecker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTextStream>

#include <hunspell/hunspell.hxx>

namespace {

// Hunspell's own cap; more than this is a menu nobody reads.
constexpr int suggestionLimit = 7;

}  // namespace

SpellChecker::SpellChecker(QObject *parent) : QObject(parent) {}

SpellChecker::~SpellChecker() = default;

QStringList SpellChecker::searchPaths() {
    QStringList paths;
    const QString home = QDir::homePath();
    paths << home + QStringLiteral("/.local/share/hunspell")
          << QStringLiteral("/usr/share/hunspell")
          << QStringLiteral("/usr/share/myspell/dicts")
          << QStringLiteral("/usr/share/myspell");
    return paths;
}

QString SpellChecker::findDictionary(const QString &language, const QStringList &paths) {
    const auto base = [&](const QDir &dir, const QString &name) {
        const QFileInfo dictionary(dir.filePath(name + QStringLiteral(".dic")));
        const QFileInfo affix(dir.filePath(name + QStringLiteral(".aff")));
        return dictionary.isFile() && affix.isFile() ? dir.filePath(name) : QString();
    };

    // The language itself, then another country's variant of it, then whatever
    // is installed: a writer with only en_GB is better served by it than by
    // nothing at all.
    const QString prefix = language.section(QLatin1Char('_'), 0, 0);
    QString sameLanguage;
    QString anyLanguage;
    for (const QString &path : paths) {
        const QDir dir(path);
        if (!dir.exists())
            continue;
        if (!language.isEmpty()) {
            const QString exact = base(dir, language);
            if (!exact.isEmpty())
                return exact;
        }
        const QStringList names = dir.entryList({QStringLiteral("*.dic")}, QDir::Files,
                                                QDir::Name);
        for (const QString &name : names) {
            const QString stem = QFileInfo(name).completeBaseName();
            const QString candidate = base(dir, stem);
            if (candidate.isEmpty())
                continue;
            if (sameLanguage.isEmpty() && !prefix.isEmpty()
                && stem.section(QLatin1Char('_'), 0, 0) == prefix) {
                sameLanguage = candidate;
            }
            if (anyLanguage.isEmpty())
                anyLanguage = candidate;
        }
    }
    return !sameLanguage.isEmpty() ? sameLanguage : anyLanguage;
}

void SpellChecker::setDictionary(const QString &basePath) {
    if (m_basePathResolved && m_basePath == basePath)
        return;
    m_basePath = basePath;
    m_basePathResolved = true;
    m_loadAttempted = false;
    m_hunspell.reset();
    m_cache.clear();
    emit vocabularyChanged();
}

void SpellChecker::ensureLoaded() {
    if (m_loadAttempted)
        return;
    m_loadAttempted = true;

    if (!m_basePathResolved) {
        m_basePath = findDictionary(QLocale::system().name());
        m_basePathResolved = true;
    }
    if (m_basePath.isEmpty()) {
        // The normal state on a fresh Arch system, so name the package rather
        // than leaving the writer to wonder why nothing is underlined.
        m_unavailableMessage =
            QStringLiteral("No spelling dictionary found. Install hunspell-en_au "
                           "(or another hunspell-* dictionary).");
        return;
    }

    const QByteArray affix = (m_basePath + QStringLiteral(".aff")).toLocal8Bit();
    const QByteArray dictionary = (m_basePath + QStringLiteral(".dic")).toLocal8Bit();
    m_hunspell = std::make_unique<Hunspell>(affix.constData(), dictionary.constData());
    m_encoding = QString::fromLatin1(m_hunspell->get_dic_encoding());
    if (m_encoding.isEmpty())
        m_encoding = QStringLiteral("UTF-8");

    // Words the writer has added are part of the dictionary from here on.
    QFile personal(personalPath());
    if (personal.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&personal);
        while (!stream.atEnd()) {
            const QString word = stream.readLine().trimmed();
            if (!word.isEmpty())
                m_hunspell->add(word.toStdString());
        }
    }
}

QString SpellChecker::language() const {
    const_cast<SpellChecker *>(this)->ensureLoaded();
    return m_hunspell ? QFileInfo(m_basePath).completeBaseName() : QString();
}

bool SpellChecker::available() const {
    const_cast<SpellChecker *>(this)->ensureLoaded();
    return m_hunspell != nullptr;
}

QString SpellChecker::unavailableMessage() const {
    const_cast<SpellChecker *>(this)->ensureLoaded();
    return m_hunspell ? QString() : m_unavailableMessage;
}

bool SpellChecker::check(const QString &word) {
    if (word.isEmpty())
        return true;
    if (m_ignored.contains(word))
        return true;

    ensureLoaded();
    // Without a dictionary nothing is a misspelling; the message is what tells
    // the writer, not a document full of red.
    if (!m_hunspell)
        return true;

    const auto cached = m_cache.constFind(word);
    if (cached != m_cache.constEnd())
        return *cached;

    // Hunspell speaks the dictionary's own encoding, which for the older
    // dictionaries is not UTF-8.
    auto encode = QStringEncoder(m_encoding.toLatin1().constData());
    const QByteArray encoded = encode.isValid() ? QByteArray(encode(word)) : word.toUtf8();
    const bool known = m_hunspell->spell(encoded.toStdString());
    m_cache.insert(word, known);
    return known;
}

QStringList SpellChecker::suggest(const QString &word) {
    ensureLoaded();
    if (!m_hunspell || word.isEmpty())
        return {};

    auto encode = QStringEncoder(m_encoding.toLatin1().constData());
    auto decode = QStringDecoder(m_encoding.toLatin1().constData());
    const QByteArray encoded = encode.isValid() ? QByteArray(encode(word)) : word.toUtf8();

    QStringList suggestions;
    for (const std::string &candidate : m_hunspell->suggest(encoded.toStdString())) {
        const QByteArray raw = QByteArray::fromStdString(candidate);
        suggestions << (decode.isValid() ? QString(decode(raw)) : QString::fromUtf8(raw));
        if (suggestions.size() == suggestionLimit)
            break;
    }
    return suggestions;
}

QString SpellChecker::personalPath() {
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return directory + QStringLiteral("/personal.dic");
}

void SpellChecker::addToPersonal(const QString &word) {
    if (word.isEmpty())
        return;

    const QString path = personalPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        // Appending the same word twice would grow the file forever.
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            if (stream.readLine().trimmed() == word)
                return;
        }
        stream << word << '\n';
    }

    ensureLoaded();
    if (m_hunspell)
        m_hunspell->add(word.toStdString());
    m_cache.remove(word);
    emit vocabularyChanged();
}

void SpellChecker::ignore(const QString &word) {
    if (word.isEmpty() || m_ignored.contains(word))
        return;
    m_ignored.insert(word);
    m_cache.remove(word);
    emit vocabularyChanged();
}

void SpellChecker::clearIgnored() {
    if (m_ignored.isEmpty())
        return;
    m_ignored.clear();
    m_cache.clear();
    emit vocabularyChanged();
}
