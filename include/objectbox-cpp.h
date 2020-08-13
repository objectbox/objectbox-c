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

/**
 * @defgroup cpp ObjectBox C++ API
 * @{
 */

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
    ((c) ? (void) (0) : throw std::invalid_argument(std::string("Argument validation failed: " #c)))

#define OBJECTBOX_VERIFY_STATE(c) \
    ((c) ? (void) (0) : throw std::runtime_error(std::string("State condition failed: " #c)))

[[noreturn]] void throwLastError() { throw Exception(obx_last_error_message(), obx_last_error_code()); }

void checkErrOrThrow(obx_err err) {
    if (err != OBX_SUCCESS) throwLastError();
}

template <typename T>
T* checkPtrOrThrow(T* ptr, const std::string& context) {
    if (!ptr) throw Exception(context + ": " + obx_last_error_message(), obx_last_error_code());
    return ptr;
}

template <typename EntityT>
constexpr obx_schema_id entityId() {
    return EntityT::_OBX_MetaInfo::entityId();
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
    return {ids.empty() ? nullptr : const_cast<obx_id*>(ids.data()), ids.size()};
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

class QueryCondition;

namespace {
class QCGroup;
}  // namespace

class QueryCondition {
public:
    // We're using pointers so have a virtual destructor to ensure proper destruction of the derived classes.
    virtual ~QueryCondition() = default;

    virtual QCGroup and_(const QueryCondition& other);

    QCGroup operator&&(const QueryCondition& rhs);

    virtual QCGroup or_(const QueryCondition& other);

    QCGroup operator||(const QueryCondition& rhs);

protected:
    virtual obx_qb_cond applyTo(OBX_query_builder* cqb, bool isRoot) const = 0;

    /// Allows Box<EntityT> call the private applyTo() when creating a query.
    /// Partial specialization template friendship is not possible: `template <class T> friend class Box`.
    /// Therefore, we create an internal function that does the job for us.
    /// Note: this is actually not a method but an inline function and only seems visible in the current file.
    friend obx_qb_cond internalApplyCondition(const QueryCondition& condition, OBX_query_builder* cqb, bool isRoot) {
        return condition.applyTo(cqb, isRoot);
    }

    /// Returns a copy (of the concrete class) as unique pointer. Used when grouping conditions together (AND|OR).
    virtual std::unique_ptr<QueryCondition> copyAsPtr() const = 0;

    /// Inlined friend function to call the protected copyAsPtr()
    friend std::unique_ptr<QueryCondition> internalCopyAsPtr(const QueryCondition& condition) {
        return condition.copyAsPtr();
    }

    template <class Derived>
    static std::unique_ptr<QueryCondition> copyAsPtr(const Derived& object) {
        return std::unique_ptr<QueryCondition>(new Derived(object));
    }
};

namespace {  // internal
class QCGroup : public QueryCondition {
    bool isOr;  // whether it's AND or OR group

    // Must be a vector of pointers because QueryCondition is abstract - we can't have a vector of abstract objects.
    // Must be shared_ptr for our own copyAsPtr() to work, in other words vector of unique pointers can't be copied.
    std::vector<std::shared_ptr<QueryCondition>> conditions;

public:
    QCGroup(bool isOr, std::unique_ptr<QueryCondition>&& a, std::unique_ptr<QueryCondition>&& b)
        : isOr(isOr), conditions({std::move(a), std::move(b)}) {}

    // override to combine multiple chained AND conditions into a same group
    QCGroup and_(const QueryCondition& other) override {
        // if this group is an OR group and we're adding an AND - create a new group
        if (isOr) return QueryCondition::and_(other);

        // otherwise, extend this one by making a copy and including the new condition in it
        return copyThisAndPush(other);
    }

    // we don't have to create copies of the QCGroup when the left-hand-side can be "consumed and moved" (rvalue ref)
    // Note: this is a "global" function, but declared here as a friend so it can access lhs.conditions
    friend inline QCGroup operator&&(QCGroup&& lhs, const QueryCondition& rhs) {
        if (lhs.isOr) return lhs.and_(rhs);
        lhs.conditions.push_back(internalCopyAsPtr(rhs));
        return std::move(lhs);
    }

    // override to combine multiple chained OR conditions into a same group
    QCGroup or_(const QueryCondition& other) override {
        // if this group is an AND group and we're adding an OR - create a new group
        if (!isOr) return QueryCondition::or_(other);

        // otherwise, extend this one by making a copy and including the new condition in it
        return copyThisAndPush(other);
    }

    // we don't have to create copies of the QCGroup when the left-hand-side can be "consumed and moved" (rvalue ref)
    // Note: this is a "global" function, but declared here as a friend so it can access lhs.conditions
    friend inline QCGroup operator||(QCGroup&& lhs, const QueryCondition& rhs) {
        if (!lhs.isOr) return lhs.or_(rhs);
        lhs.conditions.push_back(internalCopyAsPtr(rhs));
        return std::move(lhs);
    }

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override {
        return std::unique_ptr<QueryCondition>(new QCGroup(*this));
    };

    QCGroup copyThisAndPush(const QueryCondition& other) {
        QCGroup copy(*this);
        copy.conditions.push_back(internalCopyAsPtr(other));
        return copy;
    }

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool isRoot) const override {
        if (conditions.size() == 1) return internalApplyCondition(*conditions[0], cqb, isRoot);
        OBJECTBOX_VERIFY_STATE(conditions.size() > 0);

        std::vector<obx_qb_cond> cond_ids;
        cond_ids.reserve(conditions.size());
        for (const std::shared_ptr<QueryCondition>& cond : conditions) {
            cond_ids.emplace_back(internalApplyCondition(*cond, cqb, false));
        }
        if (isRoot && !isOr) {
            // root All (AND) is implicit so no need to actually combine the conditions explicitly
            return 0;
        }

        if (isOr) return obx_qb_any(cqb, cond_ids.data(), cond_ids.size());
        return obx_qb_all(cqb, cond_ids.data(), cond_ids.size());
    }
};

}  // namespace

QCGroup obx::QueryCondition::and_(const QueryCondition& other) {
    return {false, copyAsPtr(), internalCopyAsPtr(other)};
}
QCGroup obx::QueryCondition::operator&&(const QueryCondition& rhs) { return and_(rhs); }

QCGroup obx::QueryCondition::or_(const QueryCondition& other) { return {true, copyAsPtr(), internalCopyAsPtr(other)}; }
QCGroup obx::QueryCondition::operator||(const QueryCondition& rhs) { return or_(rhs); }

namespace {  // internal
enum class QueryOp {
    Equal,
    NotEqual,
    Less,
    LessOrEq,
    Greater,
    GreaterOrEq,
    Contains,
    StartsWith,
    EndsWith,
    Between,
    In,
    NotIn,
    Null,
    NotNull
};

// Internal base class for all the condition containers. Each container starts with `QC` and ends with the type of the
// contents. That's not necessarily the same as the property type the condition is used with, e.g. for Bool::equals()
// using QCInt64, StringVector::contains query using QCString, etc.
class QC : public QueryCondition {
protected:
    obx_schema_id propId;
    QueryOp op;

public:
    QC(obx_schema_id propId, QueryOp op) : propId(propId), op(op) {}
    virtual ~QC() = default;

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override {
        return std::unique_ptr<QueryCondition>(new QC(*this));
    };

    /// Indicates a programming error when in ObjectBox C++ binding - chosen QC struct doesn't support the desired
    /// condition. Consider changing QueryOp to a template variable - it would enable us to use static_assert() instead
    /// of the current runtime check. Additionally, it might produce better (smaller/faster) code because the compiler
    /// could optimize out all the unused switch statements and variables (`value2`).
    [[noreturn]] void throwInvalidOperation() const {
        throw std::logic_error(std::string("Invalid condition - operation not supported: ") + std::to_string(int(op)));
    }

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool) const override {
        switch (op) {
            case QueryOp::Null:
                return obx_qb_null(cqb, propId);
            case QueryOp::NotNull:
                return obx_qb_not_null(cqb, propId);
            default:
                throwInvalidOperation();
        }
    }
};

class QCInt64 : public QC {
    int64_t value1;
    int64_t value2;

public:
    QCInt64(obx_schema_id propId, QueryOp op, int64_t value1, int64_t value2 = 0)
        : QC(propId, op), value1(value1), value2(value2) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool) const override {
        switch (op) {
            case QueryOp::Equal:
                return obx_qb_equals_int(cqb, propId, value1);
            case QueryOp::NotEqual:
                return obx_qb_not_equals_int(cqb, propId, value1);
            case QueryOp::Less:
                return obx_qb_less_than_int(cqb, propId, value1);
            case QueryOp::Greater:
                return obx_qb_greater_than_int(cqb, propId, value1);
            case QueryOp::Between:
                return obx_qb_between_2ints(cqb, propId, value1, value2);
            default:
                throwInvalidOperation();
        }
    }
};

class QCDouble : public QC {
    double value1;
    double value2;

public:
    QCDouble(obx_schema_id propId, QueryOp op, double value1, double value2 = 0)
        : QC(propId, op), value1(value1), value2(value2) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool) const override {
        switch (op) {
            case QueryOp::Less:
                return obx_qb_less_than_double(cqb, propId, value1);
            case QueryOp::Greater:
                return obx_qb_greater_than_double(cqb, propId, value1);
            case QueryOp::Between:
                return obx_qb_between_2doubles(cqb, propId, value1, value2);
            default:
                throwInvalidOperation();
        }
    }
};

class QCInt32Array : public QC {
    std::vector<int32_t> values;

public:
    QCInt32Array(obx_schema_id propId, QueryOp op, std::vector<int32_t>&& values)
        : QC(propId, op), values(std::move(values)) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool) const override {
        switch (op) {
            case QueryOp::In:
                return obx_qb_in_int32s(cqb, propId, values.data(), values.size());
            case QueryOp::NotIn:
                return obx_qb_not_in_int32s(cqb, propId, values.data(), values.size());
            default:
                throwInvalidOperation();
        }
    }
};

class QCInt64Array : public QC {
    std::vector<int64_t> values;

public:
    QCInt64Array(obx_schema_id propId, QueryOp op, std::vector<int64_t>&& values)
        : QC(propId, op), values(std::move(values)) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool) const override {
        switch (op) {
            case QueryOp::In:
                return obx_qb_in_int64s(cqb, propId, values.data(), values.size());
            case QueryOp::NotIn:
                return obx_qb_not_in_int64s(cqb, propId, values.data(), values.size());
            default:
                throwInvalidOperation();
        }
    }
};

template <OBXPropertyType PropertyType>
class QCString : public QC {
    std::string value;
    bool caseSensitive;

public:
    QCString(obx_schema_id propId, QueryOp op, bool caseSensitive, std::string&& value)
        : QC(propId, op), caseSensitive(caseSensitive), value(std::move(value)) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool) const override {
        if (PropertyType == OBXPropertyType_String) {
            switch (op) {
                case QueryOp::Equal:
                    return obx_qb_equals_string(cqb, propId, value.c_str(), caseSensitive);
                case QueryOp::NotEqual:
                    return obx_qb_not_equals_string(cqb, propId, value.c_str(), caseSensitive);
                case QueryOp::Less:
                    return obx_qb_less_than_string(cqb, propId, value.c_str(), caseSensitive);
                case QueryOp::LessOrEq:
                    return obx_qb_less_or_equal_string(cqb, propId, value.c_str(), caseSensitive);
                case QueryOp::Greater:
                    return obx_qb_greater_than_string(cqb, propId, value.c_str(), caseSensitive);
                case QueryOp::GreaterOrEq:
                    return obx_qb_greater_or_equal_string(cqb, propId, value.c_str(), caseSensitive);
                case QueryOp::StartsWith:
                    return obx_qb_starts_with_string(cqb, propId, value.c_str(), caseSensitive);
                case QueryOp::EndsWith:
                    return obx_qb_ends_with_string(cqb, propId, value.c_str(), caseSensitive);
                case QueryOp::Contains:
                    return obx_qb_contains_string(cqb, propId, value.c_str(), caseSensitive);
                default:;  // fall-through to throw; default here so that the compiler doesn't complain
            }
        } else if (PropertyType == OBXPropertyType_StringVector) {
            switch (op) {
                case QueryOp::Contains:
                    return obx_qb_any_equals_string(cqb, propId, value.c_str(), caseSensitive);
                default:;  // fall-through to throw; default here so that the compiler doesn't complain
            }
        }
        throwInvalidOperation();
    }
};

using QCStringForString = QCString<OBXPropertyType_String>;
using QCStringForStringVector = QCString<OBXPropertyType_StringVector>;

class QCStringArray : public QC {
    std::vector<std::string> values;  // stored string copies
    bool caseSensitive;

public:
    QCStringArray(obx_schema_id propId, QueryOp op, bool caseSensitive, std::vector<std::string>&& values)
        : QC(propId, op), caseSensitive(caseSensitive), values(std::move(values)) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool) const override {
        // don't make  an instance variable - it's not trivially copyable by copyAsPtr() and is usually called just once
        std::vector<const char*> cvalues;
        cvalues.resize(values.size());
        for (size_t i = 0; i < values.size(); i++) {
            cvalues[i] = values[i].c_str();
        }
        switch (op) {
            case QueryOp::In:
                return obx_qb_in_strings(cqb, propId, cvalues.data(), cvalues.size(), caseSensitive);
            default:
                throwInvalidOperation();
        }
    }
};

class QCBytes : public QC {
    std::vector<uint8_t> value;

public:
    QCBytes(obx_schema_id propId, QueryOp op, std::vector<uint8_t>&& value) : QC(propId, op), value(std::move(value)) {}

    QCBytes(obx_schema_id propId, QueryOp op, const void* data, size_t size)
        : QC(propId, op), value(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool) const override {
        switch (op) {
            case QueryOp::Equal:
                return obx_qb_equals_bytes(cqb, propId, value.data(), value.size());
            case QueryOp::Less:
                return obx_qb_less_than_bytes(cqb, propId, value.data(), value.size());
            case QueryOp::LessOrEq:
                return obx_qb_less_or_equal_bytes(cqb, propId, value.data(), value.size());
            case QueryOp::Greater:
                return obx_qb_greater_than_bytes(cqb, propId, value.data(), value.size());
            case QueryOp::GreaterOrEq:
                return obx_qb_greater_or_equal_bytes(cqb, propId, value.data(), value.size());
            default:
                throwInvalidOperation();
        }
    }
};
}  // namespace

// FlatBuffer builder is reused so the allocated memory stays available for the future objects.
thread_local flatbuffers::FlatBufferBuilder fbb;
inline void fbbCleanAfterUse() {
    if (fbb.GetSize() > 1024 * 1024) fbb.Reset();
}

// enable_if_t missing in c++11 so let's have a shorthand here
template <bool Condition, typename T = void>
using enable_if_t = typename std::enable_if<Condition, T>::type;

template <OBXPropertyType T, bool includingRelation = false>
using EnableIfInteger =
    enable_if_t<T == OBXPropertyType_Int || T == OBXPropertyType_Long || T == OBXPropertyType_Short ||
                T == OBXPropertyType_Short || T == OBXPropertyType_Byte || T == OBXPropertyType_Date ||
                T == OBXPropertyType_DateNano || (includingRelation && T == OBXPropertyType_Relation)>;

template <OBXPropertyType T>
using EnableIfIntegerOrRel = EnableIfInteger<T, true>;

template <OBXPropertyType T>
using EnableIfFloating = enable_if_t<T == OBXPropertyType_Float || T == OBXPropertyType_Double>;

template <OBXPropertyType T>
using EnableIfDate = enable_if_t<T == OBXPropertyType_Date || T == OBXPropertyType_DateNano>;

static constexpr OBXPropertyType typeless = OBXPropertyType(0);
}  // namespace

/// "Typeless" property used as a base class for other types - sharing common conditions.
template <typename EntityT>
class PropertyTypeless {
public:
    constexpr PropertyTypeless(obx_schema_id id) : id_(id) {}
    inline obx_schema_id id() const { return id_; }

    QC isNull() const { return {id_, QueryOp::Null}; }
    QC isNotNull() const { return {id_, QueryOp::NotNull}; }

protected:
    /// property ID
    const obx_schema_id id_;
};

/// Carries property information when used in the entity-meta ("underscore") class
template <typename EntityT, OBXPropertyType ValueT>
class Property : public PropertyTypeless<EntityT> {
public:
    constexpr Property(obx_schema_id id) : PropertyTypeless<EntityT>(id) {}

    template <OBXPropertyType T = ValueT, typename = enable_if_t<T == OBXPropertyType_Bool>>
    QCInt64 equals(bool value) const {
        return {this->id_, QueryOp::Equal, value};
    }

    template <OBXPropertyType T = ValueT, typename = enable_if_t<T == OBXPropertyType_Bool>>
    QCInt64 notEquals(bool value) const {
        return {this->id_, QueryOp::NotEqual, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfIntegerOrRel<T>>
    QCInt64 equals(int64_t value) const {
        return {this->id_, QueryOp::Equal, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfIntegerOrRel<T>>
    QCInt64 notEquals(int64_t value) const {
        return {this->id_, QueryOp::NotEqual, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfInteger<T>>
    QCInt64 lessThan(int64_t value) const {
        return {this->id_, QueryOp::Less, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfInteger<T>>
    QCInt64 greaterThan(int64_t value) const {
        return {this->id_, QueryOp::Greater, value};
    }

    /// finds objects with property value between a and b (including a and b)
    template <OBXPropertyType T = ValueT, typename = EnableIfInteger<T>>
    QCInt64 between(int64_t a, int64_t b) const {
        return {this->id_, QueryOp::Between, a, b};
    }

    template <OBXPropertyType T = ValueT, typename = enable_if_t<T == OBXPropertyType_Int>>
    QCInt32Array in(std::vector<int32_t>&& values) const {
        return {this->id_, QueryOp::In, std::move(values)};
    }

    template <OBXPropertyType T = ValueT, typename = enable_if_t<T == OBXPropertyType_Int>>
    QCInt32Array in(const std::vector<int32_t>& values) const {
        return in(std::vector<int32_t>(values));
    }

    template <OBXPropertyType T = ValueT, typename = enable_if_t<T == OBXPropertyType_Int>>
    QCInt32Array notIn(std::vector<int32_t>&& values) const {
        return {this->id_, QueryOp::NotIn, std::move(values)};
    }

    template <OBXPropertyType T = ValueT, typename = enable_if_t<T == OBXPropertyType_Int>>
    QCInt32Array notIn(const std::vector<int32_t>& values) const {
        return notIn(std::vector<int32_t>(values));
    }

    template <OBXPropertyType T = ValueT,
              typename = enable_if_t<T == OBXPropertyType_Long || T == OBXPropertyType_Relation>>
    QCInt64Array in(std::vector<int64_t>&& values) const {
        return {this->id_, QueryOp::In, std::move(values)};
    }

    template <OBXPropertyType T = ValueT,
              typename = enable_if_t<T == OBXPropertyType_Long || T == OBXPropertyType_Relation>>
    QCInt64Array in(const std::vector<int64_t>& values) const {
        return in(std::vector<int64_t>(values));
    }

    template <OBXPropertyType T = ValueT,
              typename = enable_if_t<T == OBXPropertyType_Long || T == OBXPropertyType_Relation>>
    QCInt64Array notIn(std::vector<int64_t>&& values) const {
        return {this->id_, QueryOp::NotIn, std::move(values)};
    }

    template <OBXPropertyType T = ValueT,
              typename = enable_if_t<T == OBXPropertyType_Long || T == OBXPropertyType_Relation>>
    QCInt64Array notIn(const std::vector<int64_t>& values) const {
        return notIn(std::vector<int64_t>(values));
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfFloating<T>>
    QCDouble lessThan(double value) const {
        return {this->id_, QueryOp::Less, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfFloating<T>>
    QCDouble greaterThan(double value) const {
        return {this->id_, QueryOp::Greater, value};
    }

    /// finds objects with property value between a and b (including a and b)
    template <OBXPropertyType T = ValueT, typename = EnableIfFloating<T>>
    QCDouble between(double a, double b) const {
        return {this->id_, QueryOp::Between, a, b};
    }
};

/// Carries property information when used in the entity-meta ("underscore") class
template <typename EntityT>
class Property<EntityT, OBXPropertyType_String> : public PropertyTypeless<EntityT> {
public:
    constexpr Property(obx_schema_id id) : PropertyTypeless<EntityT>(id) {}

    QCStringForString equals(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::Equal, caseSensitive, std::move(value)};
    }

    QCStringForString equals(const std::string& value, bool caseSensitive = true) const {
        return equals(std::string(value), caseSensitive);
    }

    QCStringForString notEquals(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::NotEqual, caseSensitive, std::move(value)};
    }

    QCStringForString notEquals(const std::string& value, bool caseSensitive = true) const {
        return notEquals(std::string(value), caseSensitive);
    }

    QCStringForString lessThan(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::Less, caseSensitive, std::move(value)};
    }

    QCStringForString lessThan(const std::string& value, bool caseSensitive = true) const {
        return lessThan(std::string(value), caseSensitive);
    }

    QCStringForString lessOrEq(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::LessOrEq, caseSensitive, std::move(value)};
    }

    QCStringForString lessOrEq(const std::string& value, bool caseSensitive = true) const {
        return lessOrEq(std::string(value), caseSensitive);
    }

    QCStringForString greaterThan(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::Greater, caseSensitive, std::move(value)};
    }

    QCStringForString greaterThan(const std::string& value, bool caseSensitive = true) const {
        return greaterThan(std::string(value), caseSensitive);
    }

    QCStringForString greaterOrEq(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::GreaterOrEq, caseSensitive, std::move(value)};
    }

    QCStringForString greaterOrEq(const std::string& value, bool caseSensitive = true) const {
        return greaterOrEq(std::string(value), caseSensitive);
    }

    QCStringForString contains(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::Contains, caseSensitive, std::move(value)};
    }

    QCStringForString contains(const std::string& value, bool caseSensitive = true) const {
        return contains(std::string(value), caseSensitive);
    }

    QCStringForString startsWith(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::StartsWith, caseSensitive, std::move(value)};
    }

    QCStringForString startsWith(const std::string& value, bool caseSensitive = true) const {
        return startsWith(std::string(value), caseSensitive);
    }

    QCStringForString endsWith(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::EndsWith, caseSensitive, std::move(value)};
    }

    QCStringForString endsWith(const std::string& value, bool caseSensitive = true) const {
        return endsWith(std::string(value), caseSensitive);
    }

    QCStringArray in(std::vector<std::string>&& values, bool caseSensitive = true) const {
        return {this->id_, QueryOp::In, caseSensitive, std::move(values)};
    }

    QCStringArray in(const std::vector<std::string>& values, bool caseSensitive = true) const {
        return in(std::vector<std::string>(values), caseSensitive);
    }
};

/// Carries property information when used in the entity-meta ("underscore") class
template <typename EntityT>
class Property<EntityT, OBXPropertyType_ByteVector> : public PropertyTypeless<EntityT> {
public:
    constexpr Property(obx_schema_id id) : PropertyTypeless<EntityT>(id) {}

    QCBytes equals(std::vector<uint8_t>&& data) const { return {this->id_, QueryOp::Equal, std::move(data)}; }

    QCBytes equals(const void* data, size_t size) const { return {this->id_, QueryOp::Equal, data, size}; }

    QCBytes equals(const std::vector<uint8_t>& data) const { return equals(std::vector<uint8_t>(data)); }

    QCBytes lessThan(std::vector<uint8_t>&& data) const { return {this->id_, QueryOp::Less, std::move(data)}; }

    QCBytes lessThan(const void* data, size_t size) const { return {this->id_, QueryOp::Less, data, size}; }

    QCBytes lessThan(const std::vector<uint8_t>& data) const { return lessThan(std::vector<uint8_t>(data)); }

    QCBytes lessOrEq(std::vector<uint8_t>&& data) const { return {this->id_, QueryOp::LessOrEq, std::move(data)}; }

    QCBytes lessOrEq(const void* data, size_t size) const { return {this->id_, QueryOp::LessOrEq, data, size}; }

    QCBytes lessOrEq(const std::vector<uint8_t>& data) const { return lessOrEq(std::vector<uint8_t>(data)); }

    QCBytes greaterThan(std::vector<uint8_t>&& data) const { return {this->id_, QueryOp::Greater, std::move(data)}; }

    QCBytes greaterThan(const void* data, size_t size) const { return {this->id_, QueryOp::Greater, data, size}; }

    QCBytes greaterThan(const std::vector<uint8_t>& data) const { return greaterThan(std::vector<uint8_t>(data)); }

    QCBytes greaterOrEq(std::vector<uint8_t>&& data) const {
        return {this->id_, QueryOp::GreaterOrEq, std::move(data)};
    }

    QCBytes greaterOrEq(const void* data, size_t size) const { return {this->id_, QueryOp::GreaterOrEq, data, size}; }

    QCBytes greaterOrEq(const std::vector<uint8_t>& data) const { return greaterOrEq(std::vector<uint8_t>(data)); }
};

/// Carries property information when used in the entity-meta ("underscore") class
template <typename EntityT>
class Property<EntityT, OBXPropertyType_StringVector> : public PropertyTypeless<EntityT> {
public:
    constexpr Property(obx_schema_id id) : PropertyTypeless<EntityT>(id) {}

    QCStringForStringVector contains(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::Contains, caseSensitive, std::move(value)};
    }

    QCStringForStringVector contains(const std::string& value, bool caseSensitive = true) const {
        return contains(std::string(value), caseSensitive);
    }
};

/// Carries property-based to-one relation information when used in the entity-meta ("underscore") class
template <typename SourceEntityT, typename TargetEntityT>
class RelationProperty : public Property<SourceEntityT, OBXPropertyType_Relation> {
public:
    constexpr RelationProperty(obx_schema_id id) : Property<SourceEntityT, OBXPropertyType_Relation>(id) {}
};

/// Carries to-many relation information when used in the entity-meta ("underscore") class
template <typename SourceEntityT, typename TargetEntityT>
class RelationStandalone {
public:
    constexpr RelationStandalone(obx_schema_id id) : id_(id) {}
    inline obx_schema_id id() const { return id_; }

protected:
    /// standalone relation ID
    const obx_schema_id id_;
};

template <typename EntityT>
class Query;

/// Provides a simple wrapper for OBX_query_builder to simplify memory management - calls obx_qb_close() on destruction.
/// To specify actual conditions, use obx_qb_*() methods with queryBuilder.cPtr() as the first argument.
template <typename EntityT>
class QueryBuilder {
    using EntityBinding = typename EntityT::_OBX_MetaInfo;
    OBX_query_builder* cQueryBuilder_;
    Store& store_;
    bool isRoot_;

public:
    explicit QueryBuilder(Store& store)
        : QueryBuilder(store, obx_query_builder(store.cPtr(), EntityBinding::entityId()), true) {}

    /// Take ownership of an OBX_query_builder.
    /// @example
    ///          QueryBuilder innerQb(obx_qb_link_property(outerQb.cPtr(), linkPropertyId), false)
    explicit QueryBuilder(Store& store, OBX_query_builder* ptr, bool isRoot)
        : store_(store), cQueryBuilder_(ptr), isRoot_(isRoot) {
        checkPtrOrThrow(cQueryBuilder_, "can't create a query builder");
    }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    QueryBuilder(const QueryBuilder&) = delete;

    QueryBuilder(QueryBuilder&& source) noexcept
        : cQueryBuilder_(source.cQueryBuilder_), store_(source.store_), isRoot_(source.isRoot_) {
        source.cQueryBuilder_ = nullptr;
    }

    virtual ~QueryBuilder() { obx_qb_close(cQueryBuilder_); }

    OBX_query_builder* cPtr() const { return cQueryBuilder_; }

    /// Adds an order based on a given property.
    /// @param property the property used for the order
    /// @param flags combination of OBXOrderFlags
    /// @return the reference to the same QueryBuilder for fluent interface.
    template <OBXPropertyType PropType>
    QueryBuilder& order(Property<EntityT, PropType> property, int flags = 0) {
        checkErrOrThrow(obx_qb_order(cQueryBuilder_, property.id(), OBXOrderFlags(flags)));
        return *this;
    }

    /// Appends given condition/combination of conditions.
    /// @return the reference to the same QueryBuilder for fluent interface.
    QueryBuilder& with(const QueryCondition& condition) {
        internalApplyCondition(condition, cQueryBuilder_, true);
        return *this;
    }

    /// Links the (time series) entity type to another entity space using a time point in a linked entity.
    /// \note 1) Time series functionality (ObjectBox TS) must be available to use this.
    /// \note 2) Returned QueryBuilder switches context, make sure to call build() on the root QueryBuilder.
    /// @param property Property of the linked entity defining a time point or the begin of a time range.
    ///        Must be a date type (e.g. PropertyType_Date or PropertyType_DateNano).
    /// @return the linked QueryBuilder; "switches context" to the linked entity.
    template <typename RangePropertyEntityT, OBXPropertyType RangePropertyType,
              typename = EnableIfDate<RangePropertyType>>
    QueryBuilder<RangePropertyEntityT> linkTime(Property<RangePropertyEntityT, RangePropertyType> property) {
        return linkedQB<RangePropertyEntityT>(
            obx_qb_link_time(cPtr(), entityId<RangePropertyEntityT>(), property.id(), 0));
    }

    /// Links the (time series) entity type to another entity space using a range defined in a given linked entity.
    /// \note 1) Time series functionality (ObjectBox TS) must be available to use this.
    /// \note 2) Returned QueryBuilder switches context, make sure to call build() on the root QueryBuilder.
    /// @param beginProperty Property of the linked entity defining the beginning of a time range.
    ///        Must be a date type (e.g. PropertyType_Date or PropertyType_DateNano).
    /// @param endProperty Property of the linked entity defining the end of a time range.
    ///        Must be a date type (e.g. PropertyType_Date or PropertyType_DateNano).
    /// @return the linked QueryBuilder; "switches context" to the linked entity.
    template <typename RangePropertyEntityT, OBXPropertyType RangePropertyType,
              typename = EnableIfDate<RangePropertyType>>
    QueryBuilder<RangePropertyEntityT> linkTime(Property<RangePropertyEntityT, RangePropertyType> beginProperty,
                                                Property<RangePropertyEntityT, RangePropertyType> endProperty) {
        return linkedQB<RangePropertyEntityT>(
            obx_qb_link_time(cPtr(), entityId<RangePropertyEntityT>(), beginProperty.id(), endProperty.id()));
    }

    /// Create a link based on a property-relation (to-one).
    /// \note Returned QueryBuilder switches context, make sure to call build() on the root QueryBuilder.
    /// @param rel the relation property, with source EntityT represented by this QueryBuilder.
    /// @param condition a condition or a group of conditions to apply on the linked entity
    /// @return the linked QueryBuilder; "switches context" to the linked entity.
    template <typename RelTargetEntityT>
    QueryBuilder<RelTargetEntityT> link(RelationProperty<EntityT, RelTargetEntityT> rel) {
        return linkedQB<RelTargetEntityT>(obx_qb_link_property(cPtr(), rel.id()));
    }

    /// Create a backlink based on a property-relation used in reverse (to-many).
    /// \note Returned QueryBuilder switches context, make sure to call build() on the root QueryBuilder.
    /// @param rel the relation property, with target EntityT represented by this QueryBuilder.
    /// @return the linked QueryBuilder; "switches context" to the linked entity.
    template <typename RelSourceEntityT>
    QueryBuilder<RelSourceEntityT> backlink(RelationProperty<RelSourceEntityT, EntityT> rel) {
        return linkedQB<RelSourceEntityT>(obx_qb_backlink_property(cPtr(), entityId<RelSourceEntityT>(), rel.id()));
    }

    /// Create a link based on a standalone relation (many-to-many)
    /// \note Returned QueryBuilder switches context, make sure to call build() on the root QueryBuilder.
    /// @return the linked QueryBuilder; "switches context" to the linked entity.
    template <typename RelTargetEntityT>
    QueryBuilder<RelTargetEntityT> link(RelationStandalone<EntityT, RelTargetEntityT> rel) {
        return linkedQB<RelTargetEntityT>(obx_qb_link_standalone(cPtr(), rel.id()));
    }

    /// Create a backlink based on a standalone relation (many-to-many, reverse direction)
    /// \note Returned QueryBuilder switches context, make sure to call build() on the root QueryBuilder.
    /// @return the linked QueryBuilder; "switches context" to the linked entity.
    template <typename RelSourceEntityT>
    QueryBuilder<RelSourceEntityT> backlink(RelationStandalone<RelSourceEntityT, EntityT> rel) {
        return linkedQB<RelSourceEntityT>(obx_qb_backlink_standalone(cPtr(), rel.id()));
    }

    Query<EntityT> build();

protected:
    template <typename LinkedEntityT>
    QueryBuilder<LinkedEntityT> linkedQB(OBX_query_builder* linkedQB) {
        checkPtrOrThrow(linkedQB, "can't build a query link");
        // NOTE: linkedQB may be lost if the user doesn't keep the returned sub-builder around and that's fine.
        // We're relying on the C-API keeping track of sub-builders on the root QB.
        return QueryBuilder<LinkedEntityT>(store_, linkedQB, false);
    }
};

/// Provides a simple wrapper for OBX_query to simplify memory management - calls obx_query_close() on destruction.
/// To execute the actual methods, use obx_query_*() methods with query.cPtr() as the first argument.
/// Internal note: this is a template because it will provide EntityType-specific methods in the future
template <typename EntityT>
class Query {
    OBX_query* cQuery_;
    Store& store_;

public:
    /// Builds a query with the parameters specified by the builder
    explicit Query(Store& store, OBX_query_builder* qb) : cQuery_(obx_query(qb)), store_(store) {
        checkPtrOrThrow(cQuery_, "can't build a query");
    }

    /// Clones the query
    Query(const Query& query) : cQuery_(obx_query_clone(query.cQuery_)), store_(query.store_) {
        checkPtrOrThrow(cQuery_, "couldn't make a query clone");
    }

    Query(Query&& source) noexcept : cQuery_(source.cQuery_), store_(source.store_) { source.cQuery_ = nullptr; }

    virtual ~Query() { obx_query_close(cQuery_); }

    OBX_query* cPtr() const { return cQuery_; }

    /// Sets an offset of what items to start at.
    /// This offset is stored for any further calls on the query until changed.
    /// Call with offset=0 to reset to the default behavior, i.e. starting from the first element.
    Query& offset(size_t offset) {
        checkErrOrThrow(obx_query_offset(cQuery_, offset));
        return *this;
    }

    /// Sets a limit on the number of processed items.
    /// This limit is stored for any further calls on the query until changed.
    /// Call with limit=0 to reset to the default behavior - zero limit means no limit applied.
    Query& limit(size_t limit) {
        checkErrOrThrow(obx_query_limit(cQuery_, limit));
        return *this;
    }

    /// Finds all objects matching the query.
    /// Note: returning a vector of pointers to avoid excessive allocation because we don't know the number of returned
    /// objects beforehand.
    std::vector<std::unique_ptr<EntityT>> find() {
        OBJECTBOX_VERIFY_STATE(cQuery_ != nullptr);

        CollectingVisitor<EntityT> visitor;
        obx_query_visit(cQuery_, CollectingVisitor<EntityT>::visit, &visitor);
        return std::move(visitor.items);
    }

    /// Returns IDs of all matching objects.
    std::vector<obx_id> findIds() { return idVectorOrThrow(obx_query_find_ids(cQuery_)); }

    /// Returns the number of matching objects.
    uint64_t count() {
        uint64_t result;
        checkErrOrThrow(obx_query_count(cQuery_, &result));
        return result;
    }

    /// Removes all matching objects from the database & returns the number of deleted objects.
    size_t remove() {
        uint64_t result;
        checkErrOrThrow(obx_query_remove(cQuery_, &result));
        return result;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <
        typename PropertyEntityT, OBXPropertyType PropertyType,
        typename = enable_if_t<PropertyType == OBXPropertyType_String || PropertyType == OBXPropertyType_StringVector>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, const char* value) {
        checkErrOrThrow(obx_query_param_string(cQuery_, entityId<PropertyEntityT>(), property.id(), value));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <
        typename PropertyEntityT, OBXPropertyType PropertyType,
        typename = enable_if_t<PropertyType == OBXPropertyType_String || PropertyType == OBXPropertyType_StringVector>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, const std::string& value) {
        return setParameter(property, value.c_str());
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_String> property, const char* const values[],
                        int count) {
        checkErrOrThrow(obx_query_param_strings(cQuery_, entityId<PropertyEntityT>(), property.id(), values, count));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_String> property,
                        const std::vector<const char*>& values) {
        return setParameter(property, values.data(), values.size());
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_String> property,
                        const std::vector<std::string>& values) {
        std::vector<const char*> cValues;
        cValues.reserve(values.size());
        for (const std::string& str : values) {
            cValues.push_back(str.c_str());
        }
        return setParameter(property, cValues.data(), cValues.size());
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_Bool> property, bool value) {
        checkErrOrThrow(obx_query_param_int(cQuery_, entityId<PropertyEntityT>(), property.id(), value));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType, typename = EnableIfIntegerOrRel<PropertyType>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, int64_t value) {
        checkErrOrThrow(obx_query_param_int(cQuery_, entityId<PropertyEntityT>(), property.id(), value));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType, typename = EnableIfIntegerOrRel<PropertyType>>
    Query& setParameters(Property<PropertyEntityT, PropertyType> property, int64_t valueA, int64_t valueB) {
        checkErrOrThrow(obx_query_param_2ints(cQuery_, entityId<PropertyEntityT>(), property.id(), valueA, valueB));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType,
              typename = enable_if_t<PropertyType == OBXPropertyType_Long || PropertyType == OBXPropertyType_Relation>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, const std::vector<int64_t>& values) {
        checkErrOrThrow(obx_query_param_int64s(cQuery_, entityId<PropertyEntityT>(), property.id(), values.data(),
                                                  values.size()));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType,
              typename = enable_if_t<PropertyType == OBXPropertyType_Int>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, const std::vector<int32_t>& values) {
        checkErrOrThrow(obx_query_param_int32s(cQuery_, entityId<PropertyEntityT>(), property.id(), values.data(),
                                                  values.size()));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType, typename = EnableIfFloating<PropertyType>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, double value) {
        checkErrOrThrow(obx_query_param_double(cQuery_, entityId<PropertyEntityT>(), property.id(), value));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType, typename = EnableIfFloating<PropertyType>>
    Query& setParameters(Property<PropertyEntityT, PropertyType> property, double valueA, double valueB) {
        checkErrOrThrow(obx_query_param_2doubles(cQuery_, entityId<PropertyEntityT>(), property.id(), valueA, valueB));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_ByteVector> property, const void* value,
                        size_t size) {
        checkErrOrThrow(obx_query_param_bytes(cQuery_, entityId<PropertyEntityT>(), property.id(), value, size));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_ByteVector> property,
                        const std::vector<uint8_t>& value) {
        return setParameter(property, value.data(), value.size());
    }
};

template <typename EntityT>
inline Query<EntityT> QueryBuilder<EntityT>::build() {
    OBJECTBOX_VERIFY_STATE(isRoot_);
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

    /// Start building a query this entity.
    QueryBuilder<EntityT> query() { return QueryBuilder<EntityT>(store_); }

    /// Start building a query this entity.
    QueryBuilder<EntityT> query(const QueryCondition& condition) {
        QueryBuilder<EntityT> qb(store_);
        internalApplyCondition(condition, qb.cPtr(), true);
        return qb;
    }

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
        obx_id id = obx_box_put_object4(cBox_, fbb.GetBufferPointer(), fbb.GetSize(), mode);
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
    /// @return the number of put elements (always equal to objects.size() for this overload)
    size_t put(std::vector<EntityT>& objects, std::vector<obx_id>* outIds = nullptr, OBXPutMode mode = OBXPutMode_PUT) {
        return putMany(objects, outIds, mode);
    }

    /// @overload
    /// @return the number of put elements (always equal to objects.size() for this overload)
    size_t put(const std::vector<EntityT>& objects, std::vector<obx_id>* outIds = nullptr,
               OBXPutMode mode = OBXPutMode_PUT) {
        return putMany(objects, outIds, mode);
    }

    /// @overload
    /// @return the number of put elements, i.e. the number of std::unique_ptr != nullptr
    size_t put(std::vector<std::unique_ptr<EntityT>>& objects, std::vector<obx_id>* outIds = nullptr,
               OBXPutMode mode = OBXPutMode_PUT) {
        return putMany(objects, outIds, mode);
    }

#ifdef __cpp_lib_optional
    /// @overload
    /// @return the number of put elements, i.e. the number of items with std::optional::has_value() == true
    size_t put(std::vector<std::optional<EntityT>>& objects, std::vector<obx_id>* outIds = nullptr,
               OBXPutMode mode = OBXPutMode_PUT) {
        return putMany(objects, outIds, mode);
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
    /// @param toOneRel the relation property, which must belong to the entity type represented by this box.
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
    template <typename SourceEntityT, typename TargetEntityT>
    std::vector<obx_id> backlinkIds(RelationProperty<SourceEntityT, TargetEntityT> toOneRel, obx_id objectId) {
        static_assert(std::is_same<SourceEntityT, EntityT>::value,
                      "Given property (to-one relation) doesn't belong to this box - entity type mismatch");
        return idVectorOrThrow(obx_box_get_backlink_ids(cBox_, toOneRel.id(), objectId));
    }

    /// Replace the list of standalone relation target objects on the given source object.
    /// @param toManyRel must be a standalone relation ID with source object entity belonging to this box
    /// @param sourceObjectId identifies an object from this box
    /// @param targetObjectIds identifies objects from a target box (as per the relation definition)
    /// @todo consider providing a method similar to the one in Go - inserting target objects with 0 IDs
    template <typename SourceEntityT, typename TargetEntityT>
    void standaloneRelReplace(RelationStandalone<SourceEntityT, TargetEntityT> toManyRel, obx_id sourceObjectId,
                              const std::vector<obx_id>& targetObjectIds) {
        static_assert(std::is_same<SourceEntityT, EntityT>::value,
                      "Given relation (to-many) source entity must be the same as this box entity");

        // we use set_difference below so need to work on sorted vectors, thus we need to make a copy
        auto newIds = targetObjectIds;
        std::sort(newIds.begin(), newIds.end());

        Transaction tx(store_, TxMode::WRITE);

        auto oldIds = standaloneRelIds(toManyRel, sourceObjectId);
        std::sort(oldIds.begin(), oldIds.end());

        // find IDs to remove, i.e. those that previously were present and aren't anymore
        std::vector<obx_id> diff;
        std::set_difference(oldIds.begin(), oldIds.end(), newIds.begin(), newIds.end(),
                            std::inserter(diff, diff.begin()));

        for (obx_id targetId : diff) {
            standaloneRelRemove(toManyRel, sourceObjectId, targetId);
        }
        diff.clear();

        // find IDs to insert, i.e. those that previously weren't present and are now
        std::set_difference(newIds.begin(), newIds.end(), oldIds.begin(), oldIds.end(),
                            std::inserter(diff, diff.begin()));
        for (obx_id targetId : diff) {
            standaloneRelPut(toManyRel, sourceObjectId, targetId);
        }

        tx.success();
    }

    /// Insert a standalone relation entry between two objects.
    /// @param toManyRel must be a standalone relation ID with source object entity belonging to this box
    /// @param sourceObjectId identifies an object from this box
    /// @param targetObjectId identifies an object from a target box (as per the relation definition)
    template <typename TargetEntityT>
    void standaloneRelPut(RelationStandalone<EntityT, TargetEntityT> toManyRel, obx_id sourceObjectId,
                          obx_id targetObjectId) {
        checkErrOrThrow(obx_box_rel_put(cBox_, toManyRel.id(), sourceObjectId, targetObjectId));
    }

    /// Remove a standalone relation entry between two objects.
    /// @param toManyRel must be a standalone relation ID with source object entity belonging to this box
    /// @param sourceObjectId identifies an object from this box
    /// @param targetObjectId identifies an object from a target box (as per the relation definition)
    template <typename TargetEntityT>
    void standaloneRelRemove(RelationStandalone<EntityT, TargetEntityT> toManyRel, obx_id sourceObjectId,
                             obx_id targetObjectId) {
        checkErrOrThrow(obx_box_rel_remove(cBox_, toManyRel.id(), sourceObjectId, targetObjectId));
    }

    /// Fetch IDs of all objects in this Box related to the given object (typically from another Box).
    /// Used for a stand-alone relation and its "regular" direction; this Box represents the target of the relation.
    /// @param relationId ID of a standalone relation, whose target type matches this Box
    /// @param objectId object ID of the relation source type (typically from another Box)
    /// @returns resulting IDs representing objects in this Box
    /// @todo improve docs by providing an example with a clear distinction between source and target type
    template <typename SourceEntityT>
    std::vector<obx_id> standaloneRelIds(RelationStandalone<SourceEntityT, EntityT> toManyRel, obx_id objectId) {
        return idVectorOrThrow(obx_box_rel_get_ids(cBox_, toManyRel.id(), objectId));
    }

    /// Fetch IDs of all objects in this Box related to the given object (typically from another Box).
    /// Used for a stand-alone relation and its "backlink" direction; this Box represents the source of the relation.
    /// @param relationId ID of a standalone relation, whose source type matches this Box
    /// @param objectId object ID of the relation target type (typically from another Box)
    /// @returns resulting IDs representing objects in this Box
    template <typename TargetEntityT>
    std::vector<obx_id> standaloneRelBacklinkIds(RelationStandalone<EntityT, TargetEntityT> toManyRel,
                                                 obx_id objectId) {
        return idVectorOrThrow(obx_box_rel_get_backlink_ids(cBox_, toManyRel.id(), objectId));
    }

    /// Time series: get the limits (min/max time values) over all objects
    /// @param outMinId pointer to receive an output (may be nullptr)
    /// @param outMinValue pointer to receive an output (may be nullptr)
    /// @param outMaxId pointer to receive an output (may be nullptr)
    /// @param outMaxValue pointer to receive an output (may be nullptr)
    /// @returns true if objects were found (IDs/values are available)
    bool timeSeriesMinMax(obx_id* outMinId, int64_t* outMinValue, obx_id* outMaxId, int64_t* outMaxValue) {
        obx_err err = obx_box_ts_min_max(cBox_, outMinId, outMinValue, outMaxId, outMaxValue);
        if (err == OBX_SUCCESS) return true;
        if (err == OBX_NOT_FOUND) return false;
        throwLastError();
    }

    /// Time series: get the limits (min/max time values) over objects within the given time range
    /// @returns true if objects were found in the given range (IDs/values are available)
    bool timeSeriesMinMax(int64_t rangeBegin, int64_t rangeEnd, obx_id* outMinId, int64_t* outMinValue,
                          obx_id* outMaxId, int64_t* outMaxValue) {
        obx_err err =
            obx_box_ts_min_max_range(cBox_, rangeBegin, rangeEnd, outMinId, outMinValue, outMaxId, outMaxValue);
        if (err == OBX_SUCCESS) return true;
        if (err == OBX_NOT_FOUND) return false;
        throwLastError();
    }

private:
    template <typename Vector>
    size_t putMany(Vector& objects, std::vector<obx_id>* outIds, OBXPutMode mode) {
        if (outIds) {
            outIds->clear();
            outIds->reserve(objects.size());
        }

        // Don't start a TX in case there's no data.
        // Note: Don't move this above clearing outIds vector - our contract says that we clear outIds before starting
        // execution so we must do it even if no objects were passed.
        if (objects.empty()) return 0;

        size_t count = 0;
        CursorTx cursor(TxMode::WRITE, store_, EntityBinding::entityId());
        for (auto& object : objects) {
            obx_id id = cursorPut(cursor, object, mode);  // type-based overloads here
            if (outIds) outIds->push_back(id);  // always include in outIds even if the item wasn't present (id == 0)
            if (id) count++;
        }
        cursor.commitAndClose();
        fbbCleanAfterUse();  // NOTE might not get called in case of an exception
        return count;
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
        return object ? cursorPut(cursor, *object, mode) : 0;
    }

#ifdef __cpp_lib_optional
    obx_id cursorPut(CursorTx& cursor, std::optional<EntityT>& object, OBXPutMode mode) {
        return object.has_value() ? cursorPut(cursor, *object, mode) : 0;
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

/**@}*/  // end of doxygen group
}  // namespace obx