//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C)  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C)  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   requiresqueue.hpp
 *
 * @brief  A class which implements just the D-Bus server/service side of
 *         the RequiresQueue.  This provides a C++ API which will implements
 *         the introspection snippet generation, the D-Bus service methods
 *         in addition to the methods the service needs to prepare the
 *         RequiresQueue and retrieve the user responses sent from a
 *         front-end.
 */

#pragma once
#include <map>
#include <vector>
#include <glib.h>
#include <gio/gio.h>
#include <gdbuspp/exceptions.hpp>
#include <gdbuspp/object/base.hpp>
#include <mutex>

#include "build-config.h"
#include "dbus/constants.hpp"


/**
 *   A single slot which keeps a single requirement the a front-end
 *   must provide back.
 */
struct RequiresSlot
{
    RequiresSlot();

    unsigned int id;              ///< Unique ID per type/group
    ClientAttentionType type;     ///< Type categorization of the requirement
    ClientAttentionGroup group;   ///< Group categorization of the
                                  ///< requirement
    std::string name;             ///< Variable name of the requirement, used
                                  ///< by the service backend to more easily
                                  ///< retrieve a specific slot
    std::string value;            ///< Value provided by the front-end
    std::string user_description; ///< Description the front-end can show
                                  ///< to a user
    bool hidden_input;            ///< Should user's input be masked?
    bool provided;                ///< Have the front-end already provided
                                  ///< the requested information?
};



/**
 *  Special exception class used to signal the various stages
 *  of the requires queue and its slots.  Can be thrown both when
 *  all work has been completed, if an error occurred, etc.
 */
class RequiresQueueException : public DBus::Exception
{
  public:
    /**
     *  Used for errors which is not classified.
     *
     * @param err   A string containing the error message
     */
    RequiresQueueException(std::string err);

    /**
     *  Used for both state signalling and errors
     *
     * @param errname  A string containing a "tag name" of the state/error
     * @param errmsg   A string describing the event
     */
    RequiresQueueException(std::string errname, std::string errmsg);

    virtual ~RequiresQueueException() noexcept = default;
};



/**
 *  Implements the service/server side of the RequiresQueue
 */

class RequiresQueue
{
  public:
    using Ptr = std::shared_ptr<RequiresQueue>;
    typedef std::tuple<ClientAttentionType, ClientAttentionGroup> ClientAttTypeGroup;

    [[nodiscard]] static RequiresQueue::Ptr Create()
    {
        return RequiresQueue::Ptr(new RequiresQueue);
    }

    ~RequiresQueue();

    /**
     * Sets up the RequiresQueue D-Bus methods required for processing
     * requested end-user input.
     *
     * The meth_* strings below are the method names exposed in the D-Bus
     * object, used by the proxy caller
     *
     * @param object_ptr         Raw pointer to the DBus::Object::Base
     *                           this RequiresQueue is integrated with.
     *                           This CANNOT be nullptr.
     * @param meth_qchktypegr    Method name for retrieving a list of
     *                           unprocessed requirement type/groups
     * @param meth_queuefetch    Method name for fetching an unprocessed
     *                           queued item
     * @param meth_queuechk      Method name for retrieving the number of
     *                           unprocessed queued items
     * @param meth_provideresp   Method name for providing the user's
     *                           respose to a queued item.
     */
    void QueueSetup(DBus::Object::Base *object_ptr,
                    const std::string &meth_qchktypegr,
                    const std::string &meth_queuefetch,
                    const std::string &meth_queuechk,
                    const std::string &meth_provideresp);

    /**
     *  Empties and clears all the required items in the RequiresQueue
     */
    void ClearAll() noexcept;

    /**
     * Adds a request requirement item to the queue.
     *
     * The type and group arguments allows a single RequiresQueue object to
     * process multiple queues in parallel.
     *
     * @param type   ClientAttentionType reference
     * @param group  ClientAttentionGroup reference
     * @param name   String with a variable name for the requested input
     * @param descr  A human readable description of the value being
     *               requested.  This may be presented to the user directly.
     *
     * @return Returns the assigned ID for this requirement
     */
    unsigned int RequireAdd(ClientAttentionType type,
                            ClientAttentionGroup group,
                            std::string name,
                            std::string descr,
                            bool hidden_input);

    /**
     *  Fetch a single item from the request queue, where the item
     *  to be retrieved is provided via the GVariant argument.
     *
     *  This GVariant object must have the data type spec '(uuu)', where
     *  they refer to the request type, group and ID.
     *
     *  @param  parameters   GVariant object with the item specification
     *                       to retrieve
     *
     *  @return GVariant object with a value container of '(uuussb)'
     *          The container contains request type, group, id, variable name,
     *          description and a "no-echo" flag.
     *
     *  @throws RequiresQueueException when the queue is empty.
     **/
    GVariant *QueueFetchGVariant(GVariant *parameters) const;

    /**
     *  Updates the value for a RequiresSlot item
     *
     *  @param type      ClientAttentionType of the item to update
     *  @param group     ClientAttentionGroup the item belongs to
     *  @param id        Slot ID of the item to update
     *  @param newvalue  The new value for this item
     *
     *  @throws RequiresQueueException on errors
     */
    void UpdateEntry(ClientAttentionType type, ClientAttentionGroup group, unsigned int id, std::string newvalue);

    /**
     *  This is a variant UpdateEntry() extracting the item update information
     *  from a GVariant container.  This GVariant object must carry the data
     *  type '(uuus)' which refers to the request type, group, id and the string
     *  element contains the new value for the item.
     *
     *  @param indata     GVariant object containing the input data from
     *                    the D-Bus proxy caller
     *
     *  @throws RequiresQueueException on update errors
     */
    void UpdateEntry(GVariant *indata);

    /**
     *  Resets the value and the "provided flag" of an item already provided
     *
     * @param type   ClientAttentionType which the value is categorised under
     * @param group  ClientAttentionGroup which the value is categorised under
     * @param id     The numeric ID of the value slot to reset
     *
     * @throws RequiresQueueException on errors
     */
    void ResetValue(ClientAttentionType type,
                    ClientAttentionGroup group,
                    unsigned int id);

    /**
     *  Retrieve the value provided by a user, using request type, group and
     *  id as the lookup key.
     *
     * @param type   ClientAttentionType of the request
     * @param group  ClientAttentionGroup of the request type
     * @param id     Request ID
     *
     * @return Returns a string with the value if the value was found and
     *         provided by the user
     *
     * @throws RequiresQueueException if the user has not provided this
     *         any value or the request slot was not found.
     */
    const std::string GetResponse(ClientAttentionType type,
                                  ClientAttentionGroup group,
                                  unsigned int id) const;

    /**
     *  Retrieve the value provided by a user, using the assigned variable
     *  name to the request slot as lookup key
     *
     * @param type   ClientAttentionType of the request
     * @param group  ClientAttentionGroup of the request type
     * @param name   String with variable name of the value to retrieve
     *
     * @return Returns a string with the value if the value was found and
     *         provided by the user
     *
     * @throws RequiresQueueException if the user has not provided this
     *         any value or the request slot was not found.
     */
    const std::string GetResponse(ClientAttentionType type,
                                  ClientAttentionGroup group,
                                  std::string name) const;

    /**
     *  Retrieve the number of requires slots which have been prepared for
     *  a specific request type and group.
     *
     * @param type   ClientAttentionType of the requests
     * @param group  ClientAttentionGroup of the request type
     *
     * @return Returns the number of require slots prepared
     */
    unsigned int QueueCount(ClientAttentionType type,
                            ClientAttentionGroup group) const noexcept;

    /**
     *  Retrieve an array of all the request types and groups (as tuples)
     *  of all registered user requirements (require slots) which has not
     *  been satisfied yet.  I.e, where the user need to provide input.
     *
     *  This information only provides grouped information of what needs
     *  to be provided by the end-user.  The output of this method is to
     *  be used further by the QueueCheck() method.
     *
     * @return Returns a std::vector<ClientAttTypeGroup>
     *         of all type/groups not yet satisfied.
     */
    std::vector<ClientAttTypeGroup> QueueCheckTypeGroup() const noexcept;

    /**
     *  A GVariant version of QueueCheckTypeGroup().
     *
     *  Retrieve an array of all the request types and groups (as tuples)
     *  of all registered user requirements (require slots) which has not
     *  been satisfied yet.  I.e, where the user need to provide input.
     *
     * @return GVariant container object with the 'a(uu)' data type.
     *         This container carries an array of request type and group
     *         tuples which has not yet been satisfied by the end-user.
     */
    GVariant *QueueCheckTypeGroupGVariant() const noexcept;

    /**
     *  Retrieve an array of ID references of require slots which have not
     *  received any user responses.
     *
     * @param type   ClientAttentionType of the queue to check
     * @param group  ClientAttentionGroup of the queue to check
     *
     * @return Returns a std::vector<unsigned int> with IDs to variables
     *         still not been provided by the user
     */
    std::vector<unsigned int> QueueCheck(ClientAttentionType type,
                                         ClientAttentionGroup group) const noexcept;

    /**
     *  Retrieve an array of ID references of require slots which have not
     *  received any user responses.  This is a variant of QueueCheck() which
     *  takes the request type and group information and provides the result
     *  via GVariant container objects.
     *
     *  The input GVariant container must use the data type '(uu)' which
     *  contains the request type and group information.
     *
     *  The GVariant object returned uses the '(au)' data type, which is an
     *  array of unsigned integers; the integers references the ID of the
     *  request slot not yet satisfied by the end-user.
     *
     * @param parameters  GVariant value container object containing the
     *                    request type and group information.
     *
     * @return GVariant container object of the '(au)' data type.
     */
    GVariant *QueueCheckGVariant(GVariant *parameters) const noexcept;

    /**
     *  Checks if all registered requirement slots has been satisfied
     *  with a response by the end-user.
     *
     * @return Returns true if all requires slots has been provided with
     *         a value by the end-user, otherwise false.
     */
    bool QueueAllDone() const noexcept;

    /**
     *  Checks if a ClientAttentionType and ClientAttentionGroup is fully
     *  satisfied with all slots containing user provided values.
     *
     * @param type   ClientAttentionType to check
     * @param group  ClientAttentionGroup to check
     *
     * @return Returns true if all slots have valid responses.
     */
    bool QueueDone(ClientAttentionType type,
                   ClientAttentionGroup group);

    /**
     *  Checks if a ClientAttentionType and ClientAttentionGroup is fully
     *  satisfied with all slots containing user provided values.  This i
     *  a variant of QueueDone() which takes the request type and group
     *  information via a GVariant container object.
     *
     *  The input GVariant container must use the data type '(uu)' which
     *  contains the request type and group information.
     *
     * @param parameters  GVariant value container object containing the
     *                    request type and group information.
     *
     * @return Returns true if all requires slots has been provided with
     *         a value by the end-user, otherwise false.
     */
    bool QueueDone(GVariant *parameters);

protected:
    ///< Index counter map for type:group pairs; see get_reqid_index() for details
    std::map<unsigned int, unsigned int> reqids;

    ///< All gathered requests needed to be satisfied
    std::vector<struct RequiresSlot> slots;

    /**
     * Simple index hashing to be used by a single dimensional integer
     * array/table.  This is used to have a unique single ID per type:group,
     * used for a table keeping track of slot IDs used within each type:group.
     *
     * @param type   ClientAttentionType reference
     * @param group  ClientAttentionGroup reference
     *
     * @return  Returns a unique index based on the two input arguments
     */
    unsigned int get_reqid_index(ClientAttentionType type, ClientAttentionGroup group)
    {
        return ((unsigned int)type * 100) + (unsigned int)group;
    }


    RequiresQueue();
};
