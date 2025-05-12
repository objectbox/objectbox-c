/*
 * Copyright 2018-2023 ObjectBox Ltd. All rights reserved.
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

// Usage note: Put "#define OBX_CPP_FILE" before including this file in (exactly) one of your .cpp/.cc files.

#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "objectbox.h"

#ifndef OBX_DISABLE_FLATBUFFERS  // FlatBuffers is required to put data; you can disable it until have the include file.
#include "flatbuffers/flatbuffers.h"
#endif

#ifdef __cpp_lib_optional
#include <optional>
#endif

static_assert(OBX_VERSION_MAJOR == 4 && OBX_VERSION_MINOR == 3 && OBX_VERSION_PATCH == 0,  // NOLINT
              "Versions of objectbox.h and objectbox.hpp files do not match, please update");

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"  // So we can have the next pragma with strict compiler settings
#pragma clang diagnostic ignored "-Wunused-function"  // It's an API, so it's normal to use parts only
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"  // It's an API, so it's normal to use parts only
#pragma ide diagnostic ignored "readability-else-after-return"        // They way it's used here improves readability
#endif

namespace obx {

/**
 * @defgroup cpp ObjectBox C++ API
 * @{
 */

/// \brief Base class for ObjectBox related exceptions.
/// Note that there are currently 3 main sub types:
/// IllegalArgumentException, IllegalStateException, and, for all other error types, DbException.
class Exception : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;

    /// The error code as defined in objectbox.h via the OBX_ERROR_* constants
    virtual int code() const = 0;
};

/// Thrown when the passed arguments are illegal
class IllegalArgumentException : public Exception {
public:
    using Exception::Exception;

    /// Always OBX_ERROR_ILLEGAL_ARGUMENT
    int code() const override { return OBX_ERROR_ILLEGAL_ARGUMENT; }
};

/// Thrown when a request does not make sense in the current state. For example, doing actions on a closed object.
class IllegalStateException : public Exception {
public:
    using Exception::Exception;

    /// Always OBX_ERROR_ILLEGAL_STATE
    int code() const override { return OBX_ERROR_ILLEGAL_STATE; }
};

/// Thrown when a transaction is about to commit but it would exceed the user-defined data size limit.
/// See obx_opt_max_data_size_in_kb() for details.
class MaxDataSizeExceededException : public Exception {
public:
    using Exception::Exception;

    /// Always OBX_ERROR_MAX_DATA_SIZE_EXCEEDED
    int code() const override { return OBX_ERROR_MAX_DATA_SIZE_EXCEEDED; }
};

/// The operation on a resource (typically a Store) failed because the resources is in process of being shut down or
/// already has shutdown. For example, calling methods on the Store will throw this exception after Store::close().
class ShuttingDownException : public IllegalStateException {
public:
    using IllegalStateException::IllegalStateException;

    /// Always OBX_ERROR_SHUTTING_DOWN
    int code() const override { return OBX_ERROR_SHUTTING_DOWN; }
};

#define OBX_VERIFY_ARGUMENT(c) \
    ((c) ? (void) (0) : obx::internal::throwIllegalArgumentException("Argument validation failed: ", #c))

#define OBX_VERIFY_STATE(c) \
    ((c) ? (void) (0) : obx::internal::throwIllegalStateException("State condition failed: ", #c))

/// Database related exception, containing a error code to differentiate between various errors.
/// Note: what() typically contains a specific text about the error condition (sometimes helpful to resolve the issue).
class DbException : public Exception {
    const int code_;

public:
    explicit DbException(const std::string& text, int code = 0) : Exception(text), code_(code) {}
    explicit DbException(const char* text, int code = 0) : Exception(text), code_(code) {}

    /// The error code as defined in objectbox.h via the OBX_ERROR_* constants
    int code() const override { return code_; }
};

/// A functionality was invoked that is not part in this edition of ObjectBox.
class FeatureNotAvailableException : public Exception {
public:
    using Exception::Exception;

    /// Always OBX_ERROR_FEATURE_NOT_AVAILABLE
    int code() const override { return OBX_ERROR_FEATURE_NOT_AVAILABLE; }
};

namespace internal {

[[noreturn]] void throwIllegalArgumentException(const char* text1, const char* text2);
[[noreturn]] void throwIllegalStateException(const char* text1, const char* text2);

/// @throws Exception using the given error (defaults to obx_last_error_code())
[[noreturn]] void throwLastError(obx_err err = obx_last_error_code(), const char* contextPrefix = nullptr);

void appendLastErrorText(obx_err err, std::string& outMessage);

/// @throws Exception or subclass depending on the given err, with the given message
[[noreturn]] void throwError(obx_err err, const std::string& message);

void checkErrOrThrow(obx_err err);

bool checkSuccessOrThrow(obx_err err);

void checkPtrOrThrow(const void* ptr, const char* contextPrefix = nullptr);

void checkIdOrThrow(uint64_t id, const char* contextPrefix = nullptr);

/// "Pass-through" variant of checkPtrOrThrow() - prefer the latter if this is not required (less templating).
template <typename T>
T* checkedPtrOrThrow(T* ptr, const char* contextPrefix = nullptr) {
    internal::checkPtrOrThrow(ptr, contextPrefix);
    return ptr;
}

/// Dereferences the given pointer, which must be non-null.
/// @throws IllegalStateException if ptr is nullptr
template <typename EX = IllegalStateException, typename T>
T& toRef(T* ptr, const char* message = "Can not dereference a null pointer") {
    if (ptr == nullptr) throw EX(message);
    return *ptr;
}

#ifdef OBX_CPP_FILE
[[noreturn]] void throwIllegalArgumentException(const char* text1, const char* text2) {
    std::string msg(text1);
    if (text2) msg.append(text2);
    throw IllegalArgumentException(msg);
}

[[noreturn]] void throwIllegalStateException(const char* text1, const char* text2) {
    std::string msg(text1);
    if (text2) msg.append(text2);
    throw IllegalStateException(msg);
}

[[noreturn]] void throwLastError(obx_err err, const char* contextPrefix) {
    std::string msg;
    if (contextPrefix) msg.append(contextPrefix).append(": ");

    if (err == OBX_SUCCESS) {  // Zero, there's no error actually: this is atypical corner case, which should be avoided
        msg += "No error occurred (operation was successful)";
        throw IllegalStateException(msg);
    } else {
        appendLastErrorText(err, msg);
        throwError(err, msg);
    }
}

void appendLastErrorText(obx_err err, std::string& outMessage) {
    obx_err lastErr = obx_last_error_code();
    if (err == lastErr) {
        assert(lastErr != 0);  // checked indirectly against err before
        outMessage += obx_last_error_message();
    } else {  // Do not use obx_last_error_message() as primary msg because it originated from another code
        outMessage.append("Error code ").append(std::to_string(err));
        if (lastErr != 0) {
            outMessage.append(" (last: ").append(std::to_string(lastErr));
            outMessage.append(", last msg: ").append(obx_last_error_message()).append(")");
        }
    }
}

[[noreturn]] void throwError(obx_err err, const std::string& message) {
    if (err == OBX_SUCCESS) {  // Zero, there's no error actually: this is atypical corner case, which should be avoided
        throw IllegalStateException("No error occurred; operation was successful. Given message: " + message);
    } else {
        if (err == OBX_ERROR_ILLEGAL_ARGUMENT) {
            throw IllegalArgumentException(message);
        } else if (err == OBX_ERROR_ILLEGAL_STATE) {
            throw IllegalStateException(message);
        } else if (err == OBX_ERROR_SHUTTING_DOWN) {
            throw ShuttingDownException(message);
        } else if (err == OBX_ERROR_MAX_DATA_SIZE_EXCEEDED) {
            throw MaxDataSizeExceededException(message);
        } else if (err == OBX_ERROR_FEATURE_NOT_AVAILABLE) {
            throw FeatureNotAvailableException(message);
        } else {
            throw DbException(message, err);
        }
    }
}

void checkErrOrThrow(obx_err err) {
    if (err != OBX_SUCCESS) throwLastError(err);
}

bool checkSuccessOrThrow(obx_err err) {
    if (err == OBX_NO_SUCCESS) return false;
    if (err == OBX_SUCCESS) return true;
    throwLastError(err);
}

void checkPtrOrThrow(const void* ptr, const char* contextPrefix) {
    if (ptr == nullptr) throwLastError(obx_last_error_code(), contextPrefix);
}

void checkIdOrThrow(uint64_t id, const char* contextPrefix) {
    if (id == 0) throwLastError(obx_last_error_code(), contextPrefix);
}

#endif
}  // namespace internal

/// Bytes, which must be resolved "lazily" via get() and released via this object (destructor).
/// Unlike void* style bytes, this may represent allocated resources and/or bytes that are only produced on demand.
class BytesLazy {
    OBX_bytes_lazy* cPtr_;

public:
    BytesLazy() : cPtr_(nullptr) {}

    explicit BytesLazy(OBX_bytes_lazy* cBytes) : cPtr_(cBytes) {}

    BytesLazy(BytesLazy&& src) noexcept : cPtr_(src.cPtr_) { src.cPtr_ = nullptr; }

    /// No copying allowed: OBX_bytes_lazy needs a single owner (no method to "clone" it).
    BytesLazy(const BytesLazy& src) = delete;

    ~BytesLazy() { clear(); }

    /// @returns true if it holds actual bytes resources (e.g. not default-constructed and not clear()ed yet).
    bool hasBytes() { return cPtr_ != nullptr; }

    /// @returns true if it does not hold any bytes resources (e.g. default-constructed or already clear()ed).
    bool isNull() { return cPtr_ == nullptr; }

    void swap(BytesLazy& other) { std::swap(cPtr_, other.cPtr_); }

    /// Clears any bytes resources
    void clear() {
        obx_bytes_lazy_free(cPtr_);
        cPtr_ = nullptr;
    }

    /// Gets the bytes and its size using the given "out" references.
    void get(const void*& outBytes, size_t& outSize) const {
        if (cPtr_ == nullptr) throw IllegalStateException("This instance does not hold any bytes resources");
        internal::checkErrOrThrow(obx_bytes_lazy_get(cPtr_, &outBytes, &outSize));
    }

    /// Note that this will potentially resolve actual bytes just like get().
    /// Also, it would be more efficient to only call get() to get everything in a single call.
    size_t size() {
        if (cPtr_ == nullptr) throw IllegalStateException("This instance does not hold any bytes resources");
        size_t size = 0;
        internal::checkErrOrThrow(obx_bytes_lazy_get(cPtr_, nullptr, &size));
        return size;
    }
};

namespace {
template <typename EntityT>
constexpr obx_schema_id entityId() {
    return EntityT::_OBX_MetaInfo::entityId();
}
}  // namespace

template <class T>
class Box;

class BoxTypeless;

template <class T>
class AsyncBox;

class Transaction;

class Sync;
class SyncClient;
class SyncServer;

class Closable {
public:
    virtual ~Closable() = default;
    virtual bool isClosed() = 0;
    virtual void close() = 0;
};

using ObxLogCallback = std::function<void(OBXLogLevel logLevel, const char* text, size_t textSize)>;

/// Options provide a way to configure Store when opening it.
/// Options functions can be chained, e.g. options.directory("mypath/objectbox").maxDbSizeInKb(2048);
/// Note: Options objects can be used only once to create a store as they are "consumed" during Store creation.
///       Thus, you need to create a new Option object for each Store that is created.
class Options {
    friend class Store;
    friend class SyncServer;

    mutable OBX_store_options* opt = nullptr;

    OBX_store_options* release() {
        OBX_store_options* result = opt;
        opt = nullptr;
        return result;
    }

public:
    Options() {
        opt = obx_opt();
        internal::checkPtrOrThrow(opt, "Could not create store options");
    }

    /// @deprecated is this used by generator?
    explicit Options(OBX_model* model) : Options() { this->model(model); }

    ~Options() { obx_opt_free(opt); }

    /// Set the model on the options. The default is no model.
    /// NOTE: the model is always freed by this function, including when an error occurs.
    Options& model(OBX_model* model) {
        internal::checkErrOrThrow(obx_opt_model(opt, model));
        return *this;
    }

    /// Set the store directory on the options. The default is "objectbox".
    /// Use the prefix "memory:" to open an in-memory database, e.g. "memory:myApp" (see docs for details).
    Options& directory(const char* dir) {
        internal::checkErrOrThrow(obx_opt_directory(opt, dir));
        return *this;
    }

    /// Set the store directory on the options. The default is "objectbox".
    /// Use the prefix "memory:" to open an in-memory database, e.g. "memory:myApp" (see docs for details).
    Options& directory(const std::string& dir) { return directory(dir.c_str()); }

    /// Gets the option for "directory"; this is either the default, or, the value set by directory().
    std::string getDirectory() const {
        const char* dir = obx_opt_get_directory(opt);
        internal::checkPtrOrThrow(dir, "Could not get directory");
        return dir;
    }

    /// Set the maximum db size on the options. The default is 1Gb.
    Options& maxDbSizeInKb(uint64_t sizeInKb) {
        obx_opt_max_db_size_in_kb(opt, sizeInKb);
        return *this;
    }

    /// Gets the option for "max DB size"; this is either the default, or, the value set by maxDbSizeInKb().
    uint64_t getMaxDbSizeInKb() const { return obx_opt_get_max_db_size_in_kb(opt); }

    /// Data size tracking is more involved than DB size tracking, e.g. it stores an internal counter.
    /// Thus only use it if a stricter, more accurate limit is required (it's off by default).
    /// It tracks the size of actual data bytes of objects (system and metadata is not considered).
    /// On the upside, reaching the data limit still allows data to be removed (assuming DB limit is not reached).
    /// Max data and DB sizes can be combined; data size must be below the DB size.
    Options& maxDataSizeInKb(uint64_t sizeInKb) {
        obx_opt_max_data_size_in_kb(opt, sizeInKb);
        return *this;
    }

    /// Gets the option for "max DB size"; this is either the default, or, the value set by maxDataSizeInKb().
    uint64_t getMaxDataSizeInKb() const { return obx_opt_get_max_data_size_in_kb(opt); }

    /// Set the file mode on the options. The default is 0644 (unix-style)
    Options& fileMode(unsigned int fileMode) {
        obx_opt_file_mode(opt, fileMode);
        return *this;
    }

    /// Set the maximum number of readers (related to read transactions.
    /// "Readers" are an finite resource for which we need to define a maximum number upfront.
    /// The default value is enough for most apps and usually you can ignore it completely.
    /// However, if you get the OBX_ERROR_MAX_READERS_EXCEEDED error, you should verify your threading.
    /// For each thread, ObjectBox uses multiple readers.
    /// Their number (per thread) depends on number of types, relations, and usage patterns.
    /// Thus, if you are working with many threads (e.g. in a server-like scenario), it can make sense to increase
    /// the maximum number of readers.
    ///
    /// \note The internal default is currently 126. So when hitting this limit, try values around 200-500.
    ///
    /// \attention Each thread that performed a read transaction and is still alive holds on to a reader slot.
    ///       These slots only get vacated when the thread ends. Thus be mindful with the number of active threads.
    ///       Alternatively, you can opt to try the experimental noReaderThreadLocals option flag.
    Options& maxReaders(unsigned int maxReaders) {
        obx_opt_max_readers(opt, maxReaders);
        return *this;
    }

    /// Disables the usage of thread locals for "readers" related to read transactions.
    /// This can make sense if you are using a lot of threads that are kept alive.
    /// \note This is still experimental, as it comes with subtle behavior changes at a low level and may affect
    ///       corner cases with e.g. transactions, which may not be fully tested at the moment.
    Options& noReaderThreadLocals(bool flag) {
        obx_opt_no_reader_thread_locals(opt, flag);
        return *this;
    }

    /// Set the model on the options copying the given bytes. The default is no model.
    Options& modelBytes(const void* bytes, size_t size) {
        internal::checkErrOrThrow(obx_opt_model_bytes(opt, bytes, size));
        return *this;
    }

    /// Like modelBytes() BUT WITHOUT copying the given bytes.
    /// Thus, you must keep the bytes available until after the store is created.
    Options& modelBytesDirect(const void* bytes, size_t size) {
        internal::checkErrOrThrow(obx_opt_model_bytes_direct(opt, bytes, size));
        return *this;
    }

    /// When the DB is opened initially, ObjectBox can do a consistency check on the given amount of pages.
    /// Reliable file systems already guarantee consistency, so this is primarily meant to deal with unreliable
    /// OSes, file systems, or hardware. Thus, usually a low number (e.g. 1-20) is sufficient and does not impact
    /// startup performance significantly. To completely disable this you can pass 0, but we recommend a setting of
    /// at least 1.
    /// Note: ObjectBox builds upon ACID storage, which guarantees consistency given that the file system is working
    /// correctly (in particular fsync).
    /// @param pageLimit limits the number of checked pages (currently defaults to 0, but will be increased in the
    /// future)
    /// @param flags flags used to influence how the validation checks are performed
    Options& validateOnOpenPages(size_t pageLimit, uint32_t flags = OBXValidateOnOpenPagesFlags_None) {
        obx_opt_validate_on_open_pages(opt, pageLimit, flags);
        return *this;
    }

    /// When the DB is opened initially, ObjectBox can do a validation over the key/value pairs to check, for example,
    /// whether they're consistent towards our internal specification.
    /// @param flags flags used to influence how the validation checks are performed;
    ///        only OBXValidateOnOpenKvFlags_None is supported for now.
    Options& validateOnOpenKv(uint32_t flags = OBXValidateOnOpenKvFlags_None) {
        obx_opt_validate_on_open_kv(opt, flags);
        return *this;
    }

    /// Don't touch unless you know exactly what you are doing:
    /// Advanced setting typically meant for language bindings (not end users). See OBXPutPaddingMode description.
    Options& putPaddingMode(OBXPutPaddingMode mode) {
        obx_opt_put_padding_mode(opt, mode);
        return *this;
    }

    /// Advanced setting meant only for special scenarios: setting to false causes opening the database in a
    /// limited, schema-less mode. If you don't know what this means exactly: ignore this flag. Defaults to true.
    Options& readSchema(bool value) {
        obx_opt_read_schema(opt, value);
        return *this;
    }

    /// Advanced setting recommended to be used together with read-only mode to ensure no data is lost.
    /// Ignores the latest data snapshot (committed transaction state) and uses the previous snapshot instead.
    /// When used with care (e.g. backup the DB files first), this option may also recover data removed by the
    /// latest transaction. Defaults to false.
    Options& usePreviousCommit(bool value) {
        obx_opt_use_previous_commit(opt, value);
        return *this;
    }

    /// Open store in read-only mode: no schema update, no write transactions. Defaults to false.
    Options& readOnly(bool value) {
        obx_opt_read_only(opt, value);
        return *this;
    }

    /// Configure debug flags; e.g. to influence logging. Defaults to NONE.
    Options& debugFlags(uint32_t flags) {
        obx_opt_debug_flags(opt, flags);
        return *this;
    }

    /// Adds debug flags to potentially existing ones (that were previously set).
    Options& addDebugFlags(uint32_t flags) {
        obx_opt_add_debug_flags(opt, flags);
        return *this;
    }

    /// Gets the option for "debug flags"; this is either the default, or, the value set by debugFlags().
    uint32_t getDebugFlags() const { return obx_opt_get_debug_flags(opt); }

    /// Maximum of async elements in the queue before new elements will be rejected.
    /// Hitting this limit usually hints that async processing cannot keep up;
    /// data is produced at a faster rate than it can be persisted in the background.
    /// In that case, increasing this value is not the only alternative; other values might also optimize
    /// throughput. For example, increasing maxInTxDurationMicros may help too.
    Options& asyncMaxQueueLength(size_t value) {
        obx_opt_async_max_queue_length(opt, value);
        return *this;
    }

    /// Producers (AsyncTx submitter) is throttled when the queue size hits this
    Options& asyncThrottleAtQueueLength(size_t value) {
        obx_opt_async_throttle_at_queue_length(opt, value);
        return *this;
    }

    /// Sleeping time for throttled producers on each submission
    Options& asyncThrottleMicros(uint32_t value) {
        obx_opt_async_throttle_micros(opt, value);
        return *this;
    }

    /// Maximum duration spent in a transaction before AsyncQ enforces a commit.
    /// This becomes relevant if the queue is constantly populated at a high rate.
    Options& asyncMaxInTxDuration(uint32_t micros) {
        obx_opt_async_max_in_tx_duration(opt, micros);
        return *this;
    }

    /// Maximum operations performed in a transaction before AsyncQ enforces a commit.
    /// This becomes relevant if the queue is constantly populated at a high rate.
    Options& asyncMaxInTxOperations(uint32_t value) {
        obx_opt_async_max_in_tx_operations(opt, value);
        return *this;
    }

    /// Before the AsyncQ is triggered by a new element in queue to starts a new run, it delays actually starting
    /// the transaction by this value. This gives a newly starting producer some time to produce more than one a
    /// single operation before AsyncQ starts. Note: this value should typically be low to keep latency low and
    /// prevent accumulating too much operations.
    Options& asyncPreTxnDelay(uint32_t delayMicros) {
        obx_opt_async_pre_txn_delay(opt, delayMicros);
        return *this;
    }

    /// Before the AsyncQ is triggered by a new element in queue to starts a new run, it delays actually starting
    /// the transaction by this value. This gives a newly starting producer some time to produce more than one a
    /// single operation before AsyncQ starts. Note: this value should typically be low to keep latency low and
    /// prevent accumulating too much operations.
    Options& asyncPreTxnDelay(uint32_t delayMicros, uint32_t delay2Micros, size_t minQueueLengthForDelay2) {
        obx_opt_async_pre_txn_delay4(opt, delayMicros, delay2Micros, minQueueLengthForDelay2);
        return *this;
    }

    /// Similar to preTxDelay but after a transaction was committed.
    /// One of the purposes is to give other transactions some time to execute.
    /// In combination with preTxDelay this can prolong non-TX batching time if only a few operations are around.
    Options& asyncPostTxnDelay(uint32_t delayMicros) {
        obx_opt_async_post_txn_delay(opt, delayMicros);
        return *this;
    }

    /// Similar to preTxDelay but after a transaction was committed.
    /// One of the purposes is to give other transactions some time to execute.
    /// In combination with preTxDelay this can prolong non-TX batching time if only a few operations are around.
    /// @param subtractProcessingTime If set, the delayMicros is interpreted from the start of TX processing.
    ///        In other words, the actual delay is delayMicros minus the TX processing time including the commit.
    ///        This can make timings more accurate (e.g. when fixed batching interval are given).

    Options& asyncPostTxnDelay(uint32_t delayMicros, uint32_t delay2Micros, size_t minQueueLengthForDelay2,
                               bool subtractProcessingTime = false) {
        obx_opt_async_post_txn_delay5(opt, delayMicros, delay2Micros, minQueueLengthForDelay2, subtractProcessingTime);
        return *this;
    }

    /// Numbers of operations below this value are considered "minor refills"
    Options& asyncMinorRefillThreshold(size_t queueLength) {
        obx_opt_async_minor_refill_threshold(opt, queueLength);
        return *this;
    }

    /// If non-zero, this allows "minor refills" with small batches that came in (off by default).
    Options& asyncMinorRefillMaxCount(uint32_t value) {
        obx_opt_async_minor_refill_max_count(opt, value);
        return *this;
    }

    /// Default value: 10000, set to 0 to deactivate pooling
    Options& asyncMaxTxPoolSize(size_t value) {
        obx_opt_async_max_tx_pool_size(opt, value);
        return *this;
    }

    /// Total cache size; default: ~ 0.5 MB
    Options& asyncObjectBytesMaxCacheSize(uint64_t value) {
        obx_opt_async_object_bytes_max_cache_size(opt, value);
        return *this;
    }

    /// Maximal size for an object to be cached (only cache smaller ones)
    Options& asyncObjectBytesMaxSizeToCache(uint64_t value) {
        obx_opt_async_object_bytes_max_size_to_cache(opt, value);
        return *this;
    }

    /// Registers a log callback, which is called for a selection of log events.
    /// Note: this does not replace the default logging, which is much more extensive (at least at this point).
    Options& logCallback(obx_log_callback* callback, void* userData) {
        obx_opt_log_callback(opt, callback, userData);
        return *this;
    }

    /// Before opening the database, this options instructs to restore the database content from the given backup file.
    /// Note: backup is a server-only feature.
    /// By default, actually restoring the backup is only performed if no database already exists
    /// (database does not contain data).
    /// @param flags For default behavior pass 0, or adjust defaults using OBXBackupRestoreFlags bit flags,
    ///        e.g., to overwrite all existing data in the database.
    Options& backupRestore(const char* backupFile, uint32_t flags = 0) {
        obx_opt_backup_restore(opt, backupFile, flags);
        return *this;
    }

    /// Enables Write-ahead logging (WAL); for now this is only supported for in-memory DBs.
    /// @param flags WAL itself is enabled by setting flag OBXWalFlags_EnableWal (also the default parameter value).
    ///        Combine with other flags using bitwise OR or switch off WAL by passing 0.
    Options& wal(uint32_t flags = OBXWalFlags_EnableWal) {
        obx_opt_wal(opt, flags);
        return *this;
    }

    /// The WAL file gets consolidated when it reached this size limit when opening the database.
    /// This setting is meant for applications that prefer to consolidate on startup,
    /// which may avoid consolidations on commits while the application is running.
    /// The default is 4096 (4 MB).
    Options& walMaxFileSizeOnOpenInKb(uint64_t size_in_kb) {
        obx_opt_wal_max_file_size_on_open_in_kb(opt, size_in_kb);
        return *this;
    }

    /// The WAL file gets consolidated when it reaches this size limit after a commit.
    /// As consolidation takes some time, it is a trade-off between accumulating enough data
    /// and the time the consolidation takes (longer with more data).
    /// The default is 16384 (16 MB).
    Options& walMaxFileSizeInKb(uint64_t size_in_kb) {
        obx_opt_wal_max_file_size_in_kb(opt, size_in_kb);
        return *this;
    }
};

/// Transactions can be started in read (only) or write mode.
enum class TxMode { READ, WRITE };

/// \brief A ObjectBox store represents a database storing data in a given directory on a local file system.
///
/// Once opened using one of the constructors, Store is an entry point to data access APIs such as Box, Query, and
/// Transaction.
///
/// It's possible open multiple stores in different directories, e.g. at the same time.
class Store {
    std::atomic<OBX_store*> cStore_;
    const bool owned_;  ///< whether the store pointer is owned (true except for SyncServer::store())
    std::shared_ptr<Closable> syncClient_;
    std::mutex syncClientMutex_;

    friend Sync;
    friend SyncClient;
    friend SyncServer;

    explicit Store(OBX_store* ptr, bool owned) : cStore_(ptr), owned_(owned) {
        OBX_VERIFY_ARGUMENT(cStore_ != nullptr);
    }

public:
    /// Return the (runtime) version of the library to be printed.
    /// The current format is "major.minor.patch" (e.g. "1.0.0") but may change in any future release.
    /// Thus, only use for information purposes.
    /// @see getVersion() for integer based versions
    static const char* versionCString() { return obx_version_string(); }

    /// Creates a new string containing versionCString()
    static std::string versionString() { return std::string(versionCString()); }

    /// Return the version of the ObjectBox core to be printed (currently also contains a version date and features).
    /// The format may change in any future release; only use for information purposes.
    static const char* versionCoreCString() { return obx_version_core_string(); }

    /// Creates a new string containing versionCoreCString()
    static std::string versionCoreString() { return std::string(versionCoreCString()); }

    /// Return the version of the library as ints. Pointers may be null
    static void getVersion(int* major, int* minor, int* patch) { obx_version(major, minor, patch); }

    /// Enable (or disable) debug logging for ObjectBox internals.
    /// This requires a version of the library with the DebugLog feature.
    /// You can check if the feature is available with obx_has_feature(OBXFeature_DebugLog).
    static void debugLog(bool enabled) { internal::checkErrOrThrow(obx_debug_log(enabled)); }

    /// Checks if debug logs are enabled for ObjectBox internals.
    /// This depends on the availability of the DebugLog feature.
    /// If the feature is available, it returns the current state, which is adjustable via obx_debug_log().
    /// Otherwise, it always returns false for standard release builds
    /// (or true if you are having a special debug version).
    static bool debugLogEnabled() { return obx_debug_log_enabled(); }

    /// Delete the store files from the given directory
    static void removeDbFiles(const std::string& directory) {
        internal::checkErrOrThrow(obx_remove_db_files(directory.c_str()));
    }

    static size_t getDbFileSize(const std::string& directory) { return obx_db_file_size(directory.c_str()); }

    // -- Instance methods ---------------------------------------------------

    /// Creates a Store with the given model and default Options.
    explicit Store(OBX_model* model) : Store(Options().model(model)) {}

    /// Creates a Store with the given Options, which also contain the data model.
    explicit Store(Options& options)
        : Store(internal::checkedPtrOrThrow(obx_store_open(options.release()), "Can not open store"), true) {}

    /// Creates a Store with the given Options, which also contain the data model.
    explicit Store(Options&& options) : Store(options) {}

    /// Wraps an existing C-API store pointer, taking ownership (don't close it manually anymore)
    explicit Store(OBX_store* cStore) : Store(cStore, true) {}

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    Store(const Store&) = delete;

    Store(Store&& source) noexcept;

    virtual ~Store();

    /// @throws ShuttingDownException if the Store is closed (close() was call on the store).
    OBX_store* cPtr() const;

    /// @returns non-zero ID for the Store
    uint64_t id() const;

    /// Get Store type
    /// @return One of ::OBXStoreTypeId
    uint32_t getStoreTypeId() { return obx_store_type_id(cPtr()); }

    /// Get the size of the store. For a disk-based store type, this corresponds to the size on disk, and for the
    /// in-memory store type, this is roughly the used memory bytes occupied by the data.
    /// @return the size in bytes of the database, or 0 if the file does not exist or some error occurred.
    uint64_t getDbSize() const { return obx_store_size(cPtr()); }

    /// The size in bytes occupied by the database on disk (if any).
    /// @returns 0 if the underlying database is in-memory only, or the size could not be determined.
    uint64_t getDbSizeOnDisk() const { return obx_store_size_on_disk(cPtr()); }

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

    /// Does not throw if the given type name is not found (it may still throw in other conditions).
    obx_schema_id getEntityTypeIdNoThrow(const char* entityName) const {
        return obx_store_entity_id(cPtr(), entityName);
    }

    obx_schema_id getEntityTypeId(const char* entityName) const {
        obx_schema_id typeId = getEntityTypeIdNoThrow(entityName);
        if (typeId == 0) internal::throwIllegalStateException("No entity type found for name: ", entityName);
        return typeId;
    }

    /// Does not throw if the given property name is not found (it may still throw in other conditions).
    obx_schema_id getPropertyIdNoThrow(obx_schema_id entityId, const char* propertyName) const {
        return obx_store_entity_property_id(cPtr(), entityId, propertyName);
    }

    obx_schema_id getPropertyId(obx_schema_id entityId, const char* propertyName) const {
        obx_schema_id propertyId = getPropertyIdNoThrow(entityId, propertyName);
        if (propertyId == 0) internal::throwIllegalStateException("No property found for name: ", propertyName);
        return propertyId;
    }

    obx_schema_id getPropertyId(const char* entityName, const char* propertyName) const {
        return getPropertyId(getEntityTypeId(entityName), propertyName);
    }

    BoxTypeless boxTypeless(const char* entityName);

    /// Await all (including future) async submissions to be completed (the async queue becomes empty).
    /// @returns true if all submissions were completed (or async processing was not started)
    /// @returns false if shutting down or an error occurred
    bool awaitCompletion() { return obx_store_await_async_completion(cPtr()); }

    /// Await previously submitted async operations to be completed (the async queue may still contain elements).
    /// @returns true if all submissions were completed (or async processing was not started)
    /// @returns false if shutting down or an error occurred
    bool awaitSubmitted() { return obx_store_await_async_submitted(cPtr()); }

    /// Backs up the store DB to the given backup-file, using the given flags.
    /// Note: backup is a server-only feature.
    /// @param flags 0 for defaults or OBXBackupFlags bit flags
    void backUpToFile(const char* backupFile, uint32_t flags = 0) const {
        obx_err err = obx_store_back_up_to_file(cPtr(), backupFile, flags);
        internal::checkErrOrThrow(err);
    }

    /// @return an existing SyncClient associated with the store (if available; see Sync::client() to create one)
    /// @note: implemented in objectbox-sync.hpp
    std::shared_ptr<SyncClient> syncClient();

    /// Prepares the store to close by setting its internal state to "closing".
    /// Methods like tx() an boxFor() will throw ShuttingDownException once closing is initiated.
    /// Unlike close(), this method will return immediately and does not free resources just yet.
    /// This is typically used in a multi-threaded context to allow an orderly shutdown in stages which go through a
    /// "not accepting new requests" state.
    void prepareToClose();

    /// Closes all resources of this store before the destructor is called, e.g. to avoid waiting in the destructor.
    /// Avoid calling methods on the Store after this call; most methods will throw ShuttingDownException in that case.
    /// Calling close() more than once have no effect, and concurrent calls to close() are fine too.
    /// \note This waits for write transactions to finish before returning from this call.
    /// \note The Store destructor also calls close(), so you do not have to call this method explicitly unless you want
    /// to control the timing of closing resources and potentially waiting for asynchronous resources (e.g. transactions
    /// and internal queues) to finish up.
    void close();
};

#ifdef OBX_CPP_FILE

Store::Store(Store&& source) noexcept : cStore_(source.cStore_.load()), owned_(source.owned_) {
    source.cStore_ = nullptr;
    std::lock_guard<std::mutex> lock(source.syncClientMutex_);
    syncClient_ = std::move(source.syncClient_);
}

Store::~Store() { close(); }

void Store::close() {
    {
        // Clean up SyncClient by explicitly closing it, even if it isn't the only shared_ptr to the instance.
        // This prevents invalid use of store after it's been closed.
        std::shared_ptr<Closable> syncClient;
        {
            std::lock_guard<std::mutex> lock(syncClientMutex_);
            syncClient = std::move(syncClient_);
            syncClient_ = nullptr;  // to make the current state obvious
        }

        if (syncClient && !syncClient->isClosed()) {
#ifndef NDEBUG  // todo we probably want our LOG macros here too
            long useCount = syncClient.use_count();
            if (useCount > 1) {  // print external refs only thus "- 1"
                printf("SyncClient still active with %ld references when store got closed\n", (useCount - 1));
            }
#endif  // NDEBUG
            syncClient->close();
        }
    }

    if (owned_) {
        OBX_store* storeToClose = cStore_.exchange(nullptr);  // Close exactly once
        obx_store_close(storeToClose);
    }
}

OBX_store* Store::cPtr() const {
    OBX_store* store = cStore_.load();
    if (store == nullptr) throw ShuttingDownException("Store is already closed");
    return store;
}

uint64_t Store::id() const {
    uint64_t id = obx_store_id(cPtr());
    internal::checkIdOrThrow(id);
    return id;
}

void Store::prepareToClose() {
    obx_err err = obx_store_prepare_to_close(cPtr());
    internal::checkErrOrThrow(err);
}

#endif

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
    TxMode mode_;
    OBX_txn* cTxn_;

public:
    Transaction(Store& store, TxMode mode);

    /// Delete because the default copy constructor can break things (i.e. a Transaction can not be copied).
    Transaction(const Transaction&) = delete;

    /// Move constructor, used by Store::tx()
    Transaction(Transaction&& source) noexcept : mode_(source.mode_), cTxn_(source.cTxn_) { source.cTxn_ = nullptr; }

    /// Copy-and-swap style
    Transaction& operator=(Transaction source);

    /// Never throws
    virtual ~Transaction() { closeNoThrow(); };

    /// A Transaction is active if it was not ended via success(), close() or moving.
    bool isActive() { return cTxn_ != nullptr; }

    /// The transaction pointer of the ObjectBox C API.
    /// @throws if this Transaction was already closed or moved
    OBX_txn* cPtr() const;

    /// "Finishes" this write transaction successfully; performs a commit if this is the top level transaction and all
    /// inner transactions (if any) were also successful. This object will also be "closed".
    /// @throws Exception if this is not a write TX or it was closed before (e.g. via success()).
    void success();

    /// Explicit close to free up resources (non-throwing version).
    /// It's OK to call this method multiple times; additional calls will have no effect.
    obx_err closeNoThrow();

    /// Explicit close to free up resources; unlike closeNoThrow() (which is also called by the destructor), this
    /// version throw in the unlikely case of failing.
    /// It's OK to call this method multiple times; additional calls will have no effect.
    void close();

    /// The data size of the committed state (not updated for this transaction).
    uint64_t getDataSizeCommitted() const;

    /// Cumulative change (delta) of data size by this pending transaction (uncommitted).
    int64_t getDataSizeChange() const;
};

#ifdef OBX_CPP_FILE

Transaction::Transaction(Store& store, TxMode mode)
    : mode_(mode), cTxn_(mode == TxMode::WRITE ? obx_txn_write(store.cPtr()) : obx_txn_read(store.cPtr())) {
    internal::checkPtrOrThrow(cTxn_, "Can not start transaction");
}

Transaction& Transaction::operator=(Transaction source) {
    std::swap(mode_, source.mode_);
    std::swap(cTxn_, source.cTxn_);
    return *this;
}

OBX_txn* Transaction::cPtr() const {
    OBX_VERIFY_STATE(cTxn_);
    return cTxn_;
}

void Transaction::success() {
    OBX_txn* txn = cTxn_;
    OBX_VERIFY_STATE(txn);
    cTxn_ = nullptr;
    internal::checkErrOrThrow(obx_txn_success(txn));
}

obx_err Transaction::closeNoThrow() {
    OBX_txn* txnToClose = cTxn_;
    cTxn_ = nullptr;
    return obx_txn_close(txnToClose);
}

void Transaction::close() { internal::checkErrOrThrow(closeNoThrow()); }

uint64_t Transaction::getDataSizeCommitted() const {
    uint64_t size = 0;
    internal::checkErrOrThrow(obx_txn_data_size(cPtr(), &size, nullptr));
    return size;
}

int64_t Transaction::getDataSizeChange() const {
    int64_t sizeChange = 0;
    internal::checkErrOrThrow(obx_txn_data_size(cPtr(), nullptr, &sizeChange));
    return sizeChange;
}

Transaction Store::tx(TxMode mode) { return Transaction(*this, mode); }
Transaction Store::txRead() { return tx(TxMode::READ); }
Transaction Store::txWrite() { return tx(TxMode::WRITE); }

#endif

namespace {  // internal
/// Internal cursor wrapper for convenience and RAII.
class CursorTx {
    Transaction tx_;
    OBX_cursor* cCursor_;

public:
    explicit CursorTx(TxMode mode, Store& store, obx_schema_id entityId)
        : tx_(store, mode), cCursor_(obx_cursor(tx_.cPtr(), entityId)) {
        internal::checkPtrOrThrow(cCursor_, "Can not open cursor");
    }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    CursorTx(const CursorTx&) = delete;

    CursorTx(CursorTx&& source) noexcept : tx_(std::move(source.tx_)), cCursor_(source.cCursor_) {
        source.cCursor_ = nullptr;
    }

    virtual ~CursorTx() { obx_cursor_close(cCursor_); }

    void commitAndClose() {
        OBX_VERIFY_STATE(cCursor_);
        obx_cursor_close(cCursor_);
        cCursor_ = nullptr;
        tx_.success();
    }

    OBX_cursor* cPtr() const { return cCursor_; }

    /// @returns the first object ID or zero if there was no object
    obx_id seekToFirstId() {
        obx_id id = 0;
        obx_err err = obx_cursor_seek_first_id(cCursor_, &id);
        if (err != OBX_NOT_FOUND) internal::checkErrOrThrow(err);  // return 0 if not found (non-exceptional outcome)
        return id;
    }

    /// @returns the next object ID or zero if there was no next object
    obx_id seekToNextId() {
        obx_id id = 0;
        obx_err err = obx_cursor_seek_next_id(cCursor_, &id);
        if (err != OBX_NOT_FOUND) internal::checkErrOrThrow(err);  // return 0 if not found (non-exceptional outcome)
        return id;
    }

    /// Gets the object ID at the current position; ensures being up-to-date by verifying against database.
    /// If the cursor is not positioned at an actual object, it returns one of two special IDs instead.
    /// @returns 0 (OBJECT_ID_BEFORE_START) if the cursor was not yet moved or reached the end (going backwards).
    /// @returns 0xFFFFFFFFFFFFFFFF (OBJECT_ID_BEYOND_END) if the cursor reached the end (going forwards).
    obx_id getCurrentId() {
        obx_id id = 0;
        obx_err err = obx_cursor_current_id(cCursor_, &id);
        if (err != OBX_NOT_FOUND) internal::checkErrOrThrow(err);  // return 0 if not found (non-exceptional outcome)
        return id;
    }
};

/// Collects all visited data; returns a vector of plain objects.
template <typename EntityT>
struct CollectingVisitor {
    std::vector<EntityT> items;

    static bool visit(const void* data, size_t size, void* userData) {
        CollectingVisitor<EntityT>* self = static_cast<CollectingVisitor<EntityT>*>(userData);
        assert(self);
        self->items.emplace_back();
        EntityT::_OBX_MetaInfo::fromFlatBuffer(data, size, self->items.back());
        return true;
    }
};

/// Collects all visited data; returns a vector of unique_ptr of objects.
template <typename EntityT>
struct CollectingVisitorUniquePtr {
    std::vector<std::unique_ptr<EntityT>> items;

    static bool visit(const void* data, size_t size, void* userData) {
        CollectingVisitorUniquePtr<EntityT>* self = static_cast<CollectingVisitorUniquePtr<EntityT>*>(userData);
        assert(self);
        self->items.emplace_back(new EntityT());
        std::unique_ptr<EntityT>& ptrRef = self->items.back();
        EntityT::_OBX_MetaInfo::fromFlatBuffer(data, size, *ptrRef);
        return true;
    }
};

}  // namespace

namespace internal {

/// Produces an OBX_id_array with internal data referencing the given ids vector. You must
/// ensure the given vector outlives the returned OBX_id_array. Additionally, you must NOT call
/// obx_id_array_free(), because the result is not allocated by C, thus it must not free it.
const OBX_id_array cIdArrayRef(const std::vector<obx_id>& ids);

/// Consumes an OBX_id_array, producing a vector of IDs and freeing the array afterwards.
/// Must be called right after the C-API call producing cIds in order to check and throw on error correctly.
/// Example: idVectorOrThrow(obx_query_find_ids(cQuery_, offset_, limit_))
/// Note: even if this function throws the given OBX_id_array is freed.
std::vector<obx_id> idVectorOrThrow(OBX_id_array* cIds);

#ifdef OBX_CPP_FILE
const OBX_id_array cIdArrayRef(const std::vector<obx_id>& ids) {
    // Note: removing const from ids.data() to match the C struct, but returning struct as const; so it should be OK:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return {ids.empty() ? nullptr : const_cast<obx_id*>(ids.data()), ids.size()};
}

std::vector<obx_id> idVectorOrThrow(OBX_id_array* cIds) {
    if (cIds == nullptr) internal::throwLastError();

    try {
        std::vector<obx_id> result;
        if (cIds->count > 0) {
            result.resize(cIds->count);
            OBX_VERIFY_STATE(result.size() == cIds->count);
            memcpy(result.data(), cIds->ids, result.size() * sizeof(result[0]));
        }
        obx_id_array_free(cIds);
        return result;
    } catch (...) {
        obx_id_array_free(cIds);
        throw;
    }
}
#endif

}  // namespace internal

namespace {

class QueryCondition;
class QCGroup;

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

class QCGroup : public QueryCondition {
    bool isOr_;  // whether it's an "AND" or "OR" group

    // Must be a vector of pointers because QueryCondition is abstract - we can't have a vector of abstract objects.
    // Must be shared_ptr for our own copyAsPtr() to work, in other words vector of unique pointers can't be copied.
    std::vector<std::shared_ptr<QueryCondition>> conditions_;

public:
    QCGroup(bool isOr, std::unique_ptr<QueryCondition>&& a, std::unique_ptr<QueryCondition>&& b)
        : isOr_(isOr), conditions_({std::move(a), std::move(b)}) {}

    // override to combine multiple chained AND conditions into a same group
    QCGroup and_(const QueryCondition& other) override {
        // if this group is an OR group and we're adding an AND - create a new group
        if (isOr_) return QueryCondition::and_(other);

        // otherwise, extend this one by making a copy and including the new condition in it
        return copyThisAndPush(other);
    }

    // we don't have to create copies of the QCGroup when the left-hand-side can be "consumed and moved" (rvalue ref)
    // Note: this is a "global" function, but declared here as a friend so it can access lhs.conditions
    friend inline QCGroup operator&&(QCGroup&& lhs, const QueryCondition& rhs) {
        if (lhs.isOr_) return lhs.and_(rhs);
        lhs.conditions_.push_back(internalCopyAsPtr(rhs));
        return std::move(lhs);
    }

    // override to combine multiple chained OR conditions into a same group
    QCGroup or_(const QueryCondition& other) override {
        // if this group is an AND group and we're adding an OR - create a new group
        if (!isOr_) return QueryCondition::or_(other);

        // otherwise, extend this one by making a copy and including the new condition in it
        return copyThisAndPush(other);
    }

    // we don't have to create copies of the QCGroup when the left-hand-side can be "consumed and moved" (rvalue ref)
    // Note: this is a "global" function, but declared here as a friend so it can access lhs.conditions
    friend inline QCGroup operator||(QCGroup&& lhs, const QueryCondition& rhs) {
        if (!lhs.isOr_) return lhs.or_(rhs);
        lhs.conditions_.push_back(internalCopyAsPtr(rhs));
        return std::move(lhs);
    }

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override {
        return std::unique_ptr<QueryCondition>(new QCGroup(*this));
    };

    QCGroup copyThisAndPush(const QueryCondition& other) {
        QCGroup copy(*this);
        copy.conditions_.push_back(internalCopyAsPtr(other));
        return copy;
    }

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool isRoot) const override {
        if (conditions_.size() == 1) return internalApplyCondition(*conditions_[0], cqb, isRoot);
        OBX_VERIFY_STATE(!conditions_.empty());

        std::vector<obx_qb_cond> cond_ids;
        cond_ids.reserve(conditions_.size());
        for (const std::shared_ptr<QueryCondition>& cond : conditions_) {
            cond_ids.emplace_back(internalApplyCondition(*cond, cqb, false));
        }
        if (isRoot && !isOr_) {
            // root All (AND) is implicit so no need to actually combine the conditions explicitly
            return 0;
        }

        if (isOr_) return obx_qb_any(cqb, cond_ids.data(), cond_ids.size());
        return obx_qb_all(cqb, cond_ids.data(), cond_ids.size());
    }
};

QCGroup obx::QueryCondition::and_(const QueryCondition& other) {
    return {false, copyAsPtr(), internalCopyAsPtr(other)};
}
inline QCGroup obx::QueryCondition::operator&&(const QueryCondition& rhs) { return and_(rhs); }

QCGroup obx::QueryCondition::or_(const QueryCondition& other) { return {true, copyAsPtr(), internalCopyAsPtr(other)}; }
inline QCGroup obx::QueryCondition::operator||(const QueryCondition& rhs) { return or_(rhs); }

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
    NotNull,
    NearestNeighbors
};

// Internal base class for all the condition containers. Each container starts with `QC` and ends with the type of the
// contents. That's not necessarily the same as the property type the condition is used with, e.g. for Bool::equals()
// using QCInt64, StringVector::contains query using QCString, etc.
class QC : public QueryCondition {
protected:
    obx_schema_id propId_;
    QueryOp op_;

public:
    QC(obx_schema_id propId, QueryOp op) : propId_(propId), op_(op) {}
    ~QC() override = default;

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override {
        return std::unique_ptr<QueryCondition>(new QC(*this));
    };

    /// Indicates a programming error when in ObjectBox C++ binding - chosen QC struct doesn't support the desired
    /// condition. Consider changing QueryOp to a template variable - it would enable us to use static_assert() instead
    /// of the current runtime check. Additionally, it might produce better (smaller/faster) code because the compiler
    /// could optimize out all the unused switch statements and variables (`value2`).
    [[noreturn]] void throwInvalidOperation() const {
        internal::throwIllegalStateException("Invalid condition - operation not supported: ",
                                             std::to_string(static_cast<int>(op_)).c_str());
    }

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        if (op_ == QueryOp::Null) {
            return obx_qb_null(cqb, propId_);
        } else if (op_ == QueryOp::NotNull) {
            return obx_qb_not_null(cqb, propId_);
        }
        throwInvalidOperation();
    }
};

class QCInt64 : public QC {
    int64_t value1_;
    int64_t value2_;

public:
    QCInt64(obx_schema_id propId, QueryOp op, int64_t value1, int64_t value2 = 0)
        : QC(propId, op), value1_(value1), value2_(value2) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        if (op_ == QueryOp::Equal) {
            return obx_qb_equals_int(cqb, propId_, value1_);
        } else if (op_ == QueryOp::NotEqual) {
            return obx_qb_not_equals_int(cqb, propId_, value1_);
        } else if (op_ == QueryOp::Less) {
            return obx_qb_less_than_int(cqb, propId_, value1_);
        } else if (op_ == QueryOp::LessOrEq) {
            return obx_qb_less_or_equal_int(cqb, propId_, value1_);
        } else if (op_ == QueryOp::Greater) {
            return obx_qb_greater_than_int(cqb, propId_, value1_);
        } else if (op_ == QueryOp::GreaterOrEq) {
            return obx_qb_greater_or_equal_int(cqb, propId_, value1_);
        } else if (op_ == QueryOp::Between) {
            return obx_qb_between_2ints(cqb, propId_, value1_, value2_);
        }
        throwInvalidOperation();
    }
};

class QCDouble : public QC {
    double value1_;
    double value2_;

public:
    QCDouble(obx_schema_id propId, QueryOp op, double value1, double value2 = 0)
        : QC(propId, op), value1_(value1), value2_(value2) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        if (op_ == QueryOp::Less) {
            return obx_qb_less_than_double(cqb, propId_, value1_);
        } else if (op_ == QueryOp::LessOrEq) {
            return obx_qb_less_or_equal_double(cqb, propId_, value1_);
        } else if (op_ == QueryOp::Greater) {
            return obx_qb_greater_than_double(cqb, propId_, value1_);
        } else if (op_ == QueryOp::GreaterOrEq) {
            return obx_qb_greater_or_equal_double(cqb, propId_, value1_);
        } else if (op_ == QueryOp::Between) {
            return obx_qb_between_2doubles(cqb, propId_, value1_, value2_);
        }
        throwInvalidOperation();
    }
};

class QCInt32Array : public QC {
    std::vector<int32_t> values_;

public:
    QCInt32Array(obx_schema_id propId, QueryOp op, std::vector<int32_t>&& values)
        : QC(propId, op), values_(std::move(values)) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        if (op_ == QueryOp::In) {
            return obx_qb_in_int32s(cqb, propId_, values_.data(), values_.size());
        } else if (op_ == QueryOp::NotIn) {
            return obx_qb_not_in_int32s(cqb, propId_, values_.data(), values_.size());
        }
        throwInvalidOperation();
    }
};

class QCInt64Array : public QC {
    std::vector<int64_t> values_;

public:
    QCInt64Array(obx_schema_id propId, QueryOp op, std::vector<int64_t>&& values)
        : QC(propId, op), values_(std::move(values)) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        if (op_ == QueryOp::In) {
            return obx_qb_in_int64s(cqb, propId_, values_.data(), values_.size());
        } else if (op_ == QueryOp::NotIn) {
            return obx_qb_not_in_int64s(cqb, propId_, values_.data(), values_.size());
        }
        throwInvalidOperation();
    }
};

template <OBXPropertyType PropertyType>
class QCString : public QC {
    std::string value_;
    bool caseSensitive_;

public:
    QCString(obx_schema_id propId, QueryOp op, bool caseSensitive, std::string&& value)
        : QC(propId, op), caseSensitive_(caseSensitive), value_(std::move(value)) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        if (PropertyType == OBXPropertyType_String) {
            if (op_ == QueryOp::Equal) {
                return obx_qb_equals_string(cqb, propId_, value_.c_str(), caseSensitive_);
            } else if (op_ == QueryOp::NotEqual) {
                return obx_qb_not_equals_string(cqb, propId_, value_.c_str(), caseSensitive_);
            } else if (op_ == QueryOp::Less) {
                return obx_qb_less_than_string(cqb, propId_, value_.c_str(), caseSensitive_);
            } else if (op_ == QueryOp::LessOrEq) {
                return obx_qb_less_or_equal_string(cqb, propId_, value_.c_str(), caseSensitive_);
            } else if (op_ == QueryOp::Greater) {
                return obx_qb_greater_than_string(cqb, propId_, value_.c_str(), caseSensitive_);
            } else if (op_ == QueryOp::GreaterOrEq) {
                return obx_qb_greater_or_equal_string(cqb, propId_, value_.c_str(), caseSensitive_);
            } else if (op_ == QueryOp::StartsWith) {
                return obx_qb_starts_with_string(cqb, propId_, value_.c_str(), caseSensitive_);
            } else if (op_ == QueryOp::EndsWith) {
                return obx_qb_ends_with_string(cqb, propId_, value_.c_str(), caseSensitive_);
            } else if (op_ == QueryOp::Contains) {
                return obx_qb_contains_string(cqb, propId_, value_.c_str(), caseSensitive_);
            }
        } else if (PropertyType == OBXPropertyType_StringVector) {
            if (op_ == QueryOp::Contains) {
                return obx_qb_any_equals_string(cqb, propId_, value_.c_str(), caseSensitive_);
            }
        }
        throwInvalidOperation();
    }
};

using QCStringForString = QCString<OBXPropertyType_String>;
using QCStringForStringVector = QCString<OBXPropertyType_StringVector>;

class QCStringArray : public QC {
    std::vector<std::string> values_;  // stored string copies
    bool caseSensitive_;

public:
    QCStringArray(obx_schema_id propId, QueryOp op, bool caseSensitive, std::vector<std::string>&& values)
        : QC(propId, op), values_(std::move(values)), caseSensitive_(caseSensitive) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        // don't make an instance variable - it's not trivially copyable by copyAsPtr() and is usually called just once
        std::vector<const char*> cvalues;
        cvalues.resize(values_.size());
        for (size_t i = 0; i < values_.size(); i++) {
            cvalues[i] = values_[i].c_str();
        }
        if (op_ == QueryOp::In) {
            return obx_qb_in_strings(cqb, propId_, cvalues.data(), cvalues.size(), caseSensitive_);
        }
        throwInvalidOperation();
    }
};

class QCBytes : public QC {
    std::vector<uint8_t> value_;

public:
    QCBytes(obx_schema_id propId, QueryOp op, std::vector<uint8_t>&& value)
        : QC(propId, op), value_(std::move(value)) {}

    QCBytes(obx_schema_id propId, QueryOp op, const void* data, size_t size)
        : QC(propId, op), value_(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        if (op_ == QueryOp::Equal) {
            return obx_qb_equals_bytes(cqb, propId_, value_.data(), value_.size());
        } else if (op_ == QueryOp::Less) {
            return obx_qb_less_than_bytes(cqb, propId_, value_.data(), value_.size());
        } else if (op_ == QueryOp::LessOrEq) {
            return obx_qb_less_or_equal_bytes(cqb, propId_, value_.data(), value_.size());
        } else if (op_ == QueryOp::Greater) {
            return obx_qb_greater_than_bytes(cqb, propId_, value_.data(), value_.size());
        } else if (op_ == QueryOp::GreaterOrEq) {
            return obx_qb_greater_or_equal_bytes(cqb, propId_, value_.data(), value_.size());
        }
        throwInvalidOperation();
    }
};

class QCVectorF32 : public QC {
    const std::vector<float> value_;
    const float* valuePtr_ = nullptr;
    const size_t maxResultCount_ = 0;

public:
    /// @param value the vector with an element count that matches the dimension given for the property.
    QCVectorF32(obx_schema_id propId, QueryOp op, std::vector<float>&& value, size_t maxResultCount)
        : QC(propId, op), value_(std::move(value)), maxResultCount_(maxResultCount) {}

    /// Does not copy the value; uses the reference
    /// @param value the vector with an element count that matches the dimension given for the property.
    QCVectorF32(obx_schema_id propId, QueryOp op, const float* value, size_t maxResultCount)
        : QC(propId, op), valuePtr_(value), maxResultCount_(maxResultCount) {}

protected:
    std::unique_ptr<QueryCondition> copyAsPtr() const override { return QueryCondition::copyAsPtr(*this); };

    obx_qb_cond applyTo(OBX_query_builder* cqb, bool /*isRoot*/) const override {
        if (op_ == QueryOp::NearestNeighbors) {
            const float* data = valuePtr_ ? valuePtr_ : value_.data();
            return obx_qb_nearest_neighbors_f32(cqb, propId_, data, maxResultCount_);
        }
        throwInvalidOperation();
    }
};

// enable_if_t missing in c++11 so let's have a shorthand here
template <bool Condition, typename T = void>
using enable_if_t = typename std::enable_if<Condition, T>::type;

template <OBXPropertyType T, bool includingRelation = false>
using EnableIfInteger =
    enable_if_t<T == OBXPropertyType_Int || T == OBXPropertyType_Long || T == OBXPropertyType_Short ||
                T == OBXPropertyType_Byte || T == OBXPropertyType_Bool || T == OBXPropertyType_Date ||
                T == OBXPropertyType_DateNano || (includingRelation && T == OBXPropertyType_Relation)>;

template <OBXPropertyType T>
using EnableIfIntegerOrRel = EnableIfInteger<T, true>;

template <OBXPropertyType T>
using EnableIfFloating = enable_if_t<T == OBXPropertyType_Float || T == OBXPropertyType_Double>;

template <OBXPropertyType T>
using EnableIfDate = enable_if_t<T == OBXPropertyType_Date || T == OBXPropertyType_DateNano>;

constexpr OBXPropertyType typeless = static_cast<OBXPropertyType>(0);
}  // namespace

/// Typeless property used as a base class for other types - sharing common conditions.
class PropertyTypeless {
protected:
    /// property ID
    const obx_schema_id id_;

public:
    explicit constexpr PropertyTypeless(obx_schema_id id) noexcept : id_(id) {}
    inline obx_schema_id id() const { return id_; }

    QC isNull() const { return {id_, QueryOp::Null}; }
    QC isNotNull() const { return {id_, QueryOp::NotNull}; }
};

/// Carries property information when used in the entity-meta ("underscore") class
template <typename EntityT, OBXPropertyType ValueT>
class Property : public PropertyTypeless {
public:
    explicit constexpr Property(obx_schema_id id) noexcept : PropertyTypeless(id) {}

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
    QCInt64 lessOrEq(int64_t value) const {
        return {this->id_, QueryOp::LessOrEq, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfInteger<T>>
    QCInt64 greaterThan(int64_t value) const {
        return {this->id_, QueryOp::Greater, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfInteger<T>>
    QCInt64 greaterOrEq(int64_t value) const {
        return {this->id_, QueryOp::GreaterOrEq, value};
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
    QCDouble lessOrEq(double value) const {
        return {this->id_, QueryOp::LessOrEq, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfFloating<T>>
    QCDouble greaterThan(double value) const {
        return {this->id_, QueryOp::Greater, value};
    }

    template <OBXPropertyType T = ValueT, typename = EnableIfFloating<T>>
    QCDouble greaterOrEq(double value) const {
        return {this->id_, QueryOp::GreaterOrEq, value};
    }

    /// finds objects with property value between a and b (including a and b)
    template <OBXPropertyType T = ValueT, typename = EnableIfFloating<T>>
    QCDouble between(double a, double b) const {
        return {this->id_, QueryOp::Between, a, b};
    }
};

/// Carries property information when used in the entity-meta ("underscore") class
template <typename EntityT>
class Property<EntityT, OBXPropertyType_String> : public PropertyTypeless {
public:
    explicit constexpr Property(obx_schema_id id) noexcept : PropertyTypeless(id) {}

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
class Property<EntityT, OBXPropertyType_ByteVector> : public PropertyTypeless {
public:
    explicit constexpr Property(obx_schema_id id) noexcept : PropertyTypeless(id) {}

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
class Property<EntityT, OBXPropertyType_StringVector> : public PropertyTypeless {
public:
    explicit constexpr Property(obx_schema_id id) noexcept : PropertyTypeless(id) {}

    QCStringForStringVector contains(std::string&& value, bool caseSensitive = true) const {
        return {this->id_, QueryOp::Contains, caseSensitive, std::move(value)};
    }

    QCStringForStringVector contains(const std::string& value, bool caseSensitive = true) const {
        return contains(std::string(value), caseSensitive);
    }
};

/// Carries property information when used in the entity-meta ("underscore") class
template <typename EntityT>
class Property<EntityT, OBXPropertyType_FloatVector> : public PropertyTypeless {
public:
    explicit constexpr Property(obx_schema_id id) noexcept : PropertyTypeless(id) {}

    /// @param value the vector with an element count that matches the dimension given for the property.
    /// @param maxNeighborCount Maximal number of nearest neighbors to search for.
    QCVectorF32 nearestNeighbors(std::vector<float>&& value, size_t maxNeighborCount) const {
        return {this->id_, QueryOp::NearestNeighbors, std::move(value), maxNeighborCount};
    }

    /// Note: The vector data is NOT copied but referenced, so keep it until the Query is built.
    /// @param value the vector with an element count that matches the dimension given for the property.
    /// @param maxNeighborCount Maximal number of nearest neighbors to search for.
    QCVectorF32 nearestNeighbors(const std::vector<float>& value, size_t maxNeighborCount) const {
        return {this->id_, QueryOp::NearestNeighbors, value.data(), maxNeighborCount};
    }

    /// Note: The vector data is NOT copied but referenced, so keep it until the Query is built.
    /// @param value the vector with an element count that matches the dimension given for the property.
    /// @param maxNeighborCount Maximal number of nearest neighbors to search for.
    QCVectorF32 nearestNeighbors(const float* value, size_t maxNeighborCount) const {
        return {this->id_, QueryOp::NearestNeighbors, value, maxNeighborCount};
    }
};

/// Carries property-based to-one relation information when used in the entity-meta ("underscore") class
template <typename SourceEntityT, typename TargetEntityT>  // NOLINT TargetEntityT may be used in the future
class RelationProperty : public Property<SourceEntityT, OBXPropertyType_Relation> {
public:
    explicit constexpr RelationProperty(obx_schema_id id) noexcept
        : Property<SourceEntityT, OBXPropertyType_Relation>(id) {}
};

/// Carries to-many relation information when used in the entity-meta ("underscore") class
template <typename SourceEntityT, typename TargetEntityT>
class RelationStandalone {
public:
    explicit constexpr RelationStandalone(obx_schema_id id) noexcept : id_(id) {}
    inline obx_schema_id id() const { return id_; }

protected:
    /// standalone relation ID
    const obx_schema_id id_;
};

class QueryBase;

template <typename EntityT>
class Query;

/// A QueryBuilderBase is used to create database queries using an API (no string based query language).
/// Building the queries involves calling functions to add conditions for the query.
/// In the end a Query object is build, which then can be used to actually run the query (potentially multiple times).
/// For generated code, you can also use the templated subclass QueryBuilder instead to have type safety at compile
/// time.
class QueryBuilderBase {
protected:
    Store& store_;
    OBX_query_builder* cQueryBuilder_;
    const obx_schema_id entityId_;
    const bool isRoot_;

public:
    QueryBuilderBase(Store& store, obx_schema_id entityId)
        : QueryBuilderBase(store, obx_query_builder(store.cPtr(), entityId), true) {}

    QueryBuilderBase(Store& store, const char* entityName)
        : QueryBuilderBase(store, store.getEntityTypeId(entityName)) {}

    /// Take ownership of an OBX_query_builder.
    ///
    /// *Example:**
    ///
    ///          QueryBuilder innerQb(obx_qb_link_property(outerQb.cPtr(), linkPropertyId), false)
    explicit QueryBuilderBase(Store& store, OBX_query_builder* ptr, bool isRoot)
        : store_(store), cQueryBuilder_(ptr), entityId_(obx_qb_type_id(cQueryBuilder_)), isRoot_(isRoot) {
        internal::checkPtrOrThrow(cQueryBuilder_, "Can not create query builder");
        internal::checkIdOrThrow(entityId_, "Can not create query builder");
    }

    /// Can't be copied, single owner of C resources is required (e.g. avoid double-free during destruction)
    QueryBuilderBase(const QueryBuilderBase&) = delete;

    QueryBuilderBase(QueryBuilderBase&& source) noexcept
        : store_(source.store_),
          cQueryBuilder_(source.cQueryBuilder_),
          entityId_(source.entityId_),
          isRoot_(source.isRoot_) {
        source.cQueryBuilder_ = nullptr;
    }

    virtual ~QueryBuilderBase() { obx_qb_close(cQueryBuilder_); }

    OBX_query_builder* cPtr() const { return cQueryBuilder_; }

    /// An object matches, if it has a given number of related objects pointing to it.
    /// At this point, there are a couple of limitations (later version may improve on that):
    /// 1) 1:N relations only, 2) the complexity is O(n * (relationCount + 1)) and cannot be improved via indexes,
    /// 3) The relation count cannot be set as an parameter.
    /// @param relationEntityId ID of the entity type the relation comes from.
    /// @param relationPropertyId ID of the property in the related entity type representing the relation.
    /// @param relationCount Number of related object an object must have to match. May be 0 if objects shall be matched
    ///        that do not have related objects. (Typically low numbers are used for the count.)
    QueryBuilderBase& relationCount(obx_schema_id relationEntityId, obx_schema_id relationPropertyId,
                                    uint32_t relationCount) {
        obx_qb_cond condition =
            obx_qb_relation_count_property(cQueryBuilder_, relationEntityId, relationPropertyId, relationCount);
        if (condition == 0) internal::throwLastError();
        return *this;
    }

    QueryBuilderBase& equals(obx_schema_id propertyId, int64_t value) {
        obx_qb_cond condition = obx_qb_equals_int(cQueryBuilder_, propertyId, value);
        if (condition == 0) internal::throwLastError();
        return *this;
    }

    QueryBuilderBase& notEquals(obx_schema_id propertyId, int64_t value) {
        obx_qb_cond condition = obx_qb_not_equals_int(cQueryBuilder_, propertyId, value);
        if (condition == 0) internal::throwLastError();
        return *this;
    }

    QueryBuilderBase& greaterThan(obx_schema_id propertyId, int64_t value) {
        obx_qb_cond condition = obx_qb_greater_than_int(cQueryBuilder_, propertyId, value);
        if (condition == 0) internal::throwLastError();
        return *this;
    }

    QueryBuilderBase& lessThan(obx_schema_id propertyId, int64_t value) {
        obx_qb_cond condition = obx_qb_less_than_int(cQueryBuilder_, propertyId, value);
        if (condition == 0) internal::throwLastError();
        return *this;
    }

    QueryBuilderBase& equalsString(obx_schema_id propertyId, const char* value, bool caseSensitive = true) {
        obx_qb_cond condition = obx_qb_equals_string(cQueryBuilder_, propertyId, value, caseSensitive);
        if (condition == 0) internal::throwLastError();
        return *this;
    }

    QueryBuilderBase& notEqualsString(obx_schema_id propertyId, const char* value, bool caseSensitive = true) {
        obx_qb_cond condition = obx_qb_not_equals_string(cQueryBuilder_, propertyId, value, caseSensitive);
        if (condition == 0) internal::throwLastError();
        return *this;
    }

    /// Adds an order based on a given property.
    /// @param property the property used for the order
    /// @param flags combination of OBXOrderFlags
    /// @return the reference to the same QueryBuilder for fluent interface.
    QueryBuilderBase& order(obx_schema_id propertyId, uint32_t flags = 0) {
        internal::checkErrOrThrow(obx_qb_order(cQueryBuilder_, propertyId, flags));
        return *this;
    }

    /// Appends given condition/combination of conditions.
    /// @return the reference to the same QueryBuilder for fluent interface.
    QueryBuilderBase& with(const QueryCondition& condition) {
        internalApplyCondition(condition, cQueryBuilder_, true);
        return *this;
    }

    /// Performs an approximate nearest neighbor (ANN) search to find objects near to the given query_vector.
    /// This requires the vector property to have a HNSW index.
    /// @param vectorPropertyId the vector property ID of the entity
    /// @param queryVector the query vector; its dimensions should be at least the dimensions of the vector property.
    /// @param maxResultCount maximum number of objects to return by the ANN condition.
    ///        Hint: it can also be used as the "ef" HNSW parameter to increase the search quality in combination with a
    ///        query limit.
    ///        For example, use 100 here with a query limit of 10 to have 10 results that are of potentially better
    ///        quality than just passing in 10 here (quality/performance tradeoff).
    QueryBuilderBase& nearestNeighborsFloat32(obx_schema_id vectorPropertyId, const float* queryVector,
                                              size_t maxResultCount) {
        obx_qb_cond condition =
            obx_qb_nearest_neighbors_f32(cQueryBuilder_, vectorPropertyId, queryVector, maxResultCount);
        if (condition == 0) internal::throwLastError();
        return *this;
    }

    /// Once all conditions have been applied, build the query using this method to actually run it.
    QueryBase buildBase();
};

/// See QueryBuilderBase for general information on query builders; this templated class allows to work with generated
/// entity types conveniently.
/// To specify actual conditions, use obx_qb_*() methods with queryBuilder.cPtr() as the first argument.
template <typename EntityT>
class QueryBuilder : public QueryBuilderBase {
    using EntityBinding = typename EntityT::_OBX_MetaInfo;

public:
    using QueryBuilderBase::QueryBuilderBase;

    explicit QueryBuilder(Store& store) : QueryBuilderBase(store, EntityBinding::entityId()) {}

    /// Adds an order based on a given property.
    /// @param property the property used for the order
    /// @param flags combination of OBXOrderFlags
    /// @return the reference to the same QueryBuilder for fluent interface.
    template <OBXPropertyType PropType>
    QueryBuilder& order(Property<EntityT, PropType> property, uint32_t flags = 0) {
        QueryBuilderBase::order(property.id(), flags);
        return *this;
    }

    /// Appends given condition/combination of conditions.
    /// @return the reference to the same QueryBuilder for fluent interface.
    QueryBuilder& with(const QueryCondition& condition) {  // Hiding function from base is OK
        QueryBuilderBase::with(condition);
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

    /// Performs an approximate nearest neighbor (ANN) search to find objects near to the given query vector.
    /// This requires the vector property to have a HNSW index.
    /// @param vectorProperty the vector property ID of the entity
    /// @param queryVector the query vector; its dimensions should be at least the dimensions of the vector property.
    /// @param maxResultCount maximum number of objects to return by the ANN condition.
    ///        Hint: it can also be used as the "ef" HNSW parameter to increase the search quality in combination with a
    ///        query limit.
    ///        For example, use 100 here with a query limit of 10 to have 10 results that are of potentially better
    ///        quality than just passing in 10 here (quality/performance tradeoff).
    QueryBuilder& nearestNeighborsFloat32(Property<EntityT, OBXPropertyType_FloatVector> vectorProperty,
                                          const float* queryVector, size_t maxResultCount) {
        QueryBuilderBase::nearestNeighborsFloat32(vectorProperty.id(), queryVector, maxResultCount);
        return *this;
    }

    /// Overload for a std::vector; for details see the pointer-based nearestNeighborsFloat32().
    QueryBuilder& nearestNeighborsFloat32(Property<EntityT, OBXPropertyType_FloatVector> vectorProperty,
                                          const std::vector<float>& queryVector, size_t maxResultCount) {
        QueryBuilderBase::nearestNeighborsFloat32(vectorProperty.id(), queryVector.data(), maxResultCount);
        return *this;
    }

    /// Once all conditions have been applied, build the query using this method to actually run it.
    Query<EntityT> build();

protected:
    template <typename LinkedEntityT>
    QueryBuilder<LinkedEntityT> linkedQB(OBX_query_builder* qb) {
        internal::checkPtrOrThrow(qb, "Can not build query link");
        // NOTE: the given qb may be lost if the user doesn't keep the returned sub-builder around and that's fine.
        // We're relying on the C-API keeping track of sub-builders on the root QB.
        return QueryBuilder<LinkedEntityT>(store_, qb, false);
    }
};

/// Query allows to find data matching user defined criteria for a entity type.
/// Created by QueryBuilder and typically used with supplying a Cursor.
class QueryBase {
protected:
    Store& store_;
    OBX_query* cQuery_;

public:
    /// Builds a query with the parameters specified by the builder
    explicit QueryBase(Store& store, OBX_query_builder* qb) : store_(store), cQuery_(obx_query(qb)) {
        internal::checkPtrOrThrow(cQuery_, "Can not build query");
    }

    /// Clones the query
    QueryBase(const QueryBase& query) : store_(query.store_), cQuery_(obx_query_clone(query.cQuery_)) {
        internal::checkPtrOrThrow(cQuery_, "Can not clone query");
    }

    QueryBase(QueryBase&& source) noexcept : store_(source.store_), cQuery_(source.cQuery_) {
        source.cQuery_ = nullptr;
    }

    virtual ~QueryBase() { obx_query_close(cQuery_); }

    OBX_query* cPtr() const { return cQuery_; }

    /// Sets an offset of what items to start at.
    /// This offset is stored for any further calls on the query until changed.
    /// Call with offset=0 to reset to the default behavior, i.e. starting from the first element.
    QueryBase& offset(size_t offset) {
        internal::checkErrOrThrow(obx_query_offset(cQuery_, offset));
        return *this;
    }

    /// Sets a limit on the number of processed items.
    /// This limit is stored for any further calls on the query until changed.
    /// Call with limit=0 to reset to the default behavior - zero limit means no limit applied.
    QueryBase& limit(size_t limit) {
        internal::checkErrOrThrow(obx_query_limit(cQuery_, limit));
        return *this;
    }

    /// Returns IDs of all matching objects.
    /// Note: if no order conditions is present, the order is arbitrary
    ///       (sometimes ordered by ID, but never guaranteed to).
    std::vector<obx_id> findIds() { return internal::idVectorOrThrow(obx_query_find_ids(cQuery_)); }

    /// Find object IDs matching the query associated to their query score (e.g. distance in NN search).
    /// The resulting vector is sorted by score in ascending order (unlike findIds()).
    std::vector<std::pair<obx_id, double>> findIdsWithScores() {
        OBX_VERIFY_STATE(cQuery_);

        OBX_id_score_array* cResult = obx_query_find_ids_with_scores(cQuery_);
        if (!cResult) internal::throwLastError();

        std::vector<std::pair<obx_id, double>> result(cResult->count);
        for (size_t i = 0; i < cResult->count; ++i) {
            std::pair<obx_id, double>& entry = result[i];
            const OBX_id_score& idScore = cResult->ids_scores[i];
            entry.first = idScore.id;
            entry.second = idScore.score;
        }

        obx_id_score_array_free(cResult);

        return result;
    }

    /// Find object IDs matching the query ordered by their query score (e.g. distance in NN search).
    /// The resulting array is sorted by score in ascending order (unlike findIds()).
    /// Unlike findIdsWithScores(), this method returns a simple vector of IDs without scores.
    std::vector<obx_id> findIdsByScore() { return internal::idVectorOrThrow(obx_query_find_ids_by_score(cQuery_)); }

    /// Walk over matching objects one-by-one using the given data visitor (C-style callback function with user data).
    /// Note: if no order conditions is present, the order is arbitrary (sometimes ordered by ID, but never guaranteed
    /// to).
    void visit(obx_data_visitor* visitor, void* userData) {
        OBX_VERIFY_STATE(cQuery_);
        obx_err err = obx_query_visit(cQuery_, visitor, userData);
        internal::checkErrOrThrow(err);
    }

    /// Walk over matching objects one-by-one using the given data visitor (C-style callback function with user data).
    /// Note: the elements are ordered by the score.
    void visitWithScore(obx_data_score_visitor* visitor, void* userData) {
        OBX_VERIFY_STATE(cQuery_);
        obx_err err = obx_query_visit_with_score(cQuery_, visitor, userData);
        internal::checkErrOrThrow(err);
    }

    /// Returns the number of matching objects.
    uint64_t count() {
        uint64_t result;
        internal::checkErrOrThrow(obx_query_count(cQuery_, &result));
        return result;
    }

    /// Removes all matching objects from the database & returns the number of deleted objects.
    size_t remove() {
        uint64_t result;
        internal::checkErrOrThrow(obx_query_remove(cQuery_, &result));
        return result;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    QueryBase& setParameter(obx_schema_id entityId, obx_schema_id propertyId, const char* value) {
        internal::checkErrOrThrow(obx_query_param_string(cQuery_, entityId, propertyId, value));
        return *this;
    }

    QueryBase& setParameter(obx_schema_id entityId, obx_schema_id propertyId, const std::string& value) {
        return setParameter(entityId, propertyId, value.c_str());
    }

    QueryBase& setParameter(obx_schema_id entityId, obx_schema_id propertyId, int64_t value) {
        internal::checkErrOrThrow(obx_query_param_int(cQuery_, entityId, propertyId, value));
        return *this;
    }
};

/// Query allows to find data matching user defined criteria for a entity type.
/// Created by QueryBuilder and typically used with supplying a Cursor.
template <typename EntityT>
class Query : public QueryBase {
public:
    using QueryBase::QueryBase;

    /// Sets an offset of what items to start at.
    /// This offset is stored for any further calls on the query until changed.
    /// Call with offset=0 to reset to the default behavior, i.e. starting from the first element.
    Query& offset(size_t offset) {  // Hiding function from base is OK
        QueryBase::offset(offset);
        return *this;
    }

    /// Sets a limit on the number of processed items.
    /// This limit is stored for any further calls on the query until changed.
    /// Call with limit=0 to reset to the default behavior - zero limit means no limit applied.
    Query& limit(size_t limit) {  // Hiding function from base is OK
        QueryBase::limit(limit);
        return *this;
    }

    /// Finds all objects matching the query.
    /// @return a vector of objects
    std::vector<EntityT> find() {
        OBX_VERIFY_STATE(cQuery_);

        CollectingVisitor<EntityT> visitor;
        obx_query_visit(cQuery_, CollectingVisitor<EntityT>::visit, &visitor);
        return std::move(visitor.items);
    }

    /// Finds all objects matching the query.
    /// @return a vector of unique_ptr of the resulting objects
    std::vector<std::unique_ptr<EntityT>> findUniquePtrs() {
        OBX_VERIFY_STATE(cQuery_);

        CollectingVisitorUniquePtr<EntityT> visitor;
        obx_query_visit(cQuery_, CollectingVisitorUniquePtr<EntityT>::visit, &visitor);
        return std::move(visitor.items);
    }

    /// Find objects matching the query associated to their query score (e.g. distance in NN search).
    /// The resulting vector is sorted by score in ascending order (unlike find()).
    std::vector<std::pair<EntityT, double>> findWithScores() {
        OBX_VERIFY_STATE(cQuery_);

        OBX_bytes_score_array* cResult = obx_query_find_with_scores(cQuery_);

        std::vector<std::pair<EntityT, double>> result(cResult->count);
        for (int i = 0; i < cResult->count; ++i) {
            std::pair<EntityT, double>& entry = result[i];
            const OBX_bytes_score& bytesScore = cResult->bytes_scores[i];
            EntityT::_OBX_MetaInfo::fromFlatBuffer(bytesScore.data, bytesScore.size, entry.first);
            entry.second = bytesScore.score;
        }

        obx_bytes_score_array_free(cResult);

        return result;
    }

    /// Find the first object matching the query or nullptr if none matches.
    std::unique_ptr<EntityT> findFirst() {
        return findSingle<std::unique_ptr<EntityT>>(obx_query_find_first, EntityT::_OBX_MetaInfo::newFromFlatBuffer);
    }

    /// Find the only object matching the query.
    /// @throws if there are multiple objects matching the query
    std::unique_ptr<EntityT> findUnique() {
        return findSingle<std::unique_ptr<EntityT>>(obx_query_find_unique, EntityT::_OBX_MetaInfo::newFromFlatBuffer);
    }

#ifdef __cpp_lib_optional
    /// Find the first object matching the query or nullptr if none matches.
    std::optional<EntityT> findFirstOptional() {
        return findSingle<std::optional<EntityT>>(obx_query_find_first, EntityT::_OBX_MetaInfo::fromFlatBuffer);
    }

    /// Find the only object matching the query.
    /// @throws if there are multiple objects matching the query
    std::optional<EntityT> findUniqueOptional() {
        return findSingle<std::optional<EntityT>>(obx_query_find_unique, EntityT::_OBX_MetaInfo::fromFlatBuffer);
    }
#endif

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <
        typename PropertyEntityT, OBXPropertyType PropertyType,
        typename = enable_if_t<PropertyType == OBXPropertyType_String || PropertyType == OBXPropertyType_StringVector>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, const char* value) {
        QueryBase::setParameter(entityId<PropertyEntityT>(), property.id(), value);
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
                        size_t count) {
        obx_err err = obx_query_param_strings(cQuery_, entityId<PropertyEntityT>(), property.id(), values, count);
        internal::checkErrOrThrow(err);
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
        internal::checkErrOrThrow(obx_query_param_int(cQuery_, entityId<PropertyEntityT>(), property.id(), value));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType, typename = EnableIfIntegerOrRel<PropertyType>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, int64_t value) {
        QueryBase::setParameter(entityId<PropertyEntityT>(), property.id(), value);
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType, typename = EnableIfIntegerOrRel<PropertyType>>
    Query& setParameters(Property<PropertyEntityT, PropertyType> property, int64_t valueA, int64_t valueB) {
        obx_err err = obx_query_param_2ints(cQuery_, entityId<PropertyEntityT>(), property.id(), valueA, valueB);
        internal::checkErrOrThrow(err);
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType,
              typename = enable_if_t<PropertyType == OBXPropertyType_Long || PropertyType == OBXPropertyType_Relation>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, const std::vector<int64_t>& values) {
        obx_err err =
            obx_query_param_int64s(cQuery_, entityId<PropertyEntityT>(), property.id(), values.data(), values.size());
        internal::checkErrOrThrow(err);
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType,
              typename = enable_if_t<PropertyType == OBXPropertyType_Int>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, const std::vector<int32_t>& values) {
        obx_err err =
            obx_query_param_int32s(cQuery_, entityId<PropertyEntityT>(), property.id(), values.data(), values.size());
        internal::checkErrOrThrow(err);
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType, typename = EnableIfFloating<PropertyType>>
    Query& setParameter(Property<PropertyEntityT, PropertyType> property, double value) {
        internal::checkErrOrThrow(obx_query_param_double(cQuery_, entityId<PropertyEntityT>(), property.id(), value));
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT, OBXPropertyType PropertyType, typename = EnableIfFloating<PropertyType>>
    Query& setParameters(Property<PropertyEntityT, PropertyType> property, double valueA, double valueB) {
        obx_err err = obx_query_param_2doubles(cQuery_, entityId<PropertyEntityT>(), property.id(), valueA, valueB);
        internal::checkErrOrThrow(err);
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_ByteVector> property, const void* value,
                        size_t size) {
        obx_err err = obx_query_param_bytes(cQuery_, entityId<PropertyEntityT>(), property.id(), value, size);
        internal::checkErrOrThrow(err);
        return *this;
    }

    /// Change previously set condition value in an existing query - this improves reusability of the query object.
    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_ByteVector> property,
                        const std::vector<uint8_t>& value) {
        return setParameter(property, value.data(), value.size());
    }

    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_FloatVector> property, const float* value,
                        size_t element_count) {
        obx_err err =
            obx_query_param_vector_float32(cQuery_, entityId<PropertyEntityT>(), property.id(), value, element_count);
        internal::checkErrOrThrow(err);
        return *this;
    }

    template <typename PropertyEntityT>
    Query& setParameter(Property<PropertyEntityT, OBXPropertyType_FloatVector> property,
                        const std::vector<float>& vector) {
        setParameter(property, vector.data(), vector.size());
        return *this;
    }

    /// Only for HNSW-enabled properties that are used for a nearest neighbor search:
    /// sets the maximum of neighbors to search for.
    template <typename PropertyEntityT>
    Query& setParameterMaxNeighbors(Property<PropertyEntityT, OBXPropertyType_FloatVector> property,
                                    int64_t maxNeighborCount) {
        QueryBase::setParameter(entityId<PropertyEntityT>(), property.id(), maxNeighborCount);
        return *this;
    }

private:
    template <typename RET, typename T>
    RET findSingle(obx_err nativeFn(OBX_query*, const void**, size_t*), T fromFlatBuffer(const void*, size_t)) {
        OBX_VERIFY_STATE(cQuery_);
        Transaction tx = store_.txRead();
        const void* data;
        size_t size;
        obx_err err = nativeFn(cQuery_, &data, &size);
        if (err == OBX_NOT_FOUND) return RET();
        internal::checkErrOrThrow(err);
        return fromFlatBuffer(data, size);
    }
};

#ifdef OBX_CPP_FILE
QueryBase QueryBuilderBase::buildBase() {
    OBX_VERIFY_STATE(isRoot_);
    return QueryBase(store_, cQueryBuilder_);
}
#endif

template <typename EntityT>
inline Query<EntityT> QueryBuilder<EntityT>::build() {
    OBX_VERIFY_STATE(isRoot_);
    return Query<EntityT>(store_, cQueryBuilder_);
}

#ifndef OBX_DISABLE_FLATBUFFERS
namespace internal {

#ifdef OBX_CPP_FILE
/// FlatBuffer builder is reused so the allocated memory stays available for the future objects.
flatbuffers::FlatBufferBuilder& threadLocalFbbDirty() {
    static thread_local flatbuffers::FlatBufferBuilder fbb;
    return fbb;
}
#else

// If you get a linker error like "undefined symbol" for this:
// ensure to add the line "#define OBX_CPP_FILE" before including this file in (exactly) one of your .cpp/.cc files.
flatbuffers::FlatBufferBuilder& threadLocalFbbDirty();  ///< Not cleared, thus potentially "dirty" fbb
#endif

void inline threadLocalFbbDone() {
    flatbuffers::FlatBufferBuilder& fbb = threadLocalFbbDirty();
    if (fbb.GetSize() > 512 * 1024) fbb.Reset();  // De-alloc large buffers after use
}

}  // namespace internal
#endif

/// Like Box, but without template type.
/// Serves as the basis for Box, but can also be used as "lower-level" box with some restrictions on the functionality.
class BoxTypeless {
protected:
    Store& store_;
    OBX_box* cBox_;
    const obx_schema_id entityTypeId_;

public:
    BoxTypeless(Store& store, obx_schema_id entityTypeId)
        : store_(store), cBox_(obx_box(store.cPtr(), entityTypeId)), entityTypeId_(entityTypeId) {
        if (cBox_ == nullptr) {
            std::string msg = "Can not create box for entity type ID " + std::to_string(entityTypeId_);
            internal::checkPtrOrThrow(cBox_, msg.c_str());
        }
    }

    OBX_box* cPtr() const { return cBox_; }

    /// Return the number of objects contained by this box.
    /// @param limit if provided: stop counting at the given limit - useful if you need to make sure the Box has "at
    /// least" this many objects but you don't need to know the exact number.
    uint64_t count(uint64_t limit = 0) {
        uint64_t result;
        internal::checkErrOrThrow(obx_box_count(cBox_, limit, &result));
        return result;
    }

    /// Returns true if the box contains no objects.
    bool isEmpty() {
        bool result;
        internal::checkErrOrThrow(obx_box_is_empty(cBox_, &result));
        return result;
    }

    /// Checks whether this box contains an object with the given ID.
    bool contains(obx_id id) {
        bool result;
        internal::checkErrOrThrow(obx_box_contains(cBox_, id, &result));
        return result;
    }

    /// Checks whether this box contains all objects matching the given IDs.
    bool contains(const std::vector<obx_id>& ids) {
        if (ids.empty()) return true;

        bool result;
        const OBX_id_array cIds = internal::cIdArrayRef(ids);
        internal::checkErrOrThrow(obx_box_contains_many(cBox_, &cIds, &result));
        return result;
    }

    /// Low-level API: read an object as FlatBuffers bytes from the database.
    /// @return true on success, false if the ID was not found, in which case outObject is untouched.
    bool get(CursorTx& cTx, obx_id id, const void** data, size_t* size) {
        obx_err err = obx_cursor_get(cTx.cPtr(), id, data, size);
        if (err == OBX_NOT_FOUND) return false;
        internal::checkErrOrThrow(err);
        return true;
    }

    /// Low-level API: puts the given FlatBuffers object.
    /// @returns the ID of the put object or 0 if the operation failed (no exception is thrown).
    obx_id putNoThrow(void* data, size_t size, OBXPutMode mode = OBXPutMode_PUT);

    obx_id put(void* data, size_t size, OBXPutMode mode = OBXPutMode_PUT);

    /// Remove the object with the given id
    /// @returns whether the object was removed or not (because it didn't exist)
    bool remove(obx_id id);

    /// Removes all objects matching the given IDs
    /// @returns number of removed objects between 0 and ids.size() (if all IDs existed)
    uint64_t remove(const std::vector<obx_id>& ids);

    /// Removes all objects from the box
    /// @returns the number of removed objects
    uint64_t removeAll();
};

#ifdef OBX_CPP_FILE

BoxTypeless Store::boxTypeless(const char* entityName) { return BoxTypeless(*this, getEntityTypeId(entityName)); }

obx_id BoxTypeless::putNoThrow(void* data, size_t size, OBXPutMode mode) {
    return obx_box_put_object4(cBox_, data, size, mode);
}

obx_id BoxTypeless::put(void* data, size_t size, OBXPutMode mode) {
    obx_id id = putNoThrow(data, size, mode);
    internal::checkIdOrThrow(id);
    return id;
}

bool BoxTypeless::remove(obx_id id) {
    obx_err err = obx_box_remove(cBox_, id);
    if (err == OBX_NOT_FOUND) return false;
    internal::checkErrOrThrow(err);
    return true;
}

uint64_t BoxTypeless::remove(const std::vector<obx_id>& ids) {
    uint64_t result = 0;
    const OBX_id_array cIds = internal::cIdArrayRef(ids);
    internal::checkErrOrThrow(obx_box_remove_many(cBox_, &cIds, &result));
    return result;
}

uint64_t BoxTypeless::removeAll() {
    uint64_t result = 0;
    internal::checkErrOrThrow(obx_box_remove_all(cBox_, &result));
    return result;
}

#endif

/// \brief A Box offers database operations for objects of a specific type.
///
/// Given a Store, you can create Box instances to interact with object data (e.g. get and put operations).
/// A Box instance is associated with a specific object type (data class) and gives you a high level API to interact
/// with data objects of that type.
///
/// Box operations automatically start an implicit transaction when accessing the database.
/// And because transactions offered by this API are always reentrant, you can set your own transaction boundary
/// using Store::txRead(), Store::txWrite() or Store::tx().
/// Using these explicit transactions is very much encouraged for calling multiple write operations that
/// logically belong together for better consistency(ACID) and performance.
///
/// Box instances are thread-safe and light-weight wrappers around C-boxes, which are cached internally (see obx_box()).
template <typename EntityT>
class Box : public BoxTypeless {
    friend AsyncBox<EntityT>;
    using EntityBinding = typename EntityT::_OBX_MetaInfo;

public:
    explicit Box(Store& store) : BoxTypeless(store, EntityBinding::entityId()) {}

    /// Async operations are available through the AsyncBox class.
    /// @returns a shared AsyncBox instance with the default timeout (1s) for enqueueing.
    /// Note: while this looks like it creates a new instance, it's only a thin wrapper and the actual ObjectBox core
    /// internal async box really is shared.
    AsyncBox<EntityT> async() { return AsyncBox<EntityT>(*this); }

    /// Start building a query this entity.
    QueryBuilder<EntityT> query() { return QueryBuilder<EntityT>(store_); }

    /// Start building a query this entity.
    QueryBuilder<EntityT> query(const QueryCondition& condition) {
        QueryBuilder<EntityT> qb(store_);
        internalApplyCondition(condition, qb.cPtr(), true);
        return qb;
    }

    /// Read an object from the database, returning a managed pointer.
    /// @return an object pointer or nullptr if an object with the given ID doesn't exist.
    std::unique_ptr<EntityT> get(obx_id id) {
        auto object = std::unique_ptr<EntityT>(new EntityT());
        if (!get(id, *object)) return nullptr;
        return object;
    }

    using BoxTypeless::get;

    /// Read an object from the database, replacing the contents of an existing object variable.
    /// @return true on success, false if the ID was not found, in which case outObject is untouched.
    bool get(obx_id id, EntityT& outObject) {
        CursorTx ctx(TxMode::READ, store_, EntityBinding::entityId());
        const void* data;
        size_t size;
        if (!get(ctx, id, &data, &size)) return false;
        EntityBinding::fromFlatBuffer(data, size, outObject);
        return true;
    }

#ifdef __cpp_lib_optional
    /// Read an object from the database.
    /// @return an "optional" wrapper of the object; empty if an object with the given ID doesn't exist.
    std::optional<EntityT> getOptional(obx_id id) {
        CursorTx ctx(TxMode::READ, store_, EntityBinding::entityId());
        const void* data;
        size_t size;
        if (!BoxTypeless::get(ctx, id, &data, &size)) return std::nullopt;
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
        const void* data;
        size_t size;

        obx_err err = obx_cursor_first(cursor.cPtr(), &data, &size);
        while (err == OBX_SUCCESS) {
            result.emplace_back(new EntityT());
            EntityBinding::fromFlatBuffer(data, size, *(result[result.size() - 1]));
            err = obx_cursor_next(cursor.cPtr(), &data, &size);
        }
        if (err != OBX_NOT_FOUND) internal::checkErrOrThrow(err);

        return result;
    }

#ifndef OBX_DISABLE_FLATBUFFERS

    /// Inserts or updates the given object in the database.
    /// @param object will be updated with a newly inserted ID if the one specified previously was zero. If an ID was
    /// already specified (non-zero), it will remain unchanged.
    /// @return object ID from the object param (see object param docs).
    obx_id put(EntityT& object, OBXPutMode mode = OBXPutMode_PUT) {
        // Using a const_cast to add "const" only to identify the method overload:
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        obx_id id = put(const_cast<const EntityT&>(object), mode);
        EntityBinding::setObjectId(object, id);
        return id;
    }

    /// Inserts or updates the given object in the database.
    /// @return newly assigned object ID in case this was an insert, otherwise the original ID from the object param.
    obx_id put(const EntityT& object, OBXPutMode mode = OBXPutMode_PUT) {
        flatbuffers::FlatBufferBuilder& fbb = internal::threadLocalFbbDirty();
        EntityBinding::toFlatBuffer(fbb, object);
        obx_id id = putNoThrow(fbb.GetBufferPointer(), fbb.GetSize(), mode);
        internal::threadLocalFbbDone();
        internal::checkIdOrThrow(id);
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

#endif  // OBX_DISABLE_FLATBUFFERS

    /// Fetch IDs of all objects in this box that reference the given object (ID) on the given relation property.
    /// Note: This method refers to "property based relations" unlike the "stand-alone relations" (Box::standaloneRel*).
    /// @param toOneRel the relation property, which must belong to the entity type represented by this box.
    /// @param objectId this relation points to - typically ID of an object of another entity type (another box).
    /// @returns resulting IDs representing objects in this Box, or NULL in case of an error
    ///
    /// **Example** Let's say you have the following two entities with a relation between them (.fbs file format):
    ///
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
        return internal::idVectorOrThrow(obx_box_get_backlink_ids(cBox_, toOneRel.id(), objectId));
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
        internal::checkErrOrThrow(obx_box_rel_put(cBox_, toManyRel.id(), sourceObjectId, targetObjectId));
    }

    /// Remove a standalone relation entry between two objects.
    /// @param toManyRel must be a standalone relation ID with source object entity belonging to this box
    /// @param sourceObjectId identifies an object from this box
    /// @param targetObjectId identifies an object from a target box (as per the relation definition)
    template <typename TargetEntityT>
    void standaloneRelRemove(RelationStandalone<EntityT, TargetEntityT> toManyRel, obx_id sourceObjectId,
                             obx_id targetObjectId) {
        internal::checkErrOrThrow(obx_box_rel_remove(cBox_, toManyRel.id(), sourceObjectId, targetObjectId));
    }

    /// Fetch IDs of all objects in this Box related to the given object (typically from another Box).
    /// Used for a stand-alone relation and its "regular" direction; this Box represents the target of the relation.
    /// @param relationId ID of a standalone relation, whose target type matches this Box
    /// @param objectId object ID of the relation source type (typically from another Box)
    /// @returns resulting IDs representing objects in this Box
    /// @todo improve docs by providing an example with a clear distinction between source and target type
    template <typename SourceEntityT>
    std::vector<obx_id> standaloneRelIds(RelationStandalone<SourceEntityT, EntityT> toManyRel, obx_id objectId) {
        return internal::idVectorOrThrow(obx_box_rel_get_ids(cBox_, toManyRel.id(), objectId));
    }

    /// Fetch IDs of all objects in this Box related to the given object (typically from another Box).
    /// Used for a stand-alone relation and its "backlink" direction; this Box represents the source of the relation.
    /// @param relationId ID of a standalone relation, whose source type matches this Box
    /// @param objectId object ID of the relation target type (typically from another Box)
    /// @returns resulting IDs representing objects in this Box
    template <typename TargetEntityT>
    std::vector<obx_id> standaloneRelBacklinkIds(RelationStandalone<EntityT, TargetEntityT> toManyRel,
                                                 obx_id objectId) {
        return internal::idVectorOrThrow(obx_box_rel_get_backlink_ids(cBox_, toManyRel.id(), objectId));
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
        internal::throwLastError();
    }

    /// Time series: get the limits (min/max time values) over objects within the given time range
    /// @returns true if objects were found in the given range (IDs/values are available)
    bool timeSeriesMinMax(int64_t rangeBegin, int64_t rangeEnd, obx_id* outMinId, int64_t* outMinValue,
                          obx_id* outMaxId, int64_t* outMaxValue) {
        obx_err err =
            obx_box_ts_min_max_range(cBox_, rangeBegin, rangeEnd, outMinId, outMinValue, outMaxId, outMaxValue);
        if (err == OBX_SUCCESS) return true;
        if (err == OBX_NOT_FOUND) return false;
        internal::throwLastError();
    }

private:
#ifndef OBX_DISABLE_FLATBUFFERS

    template <typename Vector>
    size_t putMany(Vector& objects, std::vector<obx_id>* outIds, OBXPutMode mode) {
        if (outIds) {
            outIds->clear();
            outIds->reserve(objects.size());
        }

        // Don't start a TX in case there's no data.
        // Note: Don't move this above clearing outIds vector - our contract says that we clear outIds before starting
        // execution, so we must do it even if no objects were passed.
        if (objects.empty()) return 0;

        size_t count = 0;
        CursorTx cursor(TxMode::WRITE, store_, EntityBinding::entityId());
        flatbuffers::FlatBufferBuilder& fbb = internal::threadLocalFbbDirty();

        for (auto& object : objects) {
            obx_id id = cursorPut(cursor, fbb, object, mode);  // type-based overloads below
            if (outIds) outIds->push_back(id);  // always include in outIds even if the item wasn't present (id == 0)
            if (id) count++;
        }
        internal::threadLocalFbbDone();  // NOTE might not get called in case of an exception
        cursor.commitAndClose();
        return count;
    }

    obx_id cursorPut(CursorTx& cursor, flatbuffers::FlatBufferBuilder& fbb, const EntityT& object, OBXPutMode mode) {
        EntityBinding::toFlatBuffer(fbb, object);
        obx_id id = obx_cursor_put_object4(cursor.cPtr(), fbb.GetBufferPointer(), fbb.GetSize(), mode);
        internal::checkIdOrThrow(id);
        return id;
    }

    obx_id cursorPut(CursorTx& cursor, flatbuffers::FlatBufferBuilder& fbb, EntityT& object, OBXPutMode mode) {
        // Using a const_cast to add "const" only to identify the method overload:
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        obx_id id = cursorPut(cursor, fbb, const_cast<const EntityT&>(object), mode);
        EntityBinding::setObjectId(object, id);
        return id;
    }

    obx_id cursorPut(CursorTx& cursor, flatbuffers::FlatBufferBuilder& fbb, const std::unique_ptr<EntityT>& object,
                     OBXPutMode mode) {
        return object ? cursorPut(cursor, fbb, *object, mode) : 0;
    }

#ifdef __cpp_lib_optional
    obx_id cursorPut(CursorTx& cursor, flatbuffers::FlatBufferBuilder& fbb, std::optional<EntityT>& object,
                     OBXPutMode mode) {
        return object.has_value() ? cursorPut(cursor, fbb, *object, mode) : 0;
    }
#endif

#endif  // OBX_DISABLE_FLATBUFFERS

    template <typename Item>
    std::vector<Item> getMany(const std::vector<obx_id>& ids) {
        std::vector<Item> result;
        result.resize(ids.size());  // prepare empty/nullptr pointers in the output

        CursorTx cursor(TxMode::READ, store_, EntityBinding::entityId());
        const void* data;
        size_t size;

        for (size_t i = 0; i < ids.size(); i++) {
            obx_err err = obx_cursor_get(cursor.cPtr(), ids[i], &data, &size);
            if (err == OBX_NOT_FOUND) continue;  // leave empty at result[i] in this case
            internal::checkErrOrThrow(err);
            readFromFb(result[i], data, size);
        }

        return result;
    }

    void readFromFb(std::unique_ptr<EntityT>& object, const void* data, size_t size) {
        object = EntityBinding::newFromFlatBuffer(data, size);
    }

#ifdef __cpp_lib_optional
    void readFromFb(std::optional<EntityT>& object, const void* data, size_t size) {
        object = EntityT();
        assert(object.has_value());
        EntityBinding::fromFlatBuffer(data, size, *object);
    }
#endif
};

/// AsyncBox provides asynchronous ("happening on the background") database manipulation.
template <typename EntityT>
class AsyncBox {
    using EntityBinding = typename EntityT::_OBX_MetaInfo;

    friend Box<EntityT>;

    const bool created_;  // whether this is a custom async box (true) or a shared instance (false)
    OBX_async* cAsync_;
    Store& store_;

    /// Creates a shared AsyncBox instance.
    explicit AsyncBox(Box<EntityT>& box) : AsyncBox(box.store_, false, obx_async(box.cPtr())) {}

    /// Creates a shared AsyncBox instance.
    AsyncBox(Store& store, bool owned, OBX_async* ptr) : store_(store), created_(owned), cAsync_(ptr) {
        internal::checkPtrOrThrow(cAsync_, "Can not create async box");
    }

public:
    /// Create a custom AsyncBox instance. Prefer using Box::async() for standard tasks, it gives you a shared instance.
    AsyncBox(Store& store, uint64_t enqueueTimeoutMillis)
        : AsyncBox(store, true, obx_async_create(store.box<EntityT>().cPtr(), enqueueTimeoutMillis)) {}

    /// Move constructor
    AsyncBox(AsyncBox&& source) noexcept : store_(source.store_), cAsync_(source.cAsync_), created_(source.created_) {
        source.cAsync_ = nullptr;
    }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    AsyncBox(const AsyncBox&) = delete;

    virtual ~AsyncBox() {
        if (created_ && cAsync_) obx_async_close(cAsync_);
    }

    OBX_async* cPtr() const {
        OBX_VERIFY_STATE(cAsync_);
        return cAsync_;
    }

#ifndef OBX_DISABLE_FLATBUFFERS

    /// Reserve an ID, which is returned immediately for future reference, and insert asynchronously.
    /// Note: of course, it can NOT be guaranteed that the entity will actually be inserted successfully in the DB.
    /// @param object will be updated with the reserved ID.
    /// @return the reserved ID which will be used for the object if the asynchronous insert succeeds.
    obx_id put(EntityT& object, OBXPutMode mode = OBXPutMode_PUT) {
        // Using a const_cast to add "const" only to identify the method overload:
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        obx_id id = put(const_cast<const EntityT&>(object), mode);
        EntityBinding::setObjectId(object, id);
        return id;
    }

    /// Reserve an ID, which is returned immediately for future reference and put asynchronously.
    /// Note: of course, it can NOT be guaranteed that the entity will actually be put successfully in the DB.
    /// @param mode - use INSERT or PUT; in case you need to use UPDATE, use the C-API directly for now
    /// @return the reserved ID which will be used for the object if the asynchronous insert succeeds.
    obx_id put(const EntityT& object, OBXPutMode mode = OBXPutMode_PUT) {
        flatbuffers::FlatBufferBuilder& fbb = internal::threadLocalFbbDirty();
        EntityBinding::toFlatBuffer(fbb, object);
        obx_id id = obx_async_put_object4(cPtr(), fbb.GetBufferPointer(), fbb.GetSize(), mode);
        internal::threadLocalFbbDone();
        internal::checkIdOrThrow(id);
        return id;
    }

#endif  // OBX_DISABLE_FLATBUFFERS

    /// Asynchronously remove the object with the given id.
    void remove(obx_id id) { internal::checkErrOrThrow(obx_async_remove(cPtr(), id)); }

    /// Await for all (including future) async submissions to be completed (the async queue becomes idle for a moment).
    /// Currently this is not limited to the single entity this AsyncBox is working on but all entities in the store.
    /// @returns true if all submissions were completed or async processing was not started; false if shutting down
    /// @returns false if shutting down or an error occurred
    bool awaitCompletion() { return obx_store_await_async_completion(store_.cPtr()); }

    /// Await for previously submitted async operations to be completed (the async queue does not have to become idle).
    /// Currently this is not limited to the single entity this AsyncBox is working on but all entities in the store.
    /// @returns true if all submissions were completed or async processing was not started
    /// @returns false if shutting down or an error occurred
    bool awaitSubmitted() { return obx_store_await_async_submitted(store_.cPtr()); }
};

using AsyncStatusCallback = std::function<void(obx_err err)>;

/// @brief Removal of expired objects; see OBXPropertyFlags_EXPIRATION_TIME.
class ExpiredObjects {
public:
    /// Removes expired objects of one or all entity types.
    /// @param typeId Type of the objects to be remove; if zero (the default), all types are included.
    ///        Hint: if you only have the entity type's name, use Store::getEntityTypeIdNoThrow() to get the ID.
    /// @returns the count of removed objects.
    /// @see OBXPropertyFlags_EXPIRATION_TIME to define a property for the expiration time.
    size_t remove(Transaction& tx, obx_schema_id typeId = 0);

    /// Asynchronously removes expired objects of one or all entity types.
    /// @param typeId Type of the objects to be remove; if zero (the default), all types are included.
    ///        Hint: if you only have the entity type's name, use Store::getEntityTypeIdNoThrow() to get the ID.
    /// @see OBXPropertyFlags_EXPIRATION_TIME to define a property for the expiration time.
    void removeAsync(Store& store, obx_schema_id typeId = 0, AsyncStatusCallback callback = {});
};

#ifdef OBX_CPP_FILE
size_t ExpiredObjects::remove(obx::Transaction& tx, obx_schema_id typeId) {
    size_t removedCount = 0;
    obx_err err = obx_expired_objects_remove(tx.cPtr(), typeId, &removedCount);
    internal::checkErrOrThrow(err);
    return removedCount;
}

namespace {
void cCallbackTrampolineStatus(obx_err status, void* userData) {
    assert(userData);
    std::unique_ptr<AsyncStatusCallback> callback(static_cast<AsyncStatusCallback*>(userData));
    (*callback)(status);
}
}  // namespace

void ExpiredObjects::removeAsync(Store& store, obx_schema_id typeId, AsyncStatusCallback callback) {
    bool hasCallback = callback != nullptr;
    auto funPtr = hasCallback ? new std::function<void(obx_err)>(std::move(callback)) : nullptr;
    obx_err err = obx_expired_objects_remove_async(store.cPtr(), typeId,
                                                   hasCallback ? cCallbackTrampolineStatus : nullptr, funPtr);
    internal::checkErrOrThrow(err);
}

#endif  // OBX_CPP_FILE

/// @brief Structural/behavioral options for a tree passed during tree creation.
class TreeOptions {
    friend class Tree;

    OBX_tree_options* cOptions_;
    char pathDelimiter_ = '/';

public:
    TreeOptions() : cOptions_(obx_tree_options()) {
        internal::checkPtrOrThrow(cOptions_, "Could not create tree options");
    }

    /// Move constructor "stealing" the C resource from the source
    TreeOptions(TreeOptions&& source) noexcept : cOptions_(source.cOptions_) { source.cOptions_ = nullptr; }

    ~TreeOptions() { obx_tree_options_free(cOptions_); }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    TreeOptions(const TreeOptions&) = delete;

    /// Adjusts the path delimiter character, which is by default "/".
    TreeOptions& pathDelimiter(char delimiter) {
        obx_err err = obx_tree_opt_path_delimiter(cOptions_, delimiter);
        internal::checkErrOrThrow(err);
        pathDelimiter_ = delimiter;
        return *this;
    }

    /// Sets the given OBXTreeOptionFlags. Combine multiple flags using bitwise OR.
    TreeOptions& flags(uint32_t flags) {
        obx_err err = obx_tree_opt_flags(cOptions_, flags);
        internal::checkErrOrThrow(err);
        return *this;
    }
};

/// Regular results in the sense of "not throwing an exception" for put operations via TreeCursor.
enum class TreePutResult {
    Undefined,     ///< Not an actual result, will never be returned from a synchronous put operation;
                   ///< when used in AsyncTreePutResult, however, this indicates an exceptional result.
    Success,       ///< OK
    PathNotFound,  ///< Given path did not exist (no meta leaf object given to create it)
    DidNotPut      ///< According to the given put mode, no put was performed, e.g. "insert but already existed"
};

namespace internal {
TreePutResult mapErrorToTreePutResult(obx_err err);
}  // namespace internal

/// Parameter to AsyncTreePutCallback, which is passed to Tree::putAsync().
struct AsyncTreePutResult {
    TreePutResult result;      ///< Non-exceptional results or "Undefined"; in the latter case obx_err has more details
    obx_err status;            ///< More detailed error code if operation did not succeed
    obx_id id;                 ///< ID of the leaf that was put if operation succeeded
    std::string errorMessage;  ///< Non-empty if an error occurred (result is "Undefined")

    ///@returns true if the operation was successful.
    inline bool isSuccess() { return status == OBX_SUCCESS; }

    /// Alternative to checking error codes: throw an exception instead.
    /// Note that this will always throw, so you should at least check for a successful outcome, e.g. via isSuccess().
    [[noreturn]] void throwException() { internal::throwError(status, "Async tree put failed: " + errorMessage); }
};

struct AsyncTreeGetResult {
    std::string path;  ///< Path of leaf
    obx_err status;    ///< OBX_SUCCESS or OBX_NOT_FOUND if operation did not succeed

    obx_id id;                ///< ID of the leaf that was get if operation succeeded or 0 if not found
    OBX_bytes leaf_data;      ///< Leaf data if operation succeeded
    OBX_bytes leaf_metadata;  ///< Leaf metadata if operation succeeded

    std::string errorMessage;  ///< Non-empty if an error occurred (result is "Undefined")

    ///@returns true if the operation was successful.
    inline bool isSuccess() { return status == OBX_SUCCESS; }

    /// Alternative to checking error codes: throw an exception instead.
    /// Note that this will always throw, so you should at least check for a successful outcome, e.g. via isSuccess().
    [[noreturn]] void throwException() { internal::throwError(status, "Async tree get failed: " + errorMessage); }
};

using AsyncTreePutCallback = std::function<void(const AsyncTreePutResult& result)>;
using AsyncTreeGetCallback = std::function<void(const AsyncTreeGetResult& result)>;
/// @brief Top level tree API representing a tree structure/schema associated with a store.
///
/// Data is accessed via TreeCursor.
class Tree {
    friend class TreeCursor;

    OBX_tree* cTree_;

    const char pathDelimiter_;

public:
    explicit Tree(Store& store) : cTree_(obx_tree(store.cPtr(), nullptr)), pathDelimiter_('/') {
        internal::checkPtrOrThrow(cTree_, "Tree could not be created");
    }

    Tree(Store& store, TreeOptions& options)
        : cTree_(obx_tree(store.cPtr(), options.cOptions_)), pathDelimiter_(options.pathDelimiter_) {
        options.cOptions_ = nullptr;  // Consumed by obx_tree()
        internal::checkPtrOrThrow(cTree_, "Tree could not be created");
    }

    Tree(Store& store, TreeOptions&& options)
        : cTree_(obx_tree(store.cPtr(), options.cOptions_)), pathDelimiter_(options.pathDelimiter_) {
        options.cOptions_ = nullptr;  // Consumed by obx_tree()
        internal::checkPtrOrThrow(cTree_, "Tree could not be created");
    }

    /// Move constructor "stealing" the C resource from the source
    Tree(Tree&& source) noexcept : cTree_(source.cTree_), pathDelimiter_(source.pathDelimiter_) {
        source.cTree_ = nullptr;
    }

    ~Tree() { obx_tree_close(cTree_); }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    Tree(const Tree&) = delete;

    /// Returns the leaf name of the given path (the string component after the last path delimiter).
    std::string getLeafName(const std::string& path) const;

    /// \brief A get operation for a tree leaf,
    /// @param withMetadata Flag if the callback also wants to receive the metadata (also as raw FlatBuffers).
    /// @param callback Once the operation has completed (successfully or not), the callback is called with
    ///        AsyncTreeGetResult.
    void getAsync(const char* path, bool withMetadata, AsyncTreeGetCallback callback);

    /// Like getAsync(), but the callback uses a C function ptr and user data instead of a std::function wrapper.
    /// @param withMetadata Flag if the callback also wants to receive the metadata (also as raw FlatBuffers).
    void getAsyncRawCallback(const char* path, bool withMetadata, obx_tree_async_get_callback* callback,
                             void* callback_user_data = nullptr);

    /// \brief A "low-level" put operation for a tree leaf using given raw FlatBuffer bytes.
    /// Any non-existing branches and meta nodes are put on the fly if an optional meta-leaf FlatBuffers is given.
    /// A typical usage pattern is to first try without the meta-leaf, and if it does not work, create the meta-leaf and
    /// call again with the meta leaf. This approach takes into account that meta-leaves typically exist, and thus no
    /// resources are wasted for meta-leaf creation if it's not required.
    /// An advantage of using "raw" operations is that custom properties can be passed in the FlatBuffers.
    /// @param data prepared FlatBuffers bytes for the data leaf (non-const as the data buffer will be mutated);
    ///        note: slots for IDs must be already be prepared (zero values)
    /// @param type value type of the given leafObject: it has to be passed to verify against stored metadata
    /// @param outId Receives the ID of the data leaf object that was put (pointer can be null)
    /// @param metadata optional FlatBuffers for the meta leaf; at minimum, "name" and "type" must be set.
    ///        Note: slots for IDs must be already be prepared (zero values).
    ///        Passing null indicates that the branches and meta nodes must already exist; otherwise
    ///        the operation will fail and OBX_NOT_FOUND will be returned.
    /// @param dataPutMode For the data leaf only (the actual value tree node)
    /// @param callback Once the operation has completed (successfully or not), the callback is called with
    ///        AsyncTreePutResult.
    void putAsync(const char* path, void* data, size_t size, OBXPropertyType type, void* metadata = nullptr,
                  size_t metadata_size = 0, OBXPutMode dataPutMode = OBXPutMode_PUT,
                  AsyncTreePutCallback callback = {});

    /// Like putAsync(), but the callback uses a C function ptr and user data instead of a std::function wrapper.
    void putAsyncRawCallback(const char* path, void* data, size_t size, OBXPropertyType type, void* metadata = nullptr,
                             size_t metadata_size = 0, OBXPutMode dataPutMode = OBXPutMode_PUT,
                             obx_tree_async_put_callback* callback = nullptr, void* callback_user_data = nullptr);

    /// Explicitly consolidates tree node conflicts (non unique nodes sharing a common path) asynchronously.
    void consolidateNodeConflictsAsync();

    /// Gets the number of currently tracked node conflicts (non-unique nodes at the same path).
    /// This count gets resent when conflicts get consolidated.
    /// Only tracked if OBXTreeOptionFlags_DetectNonUniqueNodes (or OBXTreeOptionFlags_AutoConsolidateNonUniqueNodes) is
    /// set.
    size_t nodeConflictCount() { return obx_tree_node_conflict_count(cTree_); }
};

#ifdef OBX_CPP_FILE

std::string Tree::getLeafName(const std::string& path) const {
    std::string leafName;
    auto lastDelimiter = path.find_last_of(pathDelimiter_);
    if (lastDelimiter == std::string ::npos) {
        leafName = path;
    } else {
        leafName = path.substr(lastDelimiter + 1);
    }
    return leafName;
}

namespace internal {
TreePutResult mapErrorToTreePutResult(obx_err err) {
    if (err == OBX_SUCCESS) {
        return TreePutResult::Success;
    } else if (err == OBX_NOT_FOUND) {
        return TreePutResult::PathNotFound;
    } else if (err == OBX_NO_SUCCESS) {
        return TreePutResult::DidNotPut;
    } else {
        return TreePutResult::Undefined;
    }
}
}  // namespace internal

void Tree::putAsyncRawCallback(const char* path, void* data, size_t size, OBXPropertyType type, void* metadata,
                               size_t metadata_size, OBXPutMode dataPutMode, obx_tree_async_put_callback* callback,
                               void* callback_user_data) {
    obx_err err = obx_tree_async_put_raw(cTree_, path, data, size, type, metadata, metadata_size, dataPutMode, callback,
                                         callback_user_data);
    internal::checkErrOrThrow(err);
}

void Tree::getAsyncRawCallback(const char* path, bool withMetadata, obx_tree_async_get_callback* callback,
                               void* callback_user_data) {
    obx_err err = obx_tree_async_get_raw(cTree_, path, withMetadata, callback, callback_user_data);
    internal::checkErrOrThrow(err);
}

namespace {
void cCallbackTrampolineAsyncTreePut(obx_err status, obx_id id, void* userData) {
    assert(userData);
    std::unique_ptr<AsyncTreePutCallback> callback(static_cast<AsyncTreePutCallback*>(userData));
    std::string errorMessage;
    if (status != OBX_SUCCESS) internal::appendLastErrorText(status, errorMessage);
    AsyncTreePutResult result{internal::mapErrorToTreePutResult(status), status, id, std::move(errorMessage)};
    (*callback)(result);
}
void cCallbackTrampolineAsyncTreeGet(obx_err status, obx_id id, const char* path, const void* leaf_data,
                                     size_t leaf_data_size, const void* leaf_metadata, size_t leaf_metadata_size,
                                     void* userData) {
    assert(userData);
    std::unique_ptr<AsyncTreeGetCallback> callback(static_cast<AsyncTreeGetCallback*>(userData));
    std::string errorMessage;
    if (status != OBX_SUCCESS) internal::appendLastErrorText(status, errorMessage);
    AsyncTreeGetResult result{
        path, status, id, {leaf_data, leaf_data_size}, {leaf_metadata, leaf_metadata_size}, std::move(errorMessage)};
    (*callback)(result);
}
}  // namespace

void Tree::putAsync(const char* path, void* data, size_t size, OBXPropertyType type, void* metadata,
                    size_t metadata_size, OBXPutMode dataPutMode,
                    std::function<void(const AsyncTreePutResult& result)> callback) {
    bool hasCallback = callback != nullptr;
    auto funPtr = hasCallback ? new std::function<void(const AsyncTreePutResult&)>(std::move(callback)) : nullptr;
    obx_tree_async_put_callback* cCallback = hasCallback ? cCallbackTrampolineAsyncTreePut : nullptr;
    obx_err err =
        obx_tree_async_put_raw(cTree_, path, data, size, type, metadata, metadata_size, dataPutMode, cCallback, funPtr);
    internal::checkErrOrThrow(err);
}

void Tree::consolidateNodeConflictsAsync() {
    obx_err err = obx_tree_async_consolidate_node_conflicts(cTree_);
    internal::checkErrOrThrow(err);
}

void Tree::getAsync(const char* path, bool withMetadata,
                    std::function<void(const AsyncTreeGetResult& result)> callback) {
    auto funPtr = new std::function<void(const AsyncTreeGetResult&)>(std::move(callback));
    obx_tree_async_get_callback* cCallback = cCallbackTrampolineAsyncTreeGet;
    obx_err err = obx_tree_async_get_raw(cTree_, path, withMetadata, cCallback, funPtr);
    internal::checkErrOrThrow(err);
}

#endif

class TreeCursor;

/// Information about a set of leaf nodes that is returned by TreeCursor::getLeavesInfo().
/// Contains meta data about (data) leaves: full path in the tree, ID, and property type.
class LeavesInfo {
    friend class TreeCursor;

    OBX_tree_leaves_info* cLeavesInfo_;

    LeavesInfo(OBX_tree_leaves_info* cLeavesInfo) : cLeavesInfo_(cLeavesInfo) {}

public:
    ~LeavesInfo() { obx_tree_leaves_info_free(cLeavesInfo_); }

    /// Gets the number of leaves.
    size_t size() { return obx_tree_leaves_info_size(cLeavesInfo_); }

    /// Gets the path of a given leaf (by index).
    /// @returns a C string that is valid until
    const char* leafPathCString(size_t index) { return obx_tree_leaves_info_path(cLeavesInfo_, index); }

    /// Gets the path of a given leaf (by index).
    std::string leafPath(size_t index) { return leafPathCString(index); }

    /// Gets the property type (as OBXPropertyType) of a given leaf (by index).
    /// @returns OBXPropertyType_Unknown if no property type was found.
    OBXPropertyType leafPropertyType(size_t index) { return obx_tree_leaves_info_type(cLeavesInfo_, index); }

    /// Gets the id of a given leaf (by index).
    obx_id leafId(size_t index) { return obx_tree_leaves_info_id(cLeavesInfo_, index); }
};

/// \brief Primary tree interface against the database.
/// Offers tree path based get/put functionality.
/// Not-thread safe: use a TreeCursor instance from one thread only; i.e. the underlying transaction is bound to a
/// thread.
class TreeCursor {
    friend class LeavesInfo;
    OBX_tree_cursor* cCursor_;

public:
    TreeCursor(Tree& tree, Transaction* tx) : cCursor_(obx_tree_cursor(tree.cTree_, tx ? tx->cPtr() : nullptr)) {
        internal::checkPtrOrThrow(cCursor_, "Could not create tree cursor");
    }

    TreeCursor(Tree& tree, Transaction& tx) : TreeCursor(tree, &tx) {}

    /// Move constructor "stealing" the C resource from the source
    TreeCursor(TreeCursor&& source) noexcept : cCursor_(source.cCursor_) { source.cCursor_ = nullptr; }

    ~TreeCursor() { obx_tree_cursor_close(cCursor_); }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    TreeCursor(const TreeCursor&) = delete;

    /// \brief Sets or clears a transaction from the tree cursor.
    ///
    /// A typical use case for this function is to cache the tree cursor for reusing it later.
    /// Note: before closing a transaction, ensure to clear it here first (set to null).
    void setTransaction(Transaction* tx) {
        OBX_txn* cTx = tx ? tx->cPtr() : nullptr;
        obx_err err = obx_tree_cursor_txn(cCursor_, cTx);
        internal::checkErrOrThrow(err);
    }

    void setTransaction(Transaction& tx) { setTransaction(&tx); };

    /// \brief A "low-level" get operation to access a tree leaf using the raw FlatBuffer bytes stored in the database.
    /// As usual, the data is only valid during the lifetime of the transaction and before the first write to the DB.
    /// An advantage of using "raw" operations is that custom properties can be passed in the FlatBuffer.
    /// @param data receiver of the data pointer (non-null pointer to a pointer), which will be pointing to FlatBuffers
    ///        bytes for the data leaf after this call.
    /// @param metadata optional FlatBuffers receiver (nullable pointer to a pointer) for the meta leaf.
    /// @returns true if a node was found at the given path
    bool get(const char* path, const void** data, size_t* size, const void** metadata = nullptr,
             size_t* metadataSize = nullptr) {
        obx_err err = obx_tree_cursor_get_raw(cCursor_, path, data, size, metadata, metadataSize);
        if (err == OBX_NOT_FOUND) return false;
        internal::checkErrOrThrow(err);
        return true;
    }

    /// Gets the full path (from the root) of the given leaf ID (allocated C string version).
    /// @returns If successful, an allocated path is returned (malloc), which must be free()-ed by the caller.
    /// @returns If not successful, NULL is returned.
    const char* getLeafPathCString(obx_id leafId) { return obx_tree_cursor_get_leaf_path(cCursor_, leafId); }

    /// Gets the full path (from the root) of the given leaf ID (allocated C string version).
    /// @returns The path or an empty string if not successful.
    std::string getLeafPath(obx_id leafId) {
        const char* path = obx_tree_cursor_get_leaf_path(cCursor_, leafId);
        if (path) {
            std::string pathStr(path);
            free(const_cast<char*>(path));
            return pathStr;
        }
        return std::string();
    }

    /// \brief Given an existing path, return all existing leaves with their paths.
    /// As this traverses the data tree (i.e. not the meta tree), it will only return nodes that exist (obviously).
    /// Thus, the meta tree may contain additional paths, but these are unused by the data tree (currently at least).
    /// @param path the branch or leaf path to use. Optional: if no path is given, the root node is taken.
    /// @returns leaf info ordered by path depth (i.e. starting from paths with the smallest number of branches); paths
    ///          at same depth are ordered by id.
    LeavesInfo getLeavesInfo(const char* path = nullptr) {
        OBX_tree_leaves_info* cLeavesInfo = obx_tree_cursor_get_child_leaves_info(cCursor_, path);
        internal::checkPtrOrThrow(cLeavesInfo, "Could not create leaves info");
        return LeavesInfo(cLeavesInfo);
    }

    inline LeavesInfo getLeavesInfo(const std::string& path) { return getLeavesInfo(path.c_str()); }

    /// \brief A "low-level" put operation for a tree leaf using given raw FlatBuffer bytes.
    /// Any non-existing branches and meta nodes are put on the fly if an optional meta-leaf FlatBuffers is given.
    /// A typical usage pattern is to first try without the meta-leaf, and if it does not work, create the meta-leaf and
    /// call again with the meta leaf. This approach takes into account that meta-leaves typically exist, and thus no
    /// resources are wasted for meta-leaf creation if it's not required.
    /// An advantage of using "raw" operations is that custom properties can be passed in the FlatBuffers.
    /// @param data prepared FlatBuffers bytes for the data leaf (non-const as the data buffer will be mutated);
    ///        note: slots for IDs must be already be prepared (zero values)
    /// @param type value type of the given leafObject: it has to be passed to verify against stored metadata
    /// @param outId Receives the ID of the data leaf object that was put (pointer can be null)
    /// @param metadata optional FlatBuffers for the meta leaf; at minimum, "name" and "type" must be set.
    ///        Note: slots for IDs must be already be prepared (zero values).
    ///        Passing null indicates that the branches and meta nodes must already exist; otherwise
    ///        the operation will fail and OBX_NOT_FOUND will be returned.
    /// @param dataPutMode For the data leaf only (the actual value tree node)
    /// @returns result status enum
    TreePutResult put(const char* path, void* data, size_t size, OBXPropertyType type, obx_id* outId = nullptr,
                      void* metadata = nullptr, size_t metadata_size = 0, OBXPutMode dataPutMode = OBXPutMode_PUT) {
        obx_err err =
            obx_tree_cursor_put_raw(cCursor_, path, data, size, type, outId, metadata, metadata_size, dataPutMode);
        TreePutResult result = internal::mapErrorToTreePutResult(err);
        if (result == TreePutResult::Undefined) internal::throwLastError(err);
        return result;
    }

    /// Explicitly consolidates tree node conflicts (non unique nodes sharing a common path).
    /// See also Tree::consolidateNodeConflictsAsync() for an asynchronous version.
    size_t consolidateNodeConflicts() {
        size_t count = 0;
        obx_err err = obx_tree_cursor_consolidate_node_conflicts(cCursor_, &count);
        internal::checkErrOrThrow(err);
        return count;
    }
};

/**@}*/  // end of doxygen group "cpp ObjectBox C++ API"
}  // namespace obx

#ifdef __clang__
#pragma clang diagnostic pop
#endif