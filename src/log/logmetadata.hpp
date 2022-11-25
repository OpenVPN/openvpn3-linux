//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018 - 2022  OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018 - 2022  David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file   logmetadata.hpp
 *
 * @brief  Declaration of LogMetaData and LogMetaDataValue classes
 */

#pragma once

#include <memory>
#include <vector>
#include <sstream>

#include "logtag.hpp"


/**
 *  The LogMetaDataValue is a container for a single labelled meta data value.
 *
 *  The value can be either a std::string or a LogTag object
 */
struct LogMetaDataValue
{
    enum class Type
    {
        LOGMETA_STRING,
        LOGMETA_LOGTAG
    };

    using Ptr = std::shared_ptr<LogMetaDataValue>;

    LogMetaDataValue(const std::string &l, const std::string &v, bool s = false);
    LogMetaDataValue(const std::string &l, const LogTag::Ptr v, bool s = false);


    /**
     *  This is a static helper method to create a LogMetaDataValue::Ptr object
     *  with the right constructor.
     *
     * @tparam T  The type can be either std::string or LogTag
     * @param  l  std::string carrying the label (key) of the meta data value
     * @param  v  A std::string or LogTag object carrying the meta data value
     * @param  s  bool flag to indicate if this value should be skipped when
     *            the LogMetaData::operator<<() is used.
     *
     * @return Returns a LogMetaData::Ptr (std::shard_ptr) to the LogMetaDataValue
     *         object created.
     */
    template <typename T>
    static LogMetaDataValue::Ptr create(const std::string &l,
                                        const T &v,
                                        bool s = false)
    {
        LogMetaDataValue::Ptr ret;
        ret.reset(new LogMetaDataValue(l, v, s));
        return ret;
    }


    /**
     *  Retrieve the value of this meta data as a std::string, regardless of
     *  how this LogMetaDataValue object was created.
     *
     * @param logtag_encaps  If this object carries a LogTag value, setting this
     *                       to true will encapsulate the tag hash value with
     *                       "{tag:......}"
     *
     * @return  Returns a std::string of this objects value
     */
    const std::string GetValue(const bool logtag_encaps = true) const;


    friend std::ostream &operator<<(std::ostream &os, const LogMetaDataValue mdv)
    {
        if (mdv.skip)
        {
            return os;
        }
        bool encaps = (mdv.logtag ? mdv.logtag->encaps : true);
        return os << mdv.label << "=" << mdv.GetValue(encaps);
    }


    Type type;
    std::string label;
    const std::string str_value;
    const LogTag::Ptr logtag;
    bool skip;
};



/**
 *  The LogMetaData class is a container for LogMetaDataValue objects
 */
class LogMetaData
{
  public:
    using Ptr = std::shared_ptr<LogMetaData>;
    using Records = std::vector<std::string>;

    LogMetaData(){};
    ~LogMetaData() = default;


    /**
     *  Create a new LogMetaData container for LogMetaDataValue objects
     *
     * @return Returns a LogMetaData::Ptr (std::shared_ptr) to the new container
     */
    static LogMetaData::Ptr create()
    {
        LogMetaData::Ptr ret;
        ret.reset(new LogMetaData);
        return ret;
    }


    /**
     *  Adds a new key/value based meta data to the LogMetaData container
     *
     * @tparam T   The type must be either std::string or LogTag
     * @param  l   std::string of the label (key) of this meta data value
     * @param  v   std::string or LogTag object carrying the meta data value
     * @param  s   bool flag to indicate if this value should be skipped when
     *             the LogMetaData::operator<<() is used.
     */
    template <typename T>
    void AddMeta(const std::string &l, const T &v, bool skip = false)
    {
        auto mdv = LogMetaDataValue::create(l, v, skip);
        metadata.push_back(mdv);
    }


    /**
     *   Retrieve the meta data value as a string for a specific meta data
     *   label (key).
     *
     * @param l             std::string carrying the label/key to look up
     * @param encaps_logtag If it is a LogTag object, if set to true the value
     *                      will be encapsulated with '{tag:......}'
     * @param postfix       std::string to be added after the value extraction.
     *                      Defaults to " ".
     *
     * @return Returns a std::string of the meta data value
     */
    std::string GetMetaValue(const std::string l, const bool encaps_logtag = true, const std::string postfix = " ") const;


    /**
     *  Retrieve all collected meta data values, formatted as "key=value" pairs
     *
     * @param upcase_label   bool flag to upper case the label/key value
     * @param logtag_encaps  bool flag to enable encapsulation of LogTag values
     *                        with '{tag:......'}
     * @return  Returns LogMetaData::Records (std::vector<std::string>) of all
     *          collected meta data values as key/value pairs.
     */
    Records GetMetaDataRecords(const bool upcase_label = false,
                               const bool logtag_encaps = true) const;


    /**
     *  Retrieve how many meta data values has been collected
     */
    size_t size() const;


    /**
     *  Check if any meta data values has been collected yet
     */
    bool empty() const;


    /**
     *  Clear all collected meta data values
     */
    void clear();


    friend std::ostream &operator<<(std::ostream &os, const LogMetaData &mdc)
    {
        bool first = true;
        for (const auto &mdv : mdc.metadata)
        {
            if (mdv->skip)
            {
                continue;
            }
            os << (!first ? ", " : "") << (*mdv);
            if (first)
            {
                first = false;
            }
        }
        return os;
    }


  private:
    std::vector<LogMetaDataValue::Ptr> metadata;
};
