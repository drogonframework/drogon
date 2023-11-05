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

#ifndef __HPackTable_H__
#define __HPackTable_H__

#include <string>
#include <deque>
#include <map>

namespace hpack {

class HPackTable
{
public:
    using KeyValuePair = std::pair<std::string, std::string>;
    
public:
    HPackTable();
    void setMode(bool isEncoder) { isEncoder_ = isEncoder; }
    void setMaxSize(size_t maxSize);
    void updateLimitSize(size_t limitSize);
    int  getIndex(const std::string &name, const std::string &value, bool &valueIndexed);
    bool getIndexedName(int index, std::string &name);
    bool getIndexedValue(int index, std::string &value);
    bool addHeader(const std::string &name, const std::string &value);
    
    size_t getMaxSize() { return maxSize_; }
    size_t getLimitSize() { return limitSize_; }
    size_t getTableSize() { return tableSize_; }
    
private:
    int getDynamicIndex(int idxSeq);
    void updateIndex(const std::string &key, int idxSeq);
    void removeIndex(const std::string &key);
    bool getIndex(const std::string &key, int &indexD, int &indexS);
    void evictTableBySize(size_t size);
    
private:
    using KeyValueQueue = std::deque<KeyValuePair>;
    using IndexMap = std::map<std::string, std::pair<int, int>>;
    
    KeyValueQueue dynamicTable_;
    size_t tableSize_ = 0;
    size_t limitSize_ = 4096;
    size_t maxSize_ = 4096;
    
    bool isEncoder_ = false;
    int indexSequence_ = 0;
    IndexMap indexMap_; // <key(header name[ + value]), <dynamic index sequence, static index>>
};

} // namespace hpack

#endif /* __HPackTable_H__ */
