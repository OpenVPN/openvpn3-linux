//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   logmetadata.hpp
 *
 * @brief  Declaration of LogMetaData and LogMetaDataValue classes
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <sstream>

#include "logtag.hpp"


/**
 *  The LogMetaDataValue is a container for a single labelled meta data value.
 *
 *  The value can be int32_t, uint32_t, std::string or a LogTag object
 */
struct LogMetaDataValue
{
    enum class Type
    {
        LOGMETA_STRING,
        LOGMETA_LOGTAG
    };

    const Type type;
    const std::string label;
    const std::string str_value;
    const LogTag::Ptr logtag;
    const bool skip;

    using Ptr = std::shared_ptr<LogMetaDataValue>;

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
    [[nodiscard]] static LogMetaDataValue::Ptr Create(const std::string &l,
                                                      const T &v,
                                                      bool s = false)
    {
        return LogMetaDataValue::Ptr(new LogMetaDataValue(l, v, s));
    }

    ~LogMetaDataValue() noexcept = default;

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

    friend std::ostream &operator<<(std::ostream &os, const LogMetaDataValue::Ptr mdv)
    {
        if (mdv->skip)
        {
            return os;
        }
        bool encaps = (mdv->logtag ? mdv->logtag->encaps : true);
        return os << mdv->label << "=" << mdv->GetValue(encaps);
    }


  private:
    LogMetaDataValue(const std::string &l, const std::string &v, bool s);
    LogMetaDataValue(const std::string &l, const uint32_t v, bool s);
    LogMetaDataValue(const std::string &l, const int32_t v, bool s);
    LogMetaDataValue(const std::string &l, LogTag::Ptr v, bool s);
};



/**
 *  The LogMetaData class is a container for LogMetaDataValue objects
 */
class LogMetaData
{
  public:
    using Ptr = std::shared_ptr<LogMetaData>;
    using Records = std::vector<std::string>;

    /**
     *  Create a new LogMetaData container for LogMetaDataValue objects
     *
     * @return Returns a LogMetaData::Ptr (std::shared_ptr) to the new container
     */
    [[nodiscard]] static LogMetaData::Ptr Create()
    {
        return LogMetaData::Ptr(new LogMetaData);
    }

    /**
     *  Create a duplicate (copy) of this LogMetaData container
     *
     *  NOTE: This will not duplicate the LogMetaDataValue records
     *        stored in this LogMetaData container, they will be
     *        shared between the source and the copy.
     *
     *        However, any modifications (adding new values, clearing
     *        all values) to either source or copy
     *        will be local to the modified object
     *
     * @return LogMetaData::Ptr to the new LogMetaData container with the
     *         same LogMetaDataValue elements as this container has.
     */
    LogMetaData::Ptr Duplicate()
    {
        return LogMetaData::Ptr(new LogMetaData(*this));
    }

    ~LogMetaData() noexcept = default;

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
        auto mdv = LogMetaDataValue::Create(l, v, skip);
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


    friend std::ostream &operator<<(std::ostream &os, const LogMetaData::Ptr mdc)
    {
        bool first = true;
        for (const auto &mdv : mdc->metadata)
        {
            if (mdv->skip)
            {
                continue;
            }
            os << (!first ? ", " : "") << mdv;
            if (first)
            {
                first = false;
            }
        }
        return os;
    }


  private:
    std::vector<LogMetaDataValue::Ptr> metadata = {};

    LogMetaData() = default;

    /**
     *  Private copy constructor
     *
     *  NOTE: This will not duplicate the LogMetaDataValue records
     *        stored in this LogMetaData container, they will be
     *        shared between the source and the copy.
     *
     *        However, any modifications (adding new values, clearing
     *        all values) to either source or copy
     *        will be local to the modified object
     *
     * @param src  Source LogMetaData object to copy elements from
     */
    LogMetaData(const LogMetaData &src);
};
