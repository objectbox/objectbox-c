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
#ifdef __cpp_lib_optional
#include <optional>
#endif

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

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    Store(const Store&) = delete;

    Store(Store&& source) noexcept : cStore_(source.cStore_) { source.cStore_ = nullptr; }

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
    Transaction(const Transaction&) = delete;

    /// Move constructor, used by Store::tx()
    Transaction(Transaction&& source) noexcept : cTxn_(source.cTxn_), mode_(source.mode_) { source.cTxn_ = nullptr; }

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

inline Transaction Store::tx(TxMode mode) { return Transaction(*this, mode); }
inline Transaction Store::txRead() { return tx(TxMode::READ); }
inline Transaction Store::txWrite() { return tx(TxMode::WRITE); }

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

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    CursorTx(const CursorTx&) = delete;

    CursorTx(CursorTx&& source) noexcept : cCursor_(source.cCursor_), tx_(std::move(source.tx_)) {
        source.cCursor_ = nullptr;
    }

    virtual ~CursorTx() { obx_cursor_close(cCursor_); }

    void commitAndClose() {
        OBJECTBOX_VERIFY_STATE(cCursor_ != nullptr);
        obx_cursor_close(cCursor_);
        cCursor_ = nullptr;
        tx_.success();
    }

    OBX_cursor* cPtr() const { return cCursor_; }
};

/// Collects all visited data
template <typename EntityT>
struct CollectingVisitor {
    std::vector<std::unique_ptr<EntityT>> items;

    static bool visit(void* ptr, const void* data, size_t size) {
        auto self = reinterpret_cast<CollectingVisitor<EntityT>*>(ptr);
        self->items.emplace_back(new EntityT());
        EntityT::_OBX_MetaInfo::fromFlatBuffer(data, size, *(self->items.back()));
        return true;
    }
};

/// Produces an OBX_id_array with internal data referencing the given ids vector. You must
/// ensure the given vector outlives the returned OBX_id_array. Additionally, you must NOT call
/// obx_id_array_free(), because the result is not allocated by C, thus it must not free it.
OBX_id_array cIdArrayRef(const std::vector<obx_id>& ids) {
    return {.ids = ids.empty() ? nullptr : const_cast<obx_id*>(ids.data()), .count = ids.size()};
}

/// Consumes an OBX_id_array, producing a vector of IDs and freeing the array afterwards.
/// Must be called right after the C-API call producing cIds in order to check and throw on error correctly.
/// Example: idVectorOrThrow(obx_query_find_ids(cQuery_, offset_, limit_))
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
    Store& store_;

public:
    explicit QueryBuilder(Store& store)
        : QueryBuilder(store, obx_query_builder(store.cPtr(), EntityBinding::entityId())) {}

    /// Take ownership of an OBX_query_builder.
    /// @example
    ///          QueryBuilder innerQb(obx_qb_link_property(outerQb.cPtr(), linkPropertyId))
    explicit QueryBuilder(Store& store, OBX_query_builder* ptr) : store_(store), cQueryBuilder_(ptr) {
        checkPtrOrThrow(cQueryBuilder_, "can't create a query builder");
    }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    QueryBuilder(const QueryBuilder&) = delete;

    QueryBuilder(QueryBuilder&& source) noexcept : cQueryBuilder_(source.cQueryBuilder_), store_(source.store_) {
        source.cQueryBuilder_ = nullptr;
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
    Store& store_;

    // let's simulate what the objectbox-c API should do later - keeping offset and limit part of the query object
    size_t offset_ = 0;
    size_t limit_ = 0;

public:
    /// Builds a query with the parameters specified by the builder
    explicit Query(Store& store, OBX_query_builder* qb) : cQuery_(obx_query(qb)), store_(store) {
        checkPtrOrThrow(cQuery_, "can't build a query");
    }

    /// Clones the query
    Query(const Query& query)
        : cQuery_(obx_query_clone(query.cQuery_)), store_(query.store_), offset_(query.offset_), limit_(query.limit_) {
        checkPtrOrThrow(cQuery_, "couldn't make a query clone");
    }

    Query(Query&& source) noexcept : cQuery_(source.cQuery_), store_(source.store_) { source.cQuery_ = nullptr; }

    virtual ~Query() { obx_query_close(cQuery_); }

    OBX_query* cPtr() const { return cQuery_; }

    /// Sets an offset of what items to start at.
    /// This offset is stored for any further calls on the query until changed.
    Query& offset(size_t offset) {
        offset_ = offset;
        return *this;
    }

    /// Sets a limit on the number of processed items.
    /// This limit is stored for any further calls on the query until changed.
    Query& limit(size_t limit) {
        limit_ = limit;
        return *this;
    }

    /// Finds all objects matching the query.
    /// @todo returning vector of pointers isn't needed, we could just return vector of objects but we're keeping the
    ///       interface consistent with box for now. Should be changed together though.
    std::vector<std::unique_ptr<EntityT>> find() {
        OBJECTBOX_VERIFY_STATE(cQuery_ != nullptr);

        CollectingVisitor<EntityT> visitor;
        obx_query_visit(cQuery_, CollectingVisitor<EntityT>::visit, &visitor, offset_, limit_);
        return std::move(visitor.items);
    }

    /// Returns IDs of all matching objects.
    std::vector<obx_id> findIds() { return idVectorOrThrow(obx_query_find_ids(cQuery_, offset_, limit_)); }

    /// Returns the number of matching objects.
    uint64_t count() {
        if (offset_ || limit_) {
            throw std::logic_error("Query limit/offset are not supported by count() at this moment");
        }
        uint64_t result;
        checkErrOrThrow(obx_query_count(cQuery_, &result));
        return result;
    }

    /// Removes all matching objects from the database & returns the number of deleted objects.
    size_t remove() {
        if (offset_ || limit_) {
            throw std::logic_error("Query limit/offset are not supported by remove() at this moment");
        }
        uint64_t result;
        checkErrOrThrow(obx_query_remove(cQuery_, &result));
        return result;
    }
};

template <typename EntityT>
inline Query<EntityT> QueryBuilder<EntityT>::build() {
    return Query<EntityT>(store_, cQueryBuilder_);
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
    /// @return an object pointer or nullptr if an object with the given ID doesn't exist.
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

#ifdef __cpp_lib_optional
    /// Read an object from the database.
    /// @return an "optional" wrapper of the object; empty if an object with the given ID doesn't exist.
    std::optional<EntityT> getOptional(obx_id id) {
        CursorTx cursor(TxMode::READ, store_, EntityBinding::entityId());
        void* data;
        size_t size;
        obx_err err = obx_cursor_get(cursor.cPtr(), id, &data, &size);
        if (err == OBX_NOT_FOUND) return std::nullopt;
        checkErrOrThrow(err);
        return EntityBinding::fromFlatBuffer(data, size);
    }
#endif

    /// Read multiple objects at once, i.e. in a single read transaction.
    /// @return a vector of object pointers index-matching the given ids. In case some objects are
    /// not found, it's position in the result will be NULL, thus the result will always have the
    /// same size as the given ids argument.
    std::vector<std::unique_ptr<EntityT>> get(const std::vector<obx_id>& ids) {
        return getMany<std::unique_ptr<EntityT>>(ids);
    }

#ifdef __cpp_lib_optional
    /// Read multiple objects at once, i.e. in a single read transaction.
    /// @return a vector of object pointers index-matching the given ids. In case some objects are
    /// not found, it's position in the result will be empty, thus the result will always have the
    /// same size as the given ids argument.
    std::vector<std::optional<EntityT>> getOptional(const std::vector<obx_id>& ids) {
        return getMany<std::optional<EntityT>>(ids);
    }
#endif

    /// Read all objects from the Box at once, i.e. in a single read transaction.
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
    /// @param objects objects to insert (if their IDs are zero) or update. ID properties on the newly inserted objects
    /// will be updated. If the transaction fails, the assigned IDs on the given objects will be incorrect.
    /// @param outIds may be provided to collect IDs from the objects. This collects not only new IDs assigned after an
    /// insert but also existing IDs if an object isn't new but updated instead. Thus, the outIds vector will always end
    /// up with the same number of items as objects argument, with indexes corresponding between the two. Note: outIds
    /// content is reset before executing to make sure the indexes match the objects argument even if outIds is reused
    /// between multiple calls.
    /// @throws reverts the changes if an error occurs.
    void put(std::vector<EntityT>& objects, std::vector<obx_id>* outIds = nullptr, OBXPutMode mode = OBXPutMode_PUT) {
        putMany(objects, outIds, mode);
    }

    /// @overload
    void put(const std::vector<EntityT>& objects, std::vector<obx_id>* outIds = nullptr,
             OBXPutMode mode = OBXPutMode_PUT) {
        putMany(objects, outIds, mode);
    }
    /// @overload
    void put(std::vector<std::unique_ptr<EntityT>>& objects, std::vector<obx_id>* outIds = nullptr,
             OBXPutMode mode = OBXPutMode_PUT) {
        putMany(objects, outIds, mode);
    }

#ifdef __cpp_lib_optional
    /// @overload
    void put(std::vector<std::optional<EntityT>>& objects, std::vector<obx_id>* outIds = nullptr,
             OBXPutMode mode = OBXPutMode_PUT) {
        putMany(objects, outIds, mode);
    }
#endif

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
    /// @todo hint the propertyId by using an enum class in the generated coe - drawback - needs casts in obx_qb_*()
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
    template <typename Vector>
    void putMany(Vector& objects, std::vector<obx_id>* outIds, OBXPutMode mode) {
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
            obx_id id = cursorPut(cursor, object, mode);  // type-based overloads here
            if (outIds) outIds->push_back(id);
        }
        cursor.commitAndClose();
        fbbCleanAfterUse();  // NOTE might not get called in case of an exception
    }

    obx_id cursorPut(CursorTx& cursor, const EntityT& object, OBXPutMode mode) {
        EntityBinding::toFlatBuffer(fbb, object);
        obx_id id = obx_cursor_put_object4(cursor.cPtr(), fbb.GetBufferPointer(), fbb.GetSize(), mode);
        if (id == 0) throwLastError();
        return id;
    }

    obx_id cursorPut(CursorTx& cursor, EntityT& object, OBXPutMode mode) {
        obx_id id = cursorPut(cursor, const_cast<const EntityT&>(object), mode);
        EntityBinding::setObjectId(object, id);
        return id;
    }

    obx_id cursorPut(CursorTx& cursor, const std::unique_ptr<EntityT>& object, OBXPutMode mode) {
        OBJECTBOX_VERIFY_ARGUMENT(object != nullptr);  // TODO or should we just skip such objects?
        return cursorPut(cursor, *object, mode);
    }

#ifdef __cpp_lib_optional
    obx_id cursorPut(CursorTx& cursor, std::optional<EntityT>& object, OBXPutMode mode) {
        OBJECTBOX_VERIFY_ARGUMENT(object.has_value());  // TODO or should we just skip such objects?
        return cursorPut(cursor, *object, mode);
    }
#endif

    template <typename Item>
    std::vector<Item> getMany(const std::vector<obx_id>& ids) {
        std::vector<Item> result;
        result.resize(ids.size());  // prepare empty/nullptr pointers in the output

        CursorTx cursor(TxMode::READ, store_, EntityBinding::entityId());
        void* data;
        size_t size;

        for (size_t i = 0; i < ids.size(); i++) {
            obx_err err = obx_cursor_get(cursor.cPtr(), ids[i], &data, &size);
            if (err == OBX_NOT_FOUND) continue;  // leave empty at result[i] in this case
            checkErrOrThrow(err);
            readFromFb(result[i], data, size);
        }

        return result;
    }

    void readFromFb(std::unique_ptr<EntityT>& object, void* data, size_t size) {
        object = EntityBinding::newFromFlatBuffer(data, size);
    }

#ifdef __cpp_lib_optional
    void readFromFb(std::optional<EntityT>& object, void* data, size_t size) {
        object = EntityT();
        assert(object.has_value());
        EntityBinding::fromFlatBuffer(data, size, *object);
    }
#endif
};

}  // namespace obx