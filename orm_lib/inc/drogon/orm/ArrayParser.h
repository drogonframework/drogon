/** Handling of SQL arrays.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include drogon/orm/Field.h instead.
 *
 * Copyright (c) 2018, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 */
//Taken from libpqxx and modified

#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace drogon
{
namespace orm
{
/// Low-level array parser.
/** Use this to read an array field retrieved from the database.
 *
 * Use this only if your client encoding is UTF-8, ASCII, or a single-byte
 * encoding which is a superset of ASCII.
 *
 * The input is a C-style string containing the textual representation of an
 * array, as returned by the database.  The parser reads this representation
 * on the fly.  The string must remain in memory until parsing is done.
 *
 * Parse the array by making calls to @c get_next until it returns a
 * @c juncture of "done".  The @c juncture tells you what the parser found in
 * that step: did the array "nest" to a deeper level, or "un-nest" back up?
 */
class ArrayParser
{
  public:
    /// What's the latest thing found in the array?
    enum juncture
    {
        /// Starting a new row.
        row_start,
        /// Ending the current row.
        row_end,
        /// Found a NULL value.
        null_value,
        /// Found a string value.
        string_value,
        /// Parsing has completed.
        done,
    };

    /// Constructor.  You don't need this; use @c field::as_array instead.
    explicit ArrayParser(const char input[]);

    /// Parse the next step in the array.
    /** Returns what it found.  If the juncture is @c string_value, the string
   * will contain the value.  Otherwise, it will be empty.
   *
   * Call this until the @c juncture it returns is @c done.
   */
    std::pair<juncture, std::string> getNext();

  private:
    /// Current parsing position in the input.
    const char *m_pos;
};

} // namespace orm
} // namespace drogon
