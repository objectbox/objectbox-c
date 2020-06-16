/*
 * Copyright 2018-2020 ObjectBox Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "flatbuffers/flatbuffers.h"
#include "objectbox.h"

static_assert(sizeof(obx_id) == sizeof(OBX_id_array::ids[0]),
              "Can't directly link OBX_id_array.ids to std::vector<obx_id>::data()");

namespace obx {
class Exception : public std::runtime_error {
    const int code_;

public:
    explicit Exception(const std::string& text, int code = 0) : runtime_error(text), code_(code) {}
    explicit Exception(const char* text, int code = 0) : runtime_error(text), code_(code) {}
    int code() const { return code_; }
};

/// Transactions can be started in read (only) or write mode.
enum class TxMode { READ, WRITE };

namespace {
#define OBJECTBOX_VERIFY_ARGUMENT(c) \
    ((c) ? (void) (0) : throw std::invalid_argument(std::string("Argument validation failed: #c")))

#define OBJECTBOX_VERIFY_STATE(c) \
    ((c) ? (void) (0) : throw std::runtime_error(std::string("State condition failed: #c")))

[[noreturn]] void throwLastError() { throw Exception(obx_last_error_message(), obx_last_error_code()); }

void checkErrOrThrow(obx_err err) {
    if (err != OBX_SUCCESS) throwLastError();
}

void checkPtrOrThrow(void* ptr, const std::string& context) {
    if (!ptr) throw Exception(context + ": " + obx_last_error_message(), obx_last_error_code());
}
}  // namespace

template <class T>
class Box;

class Transaction;

// TODO maybe Box vending function?
class Store {
    OBX_store* cStore_;

public:
    struct Options {
        std::string directory;
        size_t maxDbSizeInKByte = 0;
        unsigned int fileMode = 0;
        unsigned int maxReaders = 0;
        OBX_model* model = nullptr;

        Options() = default;
        Options(OBX_model* obxModel) : model(obxModel) {}
    };

    explicit Store(OBX_model* model) : Store(Options(model)) {}

    explicit Store(const Options& options) {
        if (options.model) {  // check model error explicitly or it may be swallowed later
            obx_err err = obx_model_error_code(options.model);
            if (err) {
                const char* msg = obx_model_error_message(options.model);
                obx_model_free(options.model);
                throw Exception(msg, err);
            }
        }

        OBX_store_options* opt = obx_opt();
        checkPtrOrThrow(opt, "can't create store options");
        if (!options.directory.empty()) {
            if (obx_opt_directory(opt, options.directory.c_str()) != OBX_SUCCESS) {
                obx_opt_free(opt);
                throwLastError();
            }
        }
        if (options.maxDbSizeInKByte) obx_opt_max_db_size_in_kb(opt, options.maxDbSizeInKByte);
        if (options.fileMode) obx_opt_file_mode(opt, options.fileMode);
        if (options.maxReaders) obx_opt_max_readers(opt, options.maxReaders);
        if (options.model) {
            if (obx_opt_model(opt, options.model) != OBX_SUCCESS) {
                obx_opt_free(opt);
                obx_model_free(options.model);
                throwLastError();
            }
        }
        cStore_ = obx_store_open(opt);
        checkPtrOrThrow(cStore_, "can't open store");
    }

    explicit Store(OBX_store* cStore) : cStore_(cStore) { OBJECTBOX_VERIFY_ARGUMENT(cStore != nullptr); }

    virtual ~Store() { obx_store_close(cStore_); }

    OBX_store* cPtr() const { return cStore_; }

    template <class EntityBinding>
    Box<EntityBinding> box() {
        return Box<EntityBinding>(*this);
    }

    /// Starts a transaction using the given mode.
    Transaction tx(TxMode mode);

    /// Starts a read(-only) transaction.
    Transaction txRead();

    /// Starts a (read &) write transaction.
    Transaction txWrite();
};

/// Provides RAII wrapper for an active database transaction on the current thread (do not use across threads). A
/// Transaction object is considered a "top level transaction" if it is the first one on the call stack in the thread.
/// If the thread already has an ongoing Transaction, additional Transaction instances are considered "inner
/// transactions".
///
/// The top level transaction defines the actual transaction scope on the DB level. Internally, the top level
/// Transaction object manages (creates and destroys) a Transaction object. Inner transactions use the Transaction
/// object of the top level Transaction.
///
/// For write transactions, the top level call to success() actually commits the underlying Transaction. If inner
/// transactions are spawned, all of them must call success() in order for the top level transaction to be successful
/// and actually commit.
class Transaction {
    OBX_txn* cTxn_;
    TxMode mode_;

public:
    explicit Transaction(Store& store, TxMode mode)
        : mode_(mode), cTxn_(mode == TxMode::WRITE ? obx_txn_write(store.cPtr()) : obx_txn_read(store.cPtr())) {
        checkPtrOrThrow(cTxn_, "can't start transaction");
    }

    /// Never throws except some serious internal error occurred
    virtual ~Transaction() noexcept(false) { close(); };

    /// Delete because the default copy constructor can break things (i.e. a Transaction can not be copied).
    Transaction(Transaction&) = delete;

    /// Move constructor, used by Store::tx()
    Transaction(Transaction&& tx) noexcept : cTxn_(tx.cTxn_), mode_(tx.mode_) { tx.cTxn_ = nullptr; }

    /// A Transaction is active if it was not ended via success(), close() or moving.
    bool isActive() { return cTxn_ != nullptr; }

    /// The transaction pointer of the ObjectBox C API.
    /// @throws if this Transaction was already closed or moved
    OBX_txn* cPtr() const {
        OBJECTBOX_VERIFY_STATE(cTxn_);
        return cTxn_;
    }

    /// "Finishes" this write transaction successfully; performs a commit if this is the top level transaction and all
    /// inner transactions (if any) were also successful. This object will also be "closed".
    /// @throws Exception if this is not a write TX or it was closed before (e.g. via success()).
    void success() {
        OBX_txn* txn = cTxn_;
        OBJECTBOX_VERIFY_STATE(txn);
        cTxn_ = nullptr;
        checkErrOrThrow(obx_txn_success(txn));
    }

    /// Explicit close to free up resources - usually you can leave this to the destructor.
    /// It's OK to call this method multiple times; additional calls will have no effect.
    void close() {
        OBX_txn* txnToClose = cTxn_;
        cTxn_ = nullptr;
        if (txnToClose) {
            checkErrOrThrow(obx_txn_close(txnToClose));
        }
    }
};

Transaction Store::tx(TxMode mode) { return Transaction(*this, mode); }
Transaction Store::txRead() { return tx(TxMode::READ); }
Transaction Store::txWrite() { return tx(TxMode::WRITE); }

namespace {
/// Internal cursor wrapper for convenience and RAII.
class CursorTx {
    Transaction tx_;
    OBX_cursor* cCursor_;

public:
    explicit CursorTx(TxMode mode, Store& store, obx_schema_id entityId)
        : tx_(store, mode), cCursor_(obx_cursor(tx_.cPtr(), entityId)) {
        checkPtrOrThrow(cCursor_, "can't open a cursor");
    }

    virtual ~CursorTx() {
        if (cCursor_) obx_cursor_close(cCursor_);
    }

    void commitAndClose() {
        obx_cursor_close(cCursor_);
        cCursor_ = nullptr;
        tx_.success();
    }

    OBX_cursor* cPtr() const { return cCursor_; }
};

// FlatBuffer builder is reused so the allocated memory stays available for the future objects.
thread_local flatbuffers::FlatBufferBuilder fbb;
inline void fbbCleanAfterUse() {
    if (fbb.GetSize() > 1024 * 1024) fbb.Reset();
}
}  // namespace

template <typename EntityT>
class Query;

/// Provides a simple wrapper for OBX_query_builder to simplify memory management - calls obx_qb_close() on destruction.
/// To specify actual conditions, use obx_qb_*() methods with queryBuilder.cPtr() as the first argument.
template <typename EntityT>
class QueryBuilder {
    using EntityBinding = typename EntityT::_OBX_MetaInfo;
    OBX_query_builder* cQueryBuilder_;

public:
    explicit QueryBuilder(Store& store) : QueryBuilder(obx_query_builder(store.cPtr(), EntityBinding::entityId())) {}

    /// Take ownership of an OBX_query_builder.
    /// @example
    ///          QueryBuilder innerQb(obx_qb_link_property(outerQb.cPtr(), linkPropertyId))
    explicit QueryBuilder(OBX_query_builder* ptr) : cQueryBuilder_(ptr) {
        checkPtrOrThrow(cQueryBuilder_, "can't create a query builder");
    }

    virtual ~QueryBuilder() { obx_qb_close(cQueryBuilder_); }

    OBX_query_builder* cPtr() const { return cQueryBuilder_; }

    Query<EntityT> build();
};

/// Provides a simple wrapper for OBX_query to simplify memory management - calls obx_query_close() on destruction.
/// To execute the actual methods, use obx_query_*() methods with query.cPtr() as the first argument.
/// Internal note: this is a template because it will provide EntityType-specific methods in the future
template <typename EntityT>
class Query {
    OBX_query* cQuery_;

public:
    /// Builds a query with the parameters specified by the builder
    explicit Query(OBX_query_builder* qb) : cQuery_(obx_query(qb)) { checkPtrOrThrow(cQuery_, "can't build a query"); }

    virtual ~Query() { obx_query_close(cQuery_); }

    OBX_query* cPtr() const { return cQuery_; }
};

template <typename EntityT>
Query<EntityT> QueryBuilder<EntityT>::build() {
    return Query<EntityT>(cQueryBuilder_);
}

template <typename EntityT>
class Box {
    using EntityBinding = typename EntityT::_OBX_MetaInfo;

    OBX_box* cBox_;
    Store& store_;

public:
    explicit Box(Store& store) : store_(store), cBox_(obx_box(store.cPtr(), EntityBinding::entityId())) {
        checkPtrOrThrow(cBox_, "can't create box");
    }

    OBX_box* cPtr() const { return cBox_; }

    QueryBuilder<EntityT> query() { return QueryBuilder<EntityT>(store_); }

    /// Return the number of objects contained by this box.
    /// @param limit if provided: stop counting at the given limit - useful if you need to make sure the Box has "at
    /// least" this many objects but you don't need to know the exact number.
    uint64_t count(uint64_t limit = 0) {
        uint64_t result;
        checkErrOrThrow(obx_box_count(cBox_, limit, &result));
        return result;
    }

    /// Returns true if the box contains no objects.
    bool isEmpty() {
        bool result;
        checkErrOrThrow(obx_box_is_empty(cBox_, &result));
        return result;
    }

    /// Checks whether this box contains an object with the given ID.
    bool contains(obx_id id) {
        bool result;
        checkErrOrThrow(obx_box_contains(cBox_, id, &result));
        return result;
    }

    /// Checks whether this box contains all objects matching the given IDs.
    bool contains(const std::vector<obx_id>& ids) {
        if (ids.empty()) return true;

        bool result;
        const OBX_id_array cIds = cIdArrayRef(ids);
        checkErrOrThrow(obx_box_contains_many(cBox_, &cIds, &result));
        return result;
    }

    /// Read an object from the database, returning a managed pointer.
    /// @return an object pointer or nullptr if the object doesn't exist.
    std::unique_ptr<EntityT> get(obx_id id) {
        auto object = std::unique_ptr<EntityT>(new EntityT());
        if (!get(id, *object)) return nullptr;
        return object;
    }

    /// Read an object from the database, replacing the contents of an existing object variable.
    /// @return true on success, false if the ID was not found, in which case outObject is untouched.
    bool get(obx_id id, EntityT& outObject) {
        CursorTx cursor(TxMode::READ, store_, EntityBinding::entityId());
        void* data;
        size_t size;
        obx_err err = obx_cursor_get(cursor.cPtr(), id, &data, &size);
        if (err == OBX_NOT_FOUND) return false;
        checkErrOrThrow(err);
        EntityBinding::fromFlatBuffer(data, size, outObject);
        return true;
    }

    /// Read multiple objects at once, i.e. in a single read transaction.
    /// @return a vector of object pointers index-matching the given ids. In case some objects are
    /// not found, it's position in the result will be NULL, thus the result will always have the
    /// same size as the given ids argument.
    /// @todo do we want to provide `get(..., std::vector<EntityT>& outObjects)` or change this to return such vector?
    ///       That would be less cumbersome to use in combination with put() calls accepting such a vector.
    std::vector<std::unique_ptr<EntityT>> get(const std::vector<obx_id>& ids) {
        std::vector<std::unique_ptr<EntityT>> result;
        result.resize(ids.size());  // allocate all unique_ptr with NULL contents

        CursorTx cursor(TxMode::READ, store_, EntityBinding::entityId());
        void* data;
        size_t size;

        for (size_t i = 0; i < ids.size(); i++) {
            obx_err err = obx_cursor_get(cursor.cPtr(), ids[i], &data, &size);
            if (err == OBX_NOT_FOUND) continue;  // leave nullptr at result[i] in this case
            checkErrOrThrow(err);

            result[i].reset(new EntityT());
            EntityBinding::fromFlatBuffer(data, size, *(result[i]));
        }

        return result;
    }

    /// Read all objects from the Box at once, i.e. in a single read transaction.
    /// @todo do we want to provide `getAll(std::vector<EntityT>& outObjects)` or change this to return such vector?
    std::vector<std::unique_ptr<EntityT>> getAll() {
        std::vector<std::unique_ptr<EntityT>> result;

        CursorTx cursor(TxMode::READ, store_, EntityBinding::entityId());
        void* data;
        size_t size;

        obx_err err = obx_cursor_first(cursor.cPtr(), &data, &size);
        while (err == OBX_SUCCESS) {
            result.emplace_back(new EntityT());
            EntityBinding::fromFlatBuffer(data, size, *(result[result.size() - 1]));
            err = obx_cursor_next(cursor.cPtr(), &data, &size);
        }
        if (err != OBX_NOT_FOUND) checkErrOrThrow(err);

        return result;
    }

    /// Inserts or updates the given object in the database.
    /// @param object will be updated with a newly inserted ID if the one specified previously was zero. If an ID was
    /// already specified (non-zero), it will remain unchanged.
    /// @return object ID from the object param (see object param docs).
    obx_id put(EntityT& object, OBXPutMode mode = OBXPutMode_PUT) {
        obx_id id = put(const_cast<const EntityT&>(object), mode);
        EntityBinding::setObjectId(object, id);
        return id;
    }

    /// Inserts or updates the given object in the database.
    /// @return newly assigned object ID in case this was an insert, otherwise the original ID from the object param.
    obx_id put(const EntityT& object, OBXPutMode mode = OBXPutMode_PUT) {
        EntityBinding::toFlatBuffer(fbb, object);
        obx_id id = obx_box_put_object(cBox_, fbb.GetBufferPointer(), fbb.GetSize(), mode);
        fbbCleanAfterUse();
        if (id == 0) throwLastError();
        return id;
    }

    /// Puts multiple objects using a single transaction. In case there was an error the transaction is rolled back and
    /// none of the changes are persisted.
    /// @param objects objects to insert (if their IDs are zero) or update
    /// @param outIds may be provided to collect IDs from the objects. This collects not only new IDs assigned after an
    /// insert but also existing IDs if an object isn't new but updated instead. Thus, the outIds vector will always end
    /// up with the same number of items as objects argument, with indexes corresponding between the two. Note: outIds
    /// content is reset before executing to make sure the indexes match the objects argument even if outIds is reused
    /// between multiple calls.
    /// @throws reverts the changes if an error occurs.
    void put(std::vector<EntityT>& objects, std::vector<obx_id>* outIds = nullptr, OBXPutMode mode = OBXPutMode_PUT) {
        // Even if outIds is not provided, we need a vector to store IDs to update objects AFTER the TX is committed.
        std::unique_ptr<std::vector<obx_id>> tmpIds;
        if (!outIds) {
            tmpIds.reset(new std::vector<obx_id>());
            outIds = tmpIds.get();
        }

        put(const_cast<const std::vector<EntityT>&>(objects), outIds, mode);

        // Update objects after the commit. Note: there's nothing we can do if this is all executed in an outer
        // transaction that eventually may fail, it's up to the user to handle that...
        assert(objects.size() == outIds->size());
        for (size_t i = 0; i < objects.size(); i++) {
            EntityBinding::setObjectId(objects[i], outIds->at(i));
        }
    }

    /// Puts multiple objects using a single transaction. In case there was an error the transaction is rolled back and
    /// none of the changes are persisted.
    /// @param objects objects to insert (if their IDs are zero) or update
    /// @param outIds may be provided to collect IDs from the objects. This collects not only new IDs assigned after an
    /// insert but also existing IDs if an object isn't new but updated instead. Thus, the outIds vector will always end
    /// up with the same number of items as objects argument, with indexes corresponding between the two. Note: outIds
    /// content is reset before executing to make sure the indexes match the objects argument even if outIds is reused
    /// between multiple calls.
    /// @throws reverts the changes if an error occurs.
    void put(const std::vector<EntityT>& objects, std::vector<obx_id>* outIds = nullptr,
             OBXPutMode mode = OBXPutMode_PUT) {
        if (outIds) {
            outIds->clear();
            outIds->reserve(objects.size());
        }

        // Don't start a TX in case there's no data.
        // Note: Don't move this above clearing outIds vector - our contract says that we clear outIds before starting
        // execution so we must do it even if no objects were passed.
        if (objects.empty()) return;

        CursorTx cursor(TxMode::WRITE, store_, EntityBinding::entityId());
        for (auto& object : objects) {
            obx_id id = cursorPut(cursor, object, mode);
            if (outIds) outIds->push_back(id);
        }
        cursor.commitAndClose();
        fbbCleanAfterUse();  // NOTE might not get called in case of an exception
    }

    /// Remove the object with the given id
    /// @returns whether the object was removed or not (because it didn't exist)
    bool remove(obx_id id) {
        obx_err err = obx_box_remove(cBox_, id);
        if (err == OBX_NOT_FOUND) return false;
        checkErrOrThrow(err);
        return true;
    }

    /// Removes all objects matching the given IDs
    /// @returns number of removed objects between 0 and ids.size() (if all IDs existed)
    uint64_t remove(const std::vector<obx_id>& ids) {
        uint64_t result = 0;
        const OBX_id_array cIds = cIdArrayRef(ids);
        checkErrOrThrow(obx_box_remove_many(cBox_, &cIds, &result));
        return result;
    }

    /// Removes all objects from the box
    /// @returns the number of removed objects
    uint64_t removeAll() {
        uint64_t result = 0;
        checkErrOrThrow(obx_box_remove_all(cBox_, &result));
        return result;
    }

    /// Fetch IDs of all objects in this box that reference the given object (ID) on the given relation property.
    /// Note: This method refers to "property based relations" unlike the "stand-alone relations" (Box::standaloneRel*).
    /// @param propertyId the relation property, which must belong to the entity type represented by this box.
    /// @param objectId this relation points to - typically ID of an object of another entity type (another box).
    /// @returns resulting IDs representing objects in this Box, or NULL in case of an error
    /// @example Let's say you have the following two entities with a relation between them (.fbs file format):
    ///          table Customer {
    ///              id:ulong;
    ///              name:string;
    ///              ...
    ///          }
    ///          table Order {
    ///              id:ulong;
    ///              /// objectbox:link=Customer
    ///              customerId:ulong;
    ///              ...
    ///          }
    ///          Now, you can use this method to get all orders for a given customer (e.g. 42):
    ///          obx_id customerId = 42;
    ///          Box<Order_> orderBox(store);
    ///          std::vector<obx_id> customerOrders = orderBox.backlinkIds(Order_.customerId, 42);
    /// @todo hint the propertyId by using an enum class in the generated coe - drawback - needs casts in queries...
    std::vector<obx_id> backlinkIds(obx_schema_id propertyId, obx_id objectId) {
        return idVectorOrThrow(obx_box_get_backlink_ids(cBox_, propertyId, objectId));
    }

    /// Replace the list of standalone relation target objects on the given source object.
    /// @note standalone relations are currently not supported by objectbox-cgen code generator
    /// @param relationId must be a standalone relation ID with source object entity belonging to this box
    /// @param sourceObjectId identifies an object from this box
    /// @param targetObjectIds identifies objects from a target box (as per the relation definition)
    /// @todo consider providing a method similar to the one in Go - inserting target objects with 0 IDs
    void standaloneRelReplace(obx_schema_id relationId, obx_id sourceObjectId,
                              const std::vector<obx_id>& targetObjectIds) {
        // we use set_difference below so need to work on sorted vectors, thus we need to make a copy
        auto newIds = targetObjectIds;
        std::sort(newIds.begin(), newIds.end());

        Transaction tx(store_, TxMode::WRITE);

        auto oldIds = standaloneRelIds(relationId, sourceObjectId);
        std::sort(oldIds.begin(), oldIds.end());

        // find IDs to remove, i.e. those that previously were present and aren't anymore
        std::vector<obx_id> diff;
        std::set_difference(oldIds.begin(), oldIds.end(), newIds.begin(), newIds.end(),
                            std::inserter(diff, diff.begin()));

        for (obx_id targetId : diff) {
            standaloneRelRemove(relationId, sourceObjectId, targetId);
        }
        diff.clear();

        // find IDs to insert, i.e. those that previously weren't present and are now
        std::set_difference(newIds.begin(), newIds.end(), oldIds.begin(), oldIds.end(),
                            std::inserter(diff, diff.begin()));
        for (obx_id targetId : diff) {
            standaloneRelPut(relationId, sourceObjectId, targetId);
        }

        tx.success();
    }

    /// Insert a standalone relation entry between two objects.
    /// @note standalone relations are currently not supported by objectbox-cgen code generator
    /// @param relationId must be a standalone relation ID with source object entity belonging to this box
    /// @param sourceObjectId identifies an object from this box
    /// @param targetObjectId identifies an object from a target box (as per the relation definition)
    void standaloneRelPut(obx_schema_id relationId, obx_id sourceObjectId, obx_id targetObjectId) {
        checkErrOrThrow(obx_box_rel_put(cBox_, relationId, sourceObjectId, targetObjectId));
    }

    /// Remove a standalone relation entry between two objects.
    /// @note standalone relations are currently not supported by objectbox-cgen code generator
    /// @param relationId must be a standalone relation ID with source object entity belonging to this box
    /// @param sourceObjectId identifies an object from this box
    /// @param targetObjectId identifies an object from a target box (as per the relation definition)
    void standaloneRelRemove(obx_schema_id relationId, obx_id sourceObjectId, obx_id targetObjectId) {
        checkErrOrThrow(obx_box_rel_remove(cBox_, relationId, sourceObjectId, targetObjectId));
    }

    /// Fetch IDs of all objects in this Box related to the given object (typically from another Box).
    /// Used for a stand-alone relation and its "regular" direction; this Box represents the target of the relation.
    /// @note standalone relations are currently not supported by objectbox-cgen code generator
    /// @param relationId ID of a standalone relation, whose target type matches this Box
    /// @param objectId object ID of the relation source type (typically from another Box)
    /// @returns resulting IDs representing objects in this Box
    /// @todo improve docs by providing an example with a clear distinction between source and target type
    std::vector<obx_id> standaloneRelIds(obx_schema_id relationId, obx_id objectId) {
        return idVectorOrThrow(obx_box_rel_get_ids(cBox_, relationId, objectId));
    }

    /// Fetch IDs of all objects in this Box related to the given object (typically from another Box).
    /// Used for a stand-alone relation and its "backlink" direction; this Box represents the source of the relation.
    /// @note standalone relations are currently not supported by objectbox-cgen code generator
    /// @param relationId ID of a standalone relation, whose source type matches this Box
    /// @param objectId object ID of the relation target type (typically from another Box)
    /// @returns resulting IDs representing objects in this Box
    /// @todo improve docs by providing an example with a clear distinction between source and target type
    std::vector<obx_id> standaloneRelBacklinkIds(obx_schema_id relationId, obx_id objectId) {
        return idVectorOrThrow(obx_box_rel_get_backlink_ids(cBox_, relationId, objectId));
    }

private:
    obx_id cursorPut(CursorTx& cursor, const EntityT& object, OBXPutMode mode = OBXPutMode_PUT) {
        EntityBinding::toFlatBuffer(fbb, object);
        // TODO we need to extend obx_cursor_put_object() with mode argument to be able to use it (it's faster here)
        // obx_id id = obx_cursor_put_object(cursor.cPtr(), fbb.GetBufferPointer(), fbb.GetSize());
        obx_id id = obx_box_put_object(cBox_, fbb.GetBufferPointer(), fbb.GetSize(), mode);
        if (id == 0) throwLastError();
        // NOTE: do NOT use EntityBinding::setObjectId(object, id) here - we're in a TX which may
        // get rolled back at a later point.
        return id;
    }

    /// Produces an OBX_id_array with internal data referencing the given ids vector. You must
    /// ensure the given vector outlives the returned OBX_id_array. Additionally, you must NOT call
    /// obx_id_array_free(), because the result is not allocated by C, thus it must not free it.
    OBX_id_array cIdArrayRef(const std::vector<obx_id>& ids) {
        return {.ids = ids.empty() ? nullptr : const_cast<obx_id*>(ids.data()), .count = ids.size()};
    }

    /// Consumes an OBX_id_array, producing a vector of IDs and freeing the array afterwards.
    /// Note: even if this function throws the given OBX_id_array is freed.
    std::vector<obx_id> idVectorOrThrow(OBX_id_array* cIds) {
        if (!cIds) throwLastError();

        try {
            std::vector<obx_id> result;
            if (cIds->count > 0) {
                result.resize(cIds->count);
                OBJECTBOX_VERIFY_STATE(result.size() == cIds->count);
                memcpy(result.data(), cIds->ids, result.size() * sizeof(result[0]));
            }
            obx_id_array_free(cIds);
            return result;
        } catch (...) {
            obx_id_array_free(cIds);
            throw;
        }
    }
};

}  // namespace obx