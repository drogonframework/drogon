#pragma once

#include <drogon/utils/Utilities.h>

#include <filesystem>
#include <string>

namespace drogon::test::sqlite3
{
/**
 * @brief Encode a filesystem path as a Sqlite3 connection string value.
 *
 * @details DbConnection::parseConnString() removes the backslashes that are not
 * part of one of the documented escapes, so a native path cannot be used as is
 * on Windows: the connection string `filename='C:\Users\me\x.db'` is parsed as
 * the drive-relative `C:Usersmex.db`. Convert the path to its generic UTF-8
 * form first, escape the characters that are still significant (`'` and `\`),
 * and quote the whole value.
 *
 * @param path Path of the database file.
 *
 * @return The value part of a `filename=` connection string parameter.
 */
inline std::string quoteConnectionValue(const std::filesystem::path &path)
{
    const auto value = drogon::utils::fromNativePath(path.native());
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('\'');
    for (const auto ch : value)
    {
        if (ch == '\'' || ch == '\\')
            escaped.push_back('\\');
        escaped.push_back(ch);
    }
    escaped.push_back('\'');
    return escaped;
}
}  // namespace drogon::test::sqlite3
