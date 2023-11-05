/* Copyright (c) 2016, Fengping Bao <jamol@live.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __HPacker_H__
#define __HPacker_H__

#include <string>
#include <vector>
#include <functional>

#include "HPackTable.h"

namespace hpack {

class HPacker
{
public:
    enum class IndexingType {
        NONE, // not index
        NAME, // index name only
        ALL   // index name and value
    };
    
    using KeyValuePair = HPackTable::KeyValuePair;
    using KeyValueVector = std::vector<KeyValuePair>;
    using IndexingTypeCallback = std::function<IndexingType (const std::string&, const std::string&)>;
    
public:
    int encode(const KeyValueVector &headers, uint8_t *buf, size_t len);
    int decode(const uint8_t *buf, size_t len, KeyValueVector &headers);
    void setMaxTableSize(size_t maxSize) { table_.setMaxSize(maxSize); }
    void setIndexingTypeCallback(IndexingTypeCallback cb) { query_cb_ = std::move(cb); }
    
private:
    int encodeHeader(const std::string &name, const std::string &value, uint8_t *buf, size_t len);
    int encodeSizeUpdate(int sz, uint8_t *buf, size_t len);

    IndexingType getIndexingType(const std::string &name, const std::string &value);
    
private:
    HPackTable table_;
    IndexingTypeCallback query_cb_;
    bool updateTableSize_ = true;
};

} // namespace hpack

#endif /* __HPacker_H__ */
