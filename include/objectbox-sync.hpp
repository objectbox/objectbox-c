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

#pragma once

#include "objectbox-sync.h"
#include "objectbox.hpp"

static_assert(OBX_VERSION_MAJOR == 4 && OBX_VERSION_MINOR == 0 && OBX_VERSION_PATCH == 0,  // NOLINT
              "Versions of objectbox.h and objectbox-sync.hpp files do not match, please update");

namespace obx {

class SyncCredentials {
    friend SyncClient;
    friend SyncServer;
    OBXSyncCredentialsType type_;
    std::vector<uint8_t> data_;
    std::string username_;
    std::string password_;

public:
    SyncCredentials(OBXSyncCredentialsType type, std::vector<uint8_t>&& data) : type_(type), data_(std::move(data)) {}

    SyncCredentials(OBXSyncCredentialsType type, const std::string& data) : type_(type), data_(data.size()) {
        static_assert(sizeof(data_[0]) == sizeof(data[0]), "Can't directly copy std::string to std::vector<uint8_t>");
        if (!data.empty()) memcpy(data_.data(), data.data(), data.size() * sizeof(data[0]));
    }

    SyncCredentials(OBXSyncCredentialsType type, const std::string& username, const std::string& password)
        : type_(type), username_(username), password_(password) {}

    static SyncCredentials none() {
        return SyncCredentials(OBXSyncCredentialsType::OBXSyncCredentialsType_NONE, std::vector<uint8_t>{});
    }

    static SyncCredentials sharedSecret(std::vector<uint8_t>&& data) {
        return SyncCredentials(OBXSyncCredentialsType::OBXSyncCredentialsType_SHARED_SECRET, std::move(data));
    }

    static SyncCredentials sharedSecret(const std::string& str) {
        return SyncCredentials(OBXSyncCredentialsType::OBXSyncCredentialsType_SHARED_SECRET, str);
    }

    static SyncCredentials googleAuth(const std::string& str) {
        return SyncCredentials(OBXSyncCredentialsType::OBXSyncCredentialsType_GOOGLE_AUTH, str);
    }

    static SyncCredentials obxAdminUser(const std::string& username, const std::string& password) {
        return SyncCredentials(OBXSyncCredentialsType::OBXSyncCredentialsType_OBX_ADMIN_USER, username, password);
    }

    static SyncCredentials userPassword(const std::string& username, const std::string& password) {
        return SyncCredentials(OBXSyncCredentialsType::OBXSyncCredentialsType_USER_PASSWORD, username, password);
    }
};

/// Listens to login events on a sync client.
class SyncClientLoginListener {
public:
    virtual ~SyncClientLoginListener() = default;

    /// Called on a successful login.
    /// At this point the connection to the sync destination was established and
    /// entered an operational state, in which data can be sent both ways.
    virtual void loggedIn() noexcept = 0;

    /// Called on a login failure with a `result` code specifying the issue.
    virtual void loginFailed(OBXSyncCode) noexcept = 0;
};

/// Listens to sync client connection events.
class SyncClientConnectionListener {
public:
    virtual ~SyncClientConnectionListener() = default;

    /// Called when connection is established (on first connect or after a reconnection).
    virtual void connected() noexcept = 0;

    /// Called when the client is disconnected from the sync server, e.g. due to a network error.
    /// Depending on the configuration, the sync client typically tries to reconnect automatically, triggering
    /// `connected()` when successful.
    virtual void disconnected() noexcept = 0;
};

/// Listens to sync complete event on a sync client.
class SyncClientCompletionListener {
public:
    virtual ~SyncClientCompletionListener() = default;

    /// Called each time a sync completes, in the sense that the client has caught up with the current server state.
    /// Or in other words, when the client is "up-to-date".
    virtual void updatesCompleted() noexcept = 0;
};

/// Listens to sync error event on a sync client.
class SyncClientErrorListener {
public:
    virtual ~SyncClientErrorListener() = default;

    /// Called when the client detects a sync error.
    /// @param error - indicates the error event that occurred.
    virtual void errorOccurred(OBXSyncError error) noexcept = 0;
};

/// List

/// Listens to sync time information events on a sync client.
class SyncClientTimeListener {
public:
    using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

    virtual ~SyncClientTimeListener() = default;

    /// Called when a server time information is received on the client.
    /// @param time - current server timestamp since Unix epoch
    virtual void serverTime(TimePoint time) noexcept = 0;
};

/// A collection of changes made to one entity type during a sync transaction. Delivered via SyncClientChangeListener.
/// IDs of changed objects are available via `puts` and those of removed objects via `removals`.
struct SyncChange {
    obx_schema_id entityId = 0;
    std::vector<obx_id> puts;
    std::vector<obx_id> removals;
};

/// Notifies of fine granular changes on the object level happening during sync.
/// @note this may affect performance. Use SyncClientCompletionListener for the general synchronization-finished check.
class SyncChangeListener {
public:
    virtual ~SyncChangeListener() = default;

    /// Called each time when data `changes` from sync were applied locally.
    virtual void changed(const std::vector<SyncChange>& changes) noexcept = 0;

private:
    friend SyncClient;
    friend SyncServer;

    void changed(const OBX_sync_change_array* cChanges) noexcept {
        std::vector<SyncChange> changes(cChanges->count);

        for (size_t i = 0; i < cChanges->count; i++) {
            const OBX_sync_change& cChange = cChanges->list[i];
            SyncChange& change = changes[i];
            change.entityId = cChange.entity_id;
            copyIdVector(cChange.puts, change.puts);
            copyIdVector(cChange.removals, change.removals);
        }

        changed(changes);
    }

    void copyIdVector(const OBX_id_array* in, std::vector<obx_id>& out) {
        if (in) {
            out.resize(in->count);
            memcpy(out.data(), in->ids, sizeof(out[0]) * out.size());
        } else {
            out.clear();
        }
    }
};

/// Listens to all possible sync events. See each base abstract class for detailed information.
class SyncClientListener : public SyncClientLoginListener,
                           public SyncClientCompletionListener,
                           public SyncClientConnectionListener,
                           public SyncChangeListener,
                           public SyncClientTimeListener,
                           public SyncClientErrorListener {};

class SyncObjectsMessageListener {
public:
    virtual ~SyncObjectsMessageListener() = default;

    // TODO do we want to transform to a more c++ friendly representation, like in other listeners?
    virtual void received(const OBX_sync_msg_objects* cObjects) noexcept = 0;
};

/// Start here to prepare an 'objects message'. You must add at least one object and then you can send the message
/// using SyncClient::send() or SyncServer::send().
class SyncObjectsMessageBuilder {
    friend SyncClient;
    friend SyncServer;

    OBX_sync_msg_objects_builder* cBuilder_;

    OBX_sync_msg_objects_builder* release() {
        OBX_VERIFY_STATE(cBuilder_);
        OBX_sync_msg_objects_builder* result = cBuilder_;
        cBuilder_ = nullptr;
        return result;
    }

public:
    SyncObjectsMessageBuilder() : SyncObjectsMessageBuilder(nullptr, 0) {}

    /// @param topic - application-specific message qualifier
    explicit SyncObjectsMessageBuilder(const std::string& topic)
        : SyncObjectsMessageBuilder(topic.c_str(), topic.size()) {}

    /// @param topic - application-specific message qualifier (may be NULL), usually a string but can also be binary
    explicit SyncObjectsMessageBuilder(const void* topic, size_t topicSize)
        : cBuilder_(obx_sync_msg_objects_builder(topic, topicSize)) {}

    SyncObjectsMessageBuilder(SyncObjectsMessageBuilder&& source) noexcept : cBuilder_(source.cBuilder_) {
        source.cBuilder_ = nullptr;
    }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    SyncObjectsMessageBuilder(const SyncObjectsMessageBuilder&) = delete;

    virtual ~SyncObjectsMessageBuilder() {
        if (cBuilder_) obx_sync_msg_objects_builder_discard(cBuilder_);
    }

    /// Adds an object to the given message (builder).
    /// @param id an optional (pass 0 if you don't need it) value that the application can use identify the object
    void add(OBXSyncObjectType type, const void* data, size_t size, uint64_t id = 0) {
        internal::checkErrOrThrow(obx_sync_msg_objects_builder_add(cBuilder_, type, data, size, id));
    }

    /// Adds a string object to the given message (builder).
    /// @param id an optional (pass 0 if you don't need it) value that the application can use identify the object
    void add(const std::string& object, uint64_t id = 0) {
        add(OBXSyncObjectType_String, object.c_str(), object.size(), id);
    }
};

/// Sync client is used to provide ObjectBox Sync client capabilities to your application.
class SyncClient : public Closable {
    Store& store_;
    std::atomic<OBX_sync*> cSync_{nullptr};

    /// Groups all listeners and the mutex that protects access to them. We could have a separate mutex for each
    /// listener but that's probably an overkill.
    struct {
        std::mutex mutex;
        std::shared_ptr<SyncClientLoginListener> login;
        std::shared_ptr<SyncClientCompletionListener> complete;
        std::shared_ptr<SyncClientConnectionListener> connect;
        std::shared_ptr<SyncChangeListener> change;
        std::shared_ptr<SyncClientTimeListener> time;
        std::shared_ptr<SyncClientListener> combined;
        std::shared_ptr<SyncClientErrorListener> error;
        std::shared_ptr<SyncObjectsMessageListener> object;
    } listeners_;

    using Guard = std::lock_guard<std::mutex>;

public:
    /// Creates a sync client associated with the given store and options.
    /// This does not initiate any connection attempts yet: call start() to do so.
    explicit SyncClient(Store& store, const std::vector<std::string>& serverUrls, const SyncCredentials& creds)
        : store_(store) {
        std::vector<const char*> urlPointers;  // Convert serverUrls to a vector of C strings for the C API
        urlPointers.reserve(serverUrls.size());
        for (const std::string& serverUrl : serverUrls) {
            urlPointers.emplace_back(serverUrl.c_str());
        }
        cSync_ = obx_sync_urls(store.cPtr(), urlPointers.data(), urlPointers.size());

        internal::checkPtrOrThrow(cSync_, "Could not initialize sync client");
        try {
            setCredentials(creds);
        } catch (...) {
            closeNonVirtual();  // free native resources before throwing
            throw;
        }
    }

    explicit SyncClient(Store& store, const std::string& serverUrl, const SyncCredentials& creds)
        : SyncClient(store, std::vector<std::string>{serverUrl}, creds) {}

    /// Creates a sync client associated with the given store and options.
    /// This does not initiate any connection attempts yet: call start() to do so.
    /// @param cSync an initialized sync client. You must NOT call obx_sync_close() yourself anymore.
    explicit SyncClient(Store& store, OBX_sync* cSync) : store_(store), cSync_(cSync) {
        OBX_VERIFY_STATE(obx_has_feature(OBXFeature_Sync));
        OBX_VERIFY_ARGUMENT(cSync);
    }

    /// Can't be moved due to the atomic cSync_ - use shared_ptr instead of SyncClient instances directly.
    SyncClient(SyncClient&& source) = delete;

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    SyncClient(const SyncClient&) = delete;

    ~SyncClient() override {
        try {
            closeNonVirtual();
        } catch (...) {
        }
    }

    /// Closes and cleans up all resources used by this sync client.
    /// It can no longer be used afterwards, make a new sync client instead.
    /// Does nothing if this sync client has already been closed.
    void close() override { closeNonVirtual(); }

    /// Returns if this sync client is closed and can no longer be used.
    bool isClosed() override { return cSync_ == nullptr; }

    /// Gets the current sync client state.
    OBXSyncState state() const { return obx_sync_state(cPtr()); }

    /// Gets the protocol version this client uses.
    static uint32_t protocolVersion() { return obx_sync_protocol_version(); }

    /// Returns the protocol version of the server after a connection was established (or attempted), zero otherwise.
    uint32_t serverProtocolVersion() const { return obx_sync_protocol_version_server(cPtr()); }

    /// Configure authentication credentials.
    /// The accepted OBXSyncCredentials type depends on your sync-server configuration.
    void setCredentials(const SyncCredentials& creds) {
        obx_err err;
        switch (creds.type_) {
            case OBXSyncCredentialsType_OBX_ADMIN_USER:
            case OBXSyncCredentialsType_USER_PASSWORD:
                err = obx_sync_credentials_user_password(cPtr(), creds.type_, creds.username_.c_str(),
                                                         creds.password_.c_str());
                break;
            default:
                err = obx_sync_credentials(cPtr(), creds.type_, creds.data_.empty() ? nullptr : creds.data_.data(),
                                           creds.data_.size());
        }
        internal::checkErrOrThrow(err);
    }

    /// Triggers a reconnection attempt immediately.
    /// By default, an increasing backoff interval is used for reconnection attempts.
    /// But sometimes the user of this API has additional knowledge and can initiate a reconnection attempt sooner.
    bool triggerReconnect() { return internal::checkSuccessOrThrow(obx_sync_trigger_reconnect(cPtr())); }

    /// Sets the interval in which the client sends "heartbeat" messages to the server, keeping the connection alive.
    /// To detect disconnects early on the client side, you can also use heartbeats with a smaller interval.
    /// Use with caution, setting a low value (i.e. sending heartbeat very often) may cause an excessive network usage
    /// as well as high server load (when there are many connected clients).
    /// @param interval default value is 25 minutes (1 500 000 milliseconds), which is also the allowed maximum.
    /// @throws if value is not in the allowed range, e.g. larger than the maximum (1 500 000).
    void setHeartbeatInterval(std::chrono::milliseconds interval) {
        internal::checkErrOrThrow(obx_sync_heartbeat_interval(cPtr(), static_cast<uint64_t>(interval.count())));
    }

    /// Triggers the heartbeat sending immediately.
    void sendHeartbeat() { internal::checkErrOrThrow(obx_sync_send_heartbeat(cPtr())); }

    /// Sends the given 'objects message' from the client to the currently connected server.
    void send(SyncObjectsMessageBuilder&& message) {
        internal::checkErrOrThrow(obx_sync_send_msg_objects(cPtr(), message.release()));
    }

    /// Configures how sync updates are received from the server.
    /// If automatic sync updates are turned off, they will need to be requested manually.
    void setRequestUpdatesMode(OBXRequestUpdatesMode mode) {
        internal::checkErrOrThrow(obx_sync_request_updates_mode(cPtr(), mode));
    }

    /// Configures the maximum number of outgoing TX messages that can be sent without an ACK from the server.
    /// @throws if value is not in the valid range 1-20
    void maxMessagesInFlight(int value) { internal::checkErrOrThrow(obx_sync_max_messages_in_flight(cPtr(), value)); }

    /// Once the sync client is configured, you can "start" it to initiate synchronization.
    /// This method triggers communication in the background and will return immediately.
    /// If the synchronization destination is reachable, this background thread will connect to the server,
    /// log in (authenticate) and, depending on "update request mode", start syncing data.
    /// If the device, network or server is currently offline, connection attempts will be retried later using
    /// increasing backoff intervals.
    /// If you haven't set the credentials in the options during construction, call setCredentials() before start().
    void start() { internal::checkErrOrThrow(obx_sync_start(cPtr())); }

    /// Stops this sync client. Does nothing if it is already stopped.
    void stop() { internal::checkErrOrThrow(obx_sync_stop(cPtr())); }

    /// Request updates since we last synchronized our database.
    /// @param subscribeForFuturePushes to keep sending us future updates as they come in.
    /// @see updatesCancel() to stop the updates
    bool requestUpdates(bool subscribeForFuturePushes) {
        return internal::checkSuccessOrThrow(obx_sync_updates_request(cPtr(), subscribeForFuturePushes));
    }

    /// Cancel updates from the server so that it will stop sending updates.
    /// @see updatesRequest()
    bool cancelUpdates() { return internal::checkSuccessOrThrow(obx_sync_updates_cancel(cPtr())); }

    /// Count the number of messages in the outgoing queue, i.e. those waiting to be sent to the server.
    /// Note: This calls uses a (read) transaction internally: 1) it's not just a "cheap" return of a single number.
    ///       While this will still be fast, avoid calling this function excessively.
    ///       2) the result follows transaction view semantics, thus it may not always match the actual value.
    /// @return the number of messages in the outgoing queue
    uint64_t outgoingMessageCount(uint64_t limit = 0) {
        uint64_t result;
        internal::checkErrOrThrow(obx_sync_outgoing_message_count(cPtr(), limit, &result));
        return result;
    }

    // TODO remove c-style listeners to avoid confusion? Users would still be able use them directly through the C-API.

    /// @param listener set NULL to reset
    /// @param listenerArg is a pass-through argument passed to the listener
    void setConnectListener(OBX_sync_listener_connect* listener, void* listenerArg) {
        obx_sync_listener_connect(cPtr(), listener, listenerArg);
    }

    /// @param listener set NULL to reset
    /// @param listenerArg is a pass-through argument passed to the listener
    void setDisconnectListener(OBX_sync_listener_disconnect* listener, void* listenerArg) {
        obx_sync_listener_disconnect(cPtr(), listener, listenerArg);
    }

    /// @param listener set NULL to reset
    /// @param listenerArg is a pass-through argument passed to the listener
    void setLoginListener(OBX_sync_listener_login* listener, void* listenerArg) {
        obx_sync_listener_login(cPtr(), listener, listenerArg);
    }

    /// @param listener set NULL to reset
    /// @param listenerArg is a pass-through argument passed to the listener
    void setLoginFailureListener(OBX_sync_listener_login_failure* listener, void* listenerArg) {
        obx_sync_listener_login_failure(cPtr(), listener, listenerArg);
    }

    /// @param listener set NULL to reset
    /// @param listenerArg is a pass-through argument passed to the listener
    void setCompleteListener(OBX_sync_listener_complete* listener, void* listenerArg) {
        obx_sync_listener_complete(cPtr(), listener, listenerArg);
    }

    /// @param listener set NULL to reset
    /// @param listenerArg is a pass-through argument passed to the listener
    void setChangeListener(OBX_sync_listener_change* listener, void* listenerArg) {
        obx_sync_listener_change(cPtr(), listener, listenerArg);
    }

    void setLoginListener(std::shared_ptr<SyncClientLoginListener> listener) {
        Guard lock(listeners_.mutex);

        // if it was previously set, unassign in the core before (potentially) destroying the object
        removeLoginListener();

        if (listener) {
            listeners_.login = std::move(listener);
            void* listenerPtr = listeners_.login.get();

            obx_sync_listener_login(
                cPtr(), [](void* userData) { static_cast<SyncClientLoginListener*>(userData)->loggedIn(); },
                listenerPtr);
            obx_sync_listener_login_failure(
                cPtr(),
                [](void* userData, OBXSyncCode code) {
                    static_cast<SyncClientLoginListener*>(userData)->loginFailed(code);
                },
                listenerPtr);
        }
    }

    void setCompletionListener(std::shared_ptr<SyncClientCompletionListener> listener) {
        Guard lock(listeners_.mutex);

        // if it was previously set, unassign in the core before (potentially) destroying the object
        removeCompletionListener();

        if (listener) {
            listeners_.complete = std::move(listener);
            obx_sync_listener_complete(
                cPtr(), [](void* arg) { static_cast<SyncClientCompletionListener*>(arg)->updatesCompleted(); },
                listeners_.complete.get());
        }
    }

    void setConnectionListener(std::shared_ptr<SyncClientConnectionListener> listener) {
        Guard lock(listeners_.mutex);

        // if it was previously set, unassign in the core before (potentially) destroying the object
        removeConnectionListener();

        if (listener) {
            listeners_.connect = std::move(listener);
            void* listenerPtr = listeners_.connect.get();

            obx_sync_listener_connect(
                cPtr(), [](void* userData) { static_cast<SyncClientConnectionListener*>(userData)->connected(); },
                listenerPtr);
            obx_sync_listener_disconnect(
                cPtr(), [](void* userData) { static_cast<SyncClientConnectionListener*>(userData)->disconnected(); },
                listenerPtr);
        }
    }

    void setTimeListener(std::shared_ptr<SyncClientTimeListener> listener) {
        Guard lock(listeners_.mutex);

        // if it was previously set, unassign in the core before (potentially) destroying the object
        removeTimeListener();

        if (listener) {
            listeners_.time = std::move(listener);
            obx_sync_listener_server_time(
                cPtr(),
                [](void* arg, int64_t timestampNs) {
                    static_cast<SyncClientTimeListener*>(arg)->serverTime(
                        SyncClientTimeListener::TimePoint(std::chrono::nanoseconds(timestampNs)));
                },
                listeners_.time.get());
        }
    }

    void setChangeListener(std::shared_ptr<SyncChangeListener> listener) {
        Guard lock(listeners_.mutex);

        // if it was previously set, unassign in the core before (potentially) destroying the object
        removeChangeListener();

        if (listener) {
            listeners_.change = std::move(listener);
            obx_sync_listener_change(
                cPtr(),
                [](void* arg, const OBX_sync_change_array* cChanges) {
                    static_cast<SyncChangeListener*>(arg)->changed(cChanges);
                },
                listeners_.change.get());
        }
    }

    void setErrorListener(std::shared_ptr<SyncClientErrorListener> listener) {
        Guard lock(listeners_.mutex);

        removeErrorListener();

        if (listener) {
            listeners_.error = std::move(listener);
            obx_sync_listener_error(
                cPtr(),
                [](void* arg, OBXSyncError error) { static_cast<SyncClientErrorListener*>(arg)->errorOccurred(error); },
                listeners_.change.get());
        }
    }

    void setListener(std::shared_ptr<SyncClientListener> listener) {
        Guard lock(listeners_.mutex);

        // if it was previously set, unassign in the core before (potentially) destroying the object
        bool forceRemove = listeners_.combined != nullptr;
        removeLoginListener(forceRemove);
        removeCompletionListener(forceRemove);
        removeErrorListener(forceRemove);
        removeConnectionListener(forceRemove);
        removeTimeListener(forceRemove);
        removeChangeListener(forceRemove);
        listeners_.combined.reset();

        if (listener) {
            listeners_.combined = std::move(listener);
            void* listenerPtr = listeners_.combined.get();

            // Note: we need to use a templated forward* method so that the override for the right class is called.
            obx_sync_listener_login(
                cPtr(), [](void* arg) { static_cast<SyncClientListener*>(arg)->loggedIn(); }, listenerPtr);
            obx_sync_listener_login_failure(
                cPtr(), [](void* arg, OBXSyncCode code) { static_cast<SyncClientListener*>(arg)->loginFailed(code); },
                listenerPtr);
            obx_sync_listener_complete(
                cPtr(), [](void* arg) { static_cast<SyncClientListener*>(arg)->updatesCompleted(); }, listenerPtr);
            obx_sync_listener_error(
                cPtr(),
                [](void* arg, OBXSyncError error) { static_cast<SyncClientListener*>(arg)->errorOccurred(error); },
                listenerPtr);
            obx_sync_listener_connect(
                cPtr(), [](void* arg) { static_cast<SyncClientListener*>(arg)->connected(); }, listenerPtr);
            obx_sync_listener_disconnect(
                cPtr(), [](void* arg) { static_cast<SyncClientListener*>(arg)->disconnected(); }, listenerPtr);
            obx_sync_listener_server_time(
                cPtr(),
                [](void* arg, int64_t timestampNs) {
                    static_cast<SyncClientListener*>(arg)->serverTime(
                        SyncClientTimeListener::TimePoint(std::chrono::nanoseconds(timestampNs)));
                },
                listenerPtr);
            obx_sync_listener_change(
                cPtr(),
                [](void* arg, const OBX_sync_change_array* cChanges) {
                    static_cast<SyncClientListener*>(arg)->changed(cChanges);
                },
                listenerPtr);
        }
    }

    void setObjectsMessageListener(std::shared_ptr<SyncObjectsMessageListener> listener) {
        Guard lock(listeners_.mutex);

        // Keep the previous listener (if any) alive so the core may still call it before we finally switch.
        listeners_.object.swap(listener);

        if (listeners_.object) {  // switch if a new listener was given
            obx_sync_listener_msg_objects(
                cPtr(),
                [](void* arg, const OBX_sync_msg_objects* cObjects) {
                    static_cast<SyncObjectsMessageListener*>(arg)->received(cObjects);
                },
                listeners_.object.get());
        } else if (listener) {  // unset the previous listener, if set
            obx_sync_listener_msg_objects(cPtr(), nullptr, nullptr);
        }
    }

    /// Get u64 value for sync statistics.
    uint64_t statsValueU64(OBXSyncStats counterType) {
        uint64_t value;
        internal::checkErrOrThrow(obx_sync_stats_u64(cPtr(), counterType, &value));
        return value;
    }

protected:
    OBX_sync* cPtr() const {
        OBX_sync* ptr = cSync_;
        if (ptr == nullptr) throw IllegalStateException("Sync client was already closed");
        return ptr;
    }

    /// Close, but non-virtual to allow calls from constructor/destructor.
    void closeNonVirtual() {
        OBX_sync* ptr = cSync_.exchange(nullptr);
        if (ptr) {
            {
                std::lock_guard<std::mutex> lock(store_.syncClientMutex_);
                store_.syncClient_.reset();
            }
            internal::checkErrOrThrow(obx_sync_close(ptr));
        }
    }

    void removeLoginListener(bool evenIfEmpty = false) {
        std::shared_ptr<SyncClientLoginListener> listener = std::move(listeners_.login);
        if (listener || evenIfEmpty) {
            obx_sync_listener_login(cPtr(), nullptr, nullptr);
            obx_sync_listener_login_failure(cPtr(), nullptr, nullptr);
        }
    }

    void removeCompletionListener(bool evenIfEmpty = false) {
        std::shared_ptr<SyncClientCompletionListener> listener = std::move(listeners_.complete);
        if (listener || evenIfEmpty) {
            obx_sync_listener_complete(cPtr(), nullptr, nullptr);
            listener.reset();
        }
    }

    void removeErrorListener(bool evenIfEmpty = false) {
        std::shared_ptr<SyncClientErrorListener> listener = std::move(listeners_.error);
        if (listener || evenIfEmpty) {
            obx_sync_listener_error(cPtr(), nullptr, nullptr);
            listener.reset();
        }
    }

    void removeConnectionListener(bool evenIfEmpty = false) {
        std::shared_ptr<SyncClientConnectionListener> listener = std::move(listeners_.connect);
        if (listener || evenIfEmpty) {
            obx_sync_listener_connect(cPtr(), nullptr, nullptr);
            obx_sync_listener_disconnect(cPtr(), nullptr, nullptr);
            listener.reset();
        }
    }

    void removeTimeListener(bool evenIfEmpty = false) {
        std::shared_ptr<SyncClientTimeListener> listener = std::move(listeners_.time);
        if (listener || evenIfEmpty) {
            obx_sync_listener_server_time(cPtr(), nullptr, nullptr);
            listener.reset();
        }
    }

    void removeChangeListener(bool evenIfEmpty = false) {
        std::shared_ptr<SyncChangeListener> listener = std::move(listeners_.change);
        if (listener || evenIfEmpty) {
            obx_sync_listener_change(cPtr(), nullptr, nullptr);
            listener.reset();
        }
    }
};

/// <a href="https://objectbox.io/sync/">ObjectBox Sync</a> makes data available on other devices.
/// Start building a sync client using client() and connect to a remote server.
class Sync {
public:
    static bool isAvailable() { return obx_has_feature(OBXFeature_Sync); }

    /// Creates a sync client associated with the given store and configures it with the given options.
    /// This does not initiate any connection attempts yet: call SyncClient::start() to do so.
    /// Before start(), you can still configure some aspects of the sync client, e.g. its "request update" mode.
    /// @note While you may not interact with SyncClient directly after start(), you need to hold on to the object.
    ///       Make sure the SyncClient is not destroyed and thus synchronization can keep running in the background.
    static std::shared_ptr<SyncClient> client(Store& store, const std::string& serverUrl,
                                              const SyncCredentials& creds) {
        std::lock_guard<std::mutex> lock(store.syncClientMutex_);
        if (store.syncClient_) throw IllegalStateException("Only one sync client can be active for a store");
        store.syncClient_.reset(new SyncClient(store, serverUrl, creds));
        return std::static_pointer_cast<SyncClient>(store.syncClient_);
    }

    /// Adopts an existing OBX_sync* sync client, taking ownership of the pointer.
    /// @param cSync an initialized sync client. You must NOT call obx_sync_close() yourself anymore.
    static std::shared_ptr<SyncClient> client(Store& store, OBX_sync* cSync) {
        std::lock_guard<std::mutex> lock(store.syncClientMutex_);
        if (store.syncClient_) throw IllegalStateException("Only one sync client can be active for a store");
        store.syncClient_.reset(new SyncClient(store, cSync));
        return std::static_pointer_cast<SyncClient>(store.syncClient_);
    }
};

inline std::shared_ptr<SyncClient> Store::syncClient() {
    std::lock_guard<std::mutex> lock(syncClientMutex_);
    return std::static_pointer_cast<SyncClient>(syncClient_);
}

/// The ObjectBox Sync Server to run within your application (embedded server).
/// Note that you need a special sync edition, which includes the server components. Check https://objectbox.io/sync/.
class SyncServer : public Closable {
    OBX_sync_server* cPtr_;
    std::unique_ptr<Store> store_;

    /// Groups all listeners and the mutex that protects access to them. We could have a separate mutex for each
    /// listener but that's probably an overkill.
    struct {
        std::mutex mutex;
        std::shared_ptr<SyncChangeListener> change;
        std::shared_ptr<SyncObjectsMessageListener> object;
    } listeners_;

    using Guard = std::lock_guard<std::mutex>;

public:
    static bool isAvailable() { return obx_has_feature(OBXFeature_SyncServer); }

    /// Prepares an ObjectBox Sync Server to run within your application (embedded server) at the given URI.
    /// This call opens a store with the given options (as Store() does). Get it via store().
    /// The server's store is tied to the server itself and is closed when the server is closed.
    /// Before actually starting the server via start(), you can configure:
    /// - accepted credentials via setCredentials() (always required)
    /// - SSL certificate info via setCertificatePath() (required if you use wss)
    /// \note The model given via store_options is also used to verify the compatibility of the models presented by
    /// clients.
    ///       E.g. a client with an incompatible model will be rejected during login.
    /// @param storeOptions Options for the server's store. Will be "consumed"; do not use the Options object again.
    /// @param url The URL (following the pattern protocol:://IP:port) the server should listen on.
    ///        Supported \b protocols are "ws" (WebSockets) and "wss" (secure WebSockets).
    ///        To use the latter ("wss"), you must also call obx_sync_server_certificate_path().
    ///        To bind to all available \b interfaces, including those that are available from the "outside", use
    ///        0.0.0.0 as the IP. On the other hand, "127.0.0.1" is typically (may be OS dependent) only available on
    ///        the same device. If you do not require a fixed \b port, use 0 (zero) as a port to tell the server to pick
    ///        an arbitrary port that is available. The port can be queried via obx_sync_server_port() once the server
    ///        was started. \b Examples: "ws://0.0.0.0:9999" could be used during development (no certificate config
    ///        needed), while in a production system, you may want to use wss and a specific IP for security reasons.
    explicit SyncServer(Options& storeOptions, const std::string& url)
        : cPtr_(obx_sync_server(storeOptions.release(), url.c_str())) {
        internal::checkPtrOrThrow(cPtr_, "Could not create SyncServer");
        try {
            OBX_store* cStore = obx_sync_server_store(cPtr_);
            internal::checkPtrOrThrow(cStore, "Could not get SyncServer's store");
            store_.reset(new Store(cStore, false));
        } catch (...) {
            close();
            throw;
        }
    }

    /// Rvalue variant of SyncServer(Options& storeOptions, const std::string& url) that works equivalently.
    explicit SyncServer(Options&& storeOptions, const std::string& url)
        : SyncServer(static_cast<Options&>(storeOptions), url) {}

    SyncServer(SyncServer&& source) noexcept : cPtr_(source.cPtr_) {
        source.cPtr_ = nullptr;
        Guard lock(source.listeners_.mutex);
        std::swap(listeners_.change, source.listeners_.change);
    }

    /// Can't be copied, single owner of C resources is required (to avoid double-free during destruction)
    SyncServer(const SyncServer&) = delete;

    ~SyncServer() override {
        try {
            close();
        } catch (...) {
        }
    }

    /// The store that is associated with this server.
    Store& store() {
        OBX_VERIFY_STATE(store_);
        return *store_;
    }

    /// Closes and cleans up all resources used by this sync server. Does nothing if already closed.
    /// It can no longer be used afterwards, make a new sync server instead.
    void close() final {
        OBX_sync_server* ptr = cPtr_;
        cPtr_ = nullptr;
        store_.reset();
        if (ptr) {
            internal::checkErrOrThrow(obx_sync_server_close(ptr));
        }
    }

    /// Returns if this sync server is closed and can no longer be used.
    bool isClosed() override { return cPtr_ == nullptr; }

    /// Sets SSL certificate for the server to use. Use before start().
    void setCertificatePath(const std::string& path) {
        internal::checkErrOrThrow(obx_sync_server_certificate_path(cPtr(), path.c_str()));
    }

    /// Sets credentials for the server to accept. Use before start().
    /// @param data may be NULL in combination with OBXSyncCredentialsType_NONE
    void setCredentials(const SyncCredentials& creds) {
        if (creds.type_ == OBXSyncCredentialsType_OBX_ADMIN_USER ||
            creds.type_ == OBXSyncCredentialsType_USER_PASSWORD) {
            throw obx::IllegalArgumentException("Use enableAuthenticationType() instead");
        }
        if (!creds.username_.empty() || !creds.password_.empty()) {
            throw obx::IllegalArgumentException("This function does not support username/password");
        }
        internal::checkErrOrThrow(obx_sync_server_credentials(
            cPtr(), creds.type_, creds.data_.empty() ? nullptr : creds.data_.data(), creds.data_.size()));
    }

    /// Sets credentials for the server to accept. Use before start().
    /// @param data may be NULL in combination with OBXSyncCredentialsType_NONE
    void enableAuthenticationType(OBXSyncCredentialsType type) {
        internal::checkErrOrThrow(obx_sync_server_enable_auth(cPtr(), type));
    }

    /// Sets the number of worker threads. Calll before start().
    /// @param thread_count The default is "0" which is hardware dependent, e.g. a multiple of CPU "cores".
    void setWorkerThreads(int threadCount) {
        internal::checkErrOrThrow(obx_sync_server_worker_threads(cPtr(), threadCount));
    }

    /// Sets a maximum size for sync history entries to limit storage: old entries are removed to stay below this limit.
    /// Deleting older history entries may require clients to do a full sync if they have not contacted the server for
    /// a certain time.
    /// @param maxSizeKb Once this maximum size is reached, old history entries are deleted (default 0: no limit).
    /// @param targetSizeKb If this value is non-zero, the deletion of old history entries is extended until reaching
    ///        this target (lower than the maximum) allowing deletion "batching", which may be more efficient.
    ///        If zero, the deletion stops already stops when reaching the max size (or lower).
    void setHistoryMaxSizeKb(uint64_t maxSizeKb, uint64_t targetSizeKb = 0) {
        internal::checkErrOrThrow(obx_sync_server_history_max_size_in_kb(cPtr(), maxSizeKb, targetSizeKb));
    }

    /// Once the sync server is configured, you can "start" it to start accepting client connections.
    /// This method triggers communication in the background and will return immediately.
    void start() { internal::checkErrOrThrow(obx_sync_server_start(cPtr())); }

    /// Stops this sync server. Does nothing if it is already stopped.
    void stop() { internal::checkErrOrThrow(obx_sync_server_stop(cPtr())); }

    /// Returns if this sync server is running
    bool isRunning() { return obx_sync_server_running(cPtr()); }

    /// Returns a URL this server is listening on, including the bound port (see port().
    std::string url() {
        return internal::checkedPtrOrThrow(obx_sync_server_url(cPtr()), "Can't get SyncServer bound URL");
    }

    /// Returns a port this server listens on. This is especially useful if the bindUri given to the constructor
    /// specified "0" port (i.e. automatic assignment).
    uint16_t port() {
        uint16_t result = obx_sync_server_port(cPtr());
        if (result == 0) internal::throwLastError();
        return result;
    }

    /// Returns the number of clients connected to this server.
    uint64_t connections() { return obx_sync_server_connections(cPtr()); }

    /// Get server runtime statistics.
    std::string statsString(bool includeZeroValues = true) {
        const char* serverStatsString = obx_sync_server_stats_string(cPtr(), includeZeroValues);
        return internal::checkedPtrOrThrow(serverStatsString, "Can't get SyncServer stats string");
    }

    /// Get u64 value for sync server statistics.
    uint64_t statsValueU64(OBXSyncServerStats counterType) {
        uint64_t value;
        internal::checkErrOrThrow(obx_sync_server_stats_u64(cPtr(), counterType, &value));
        return value;
    }

    /// Get double value for sync server statistics.
    double statsValueF64(OBXSyncServerStats counterType) {
        double value;
        internal::checkErrOrThrow(obx_sync_server_stats_f64(cPtr(), counterType, &value));
        return value;
    }

    void setChangeListener(std::shared_ptr<SyncChangeListener> listener) {
        Guard lock(listeners_.mutex);

        // Keep the previous listener (if any) alive so the core may still call it before we finally switch.
        // TODO, this is implemented differently (more efficiently?) than client listeners, consider changing there too?
        listeners_.change.swap(listener);

        if (listeners_.change) {  // switch if a new listener was given
            obx_sync_server_listener_change(
                cPtr(),
                [](void* arg, const OBX_sync_change_array* cChanges) {
                    static_cast<SyncChangeListener*>(arg)->changed(cChanges);
                },
                listeners_.change.get());
        } else if (listener) {  // unset the previous listener, if set
            obx_sync_server_listener_change(cPtr(), nullptr, nullptr);
        }
    }

    void setObjectsMessageListener(std::shared_ptr<SyncObjectsMessageListener> listener) {
        Guard lock(listeners_.mutex);

        // Keep the previous listener (if any) alive so the core may still call it before we finally switch.
        listeners_.object.swap(listener);

        if (listeners_.object) {  // switch if a new listener was given
            obx_sync_server_listener_msg_objects(
                cPtr(),
                [](void* arg, const OBX_sync_msg_objects* cObjects) {
                    static_cast<SyncObjectsMessageListener*>(arg)->received(cObjects);
                },
                listeners_.object.get());
        } else if (listener) {  // unset the previous listener, if set
            obx_sync_server_listener_msg_objects(cPtr(), nullptr, nullptr);
        }
    }

    /// Broadcast the given 'objects message' from the server to all currently connected (and logged-in) clients.
    void send(SyncObjectsMessageBuilder&& message) {
        internal::checkErrOrThrow(obx_sync_server_send_msg_objects(cPtr(), message.release()));
    }

    // TODO temporary public until Admin has c++ APIs
    //    protected:
    OBX_sync_server* cPtr() const {
        OBX_sync_server* ptr = cPtr_;
        if (ptr == nullptr) throw IllegalStateException("Sync server was already closed");
        return ptr;
    }
};

/// A helper class that delegates C style function pointers to C++ class method invocations via user data.
/// Also, it provides makeFunctions() and registerProtocol().
template <typename CLIENT>
class CustomMsgClientDelegate {
public:
    static std::shared_ptr<CLIENT>* sharedPtrPtr(void* clientUserData) {
        return static_cast<std::shared_ptr<CLIENT>*>(clientUserData);
    }

    static std::shared_ptr<CLIENT> sharedPtr(void* clientUserData) {
        return obx::internal::toRef(sharedPtrPtr(clientUserData));
    }

    static void* delegateCreate(uint64_t clientId, const char* url, const char* certPath, void* userConfig) {
        auto client = std::make_shared<CLIENT>(clientId, url, certPath, userConfig);
        return new std::shared_ptr<CLIENT>(std::move(client));
    }

    static obx_err delegateStart(void* clientUserData) {
        try {
            sharedPtr(clientUserData)->start();
        } catch (...) {
            return OBX_ERROR_GENERAL;
        }
        return OBX_SUCCESS;
    }

    static void delegateStop(void* clientUserData) { sharedPtr(clientUserData)->stop(); }

    static void delegateJoin(void* clientUserData) { sharedPtr(clientUserData)->join(); }

    static void delegateShutdown(void* clientUserData) {
        sharedPtr(clientUserData)->shutdown();
        delete sharedPtrPtr(clientUserData);
    }

    static bool delegateConnect(void* clientUserData) { return sharedPtr(clientUserData)->connect(); }

    static void delegateDisconnect(bool clearOutgoingMessages, void* clientUserData) {
        sharedPtr(clientUserData)->disconnect(clearOutgoingMessages);
    }

    static bool delegateSendAsync(OBX_bytes_lazy* cBytes, void* clientUserData) {
        return sharedPtr(clientUserData)->sendAsync(BytesLazy(cBytes));
    }

    static void delegateClearOutgoingMessages(void* clientUserData) {
        sharedPtr(clientUserData)->clearOutgoingMessages();
    }

    /// Create a OBX_custom_msg_client_functions struct according to the defined template delegates.
    static OBX_custom_msg_client_functions makeFunctions() {
        OBX_custom_msg_client_functions functions{};
        functions.version = sizeof(OBX_custom_msg_client_functions);
        functions.func_create = &delegateCreate;
        functions.func_start = &delegateStart;
        functions.func_stop = &delegateStop;
        functions.func_join = &delegateJoin;
        functions.func_shutdown = &delegateShutdown;
        functions.func_connect = &delegateConnect;
        functions.func_disconnect = &delegateDisconnect;
        functions.func_send_async = &delegateSendAsync;
        functions.func_clear_outgoing_messages = &delegateClearOutgoingMessages;
        return functions;
    }

    /// Must be called to register a protocol for your custom messaging client. Call before starting a client.
    /// @param protocol the communication protocol to use, e.g. "tcp"
    static void registerProtocol(const char* protocol, void* configUserData = nullptr) {
        OBX_custom_msg_client_functions functions = makeFunctions();
        internal::checkErrOrThrow(obx_custom_msg_client_register(protocol, &functions, configUserData));
    }
};

/// Typically used together with CustomMsgClientDelegate e.g. to ensure a matching interface.
/// Subclasses represent a custom client.
/// \note At this point, the overridden methods must not throw unless specified otherwise.
/// \note All virtual methods are pure virtual; no default implementation (e.g. {}) is provided to ensure the
///       implementor is aware of all the interactions and thus shall explicitly provide at least empty implementations.
class AbstractCustomMsgClient {
    const uint64_t id_;

public:
    explicit AbstractCustomMsgClient(uint64_t id) : id_(id) {}

    virtual ~AbstractCustomMsgClient() = default;

    /// ID for this client instance (was passed via the constructor).
    uint64_t id() const { return id_; }

    /// Tells the client to prepare for starting (to be implemented by concrete subclass).
    virtual void start() = 0;

    /// The custom client shall do any preparations to stop (to be implemented by concrete subclass).
    /// E.g. signal asynchronous resources (e.g. threads, async IO, ...) to stop.
    /// Note that there's no need to wait for asynchronous resources here; better use join() for this.
    virtual void stop() = 0;

    /// Called after stop() to wait for asynchronous resources here (e.g. join any spawned threads).
    virtual void join() = 0;

    /// The custom client shall do any preparations to shut down (to be implemented by concrete subclass).
    /// Ensure that everything is ready for the custom client to be destroyed:
    /// the custom client will be deleted right after this call (by the CustomMsgClientDelegate).
    virtual void shutdown() = 0;

    /// Tells the client it shall start trying to connect (to be implemented by concrete subclass).
    /// @returns true if the operation was successful.
    /// @returns false in case the operation encountered an issue.
    virtual bool connect() = 0;

    /// Tells the client it shall disconnect (to be implemented by concrete subclass).
    /// @param clearOutgoingMessages if true clearOutgoingMessages() will be called.
    virtual void disconnect(bool clearOutgoingMessages) = 0;

    /// Enqueue a message for sending (to be implemented by concrete subclass).
    /// @param message the message bytes.
    /// @return true if the process of async sending was initiated (e.g. enqueued for processing).
    /// @return false if no attempt of sending data will be made (e.g. connection was already closed).
    virtual bool sendAsync(BytesLazy&& message) = 0;

    /// Clear all outgoing messages (to be implemented by concrete subclass).
    virtual void clearOutgoingMessages() = 0;

    /// The custom msg client must call this whenever a message is received from the server.
    /// @param messageData the message bytes.
    /// @param messageSize the number of message bytes.
    /// @returns true if the given message could be forwarded.
    /// @returns false in case the operation encountered an issue.
    inline bool forwardReceivedMessageFromServer(const void* messageData, size_t messageSize) {
        obx_err err = obx_custom_msg_client_receive_message_from_server(id_, messageData, messageSize);
        return internal::checkSuccessOrThrow(err);
    }

    /// The custom msg client must call this whenever the state (according to given enum values) changes.
    /// @param state The state to forward
    /// @returns true if the client was in a state that allowed the transition to the given state.
    /// @returns false if no state transition was possible from the current to the given state (e.g. an internal
    /// "closed" state was reached).
    inline bool forwardState(OBXCustomMsgClientState state) {
        obx_err err = obx_custom_msg_client_set_state(id_, state);
        return internal::checkSuccessOrThrow(err);
    }

    /// The custom msg client may call this if it has knowledge when a reconnection attempt makes sense,
    /// for example, when the network becomes available.
    /// @returns true if a reconnect was actually triggered and false otherwise.
    inline bool triggerReconnect() {
        obx_err err = obx_custom_msg_client_trigger_reconnect(id_);
        return internal::checkSuccessOrThrow(err);
    }
};

/// A helper class that delegates C style function pointers to C++ class method invocations via user data.
/// Also, it provides makeFunctions() and registerProtocol().
template <typename SERVER, typename CONNECTION>
class CustomMsgServerDelegate {
public:
    static std::shared_ptr<SERVER>* sharedPtrPtr(void* serverUserData) {
        return static_cast<std::shared_ptr<SERVER>*>(serverUserData);
    }

    static std::shared_ptr<SERVER> sharedPtr(void* serverUserData) {
        return obx::internal::toRef(sharedPtrPtr(serverUserData));
    }

    static CONNECTION& refConnection(void* connectionUserData) {
        return obx::internal::toRef(static_cast<CONNECTION*>(connectionUserData));
    }

    static void* delegateCreate(uint64_t serverId, const char* url, const char* certPath, void* configUserData) {
        auto server = std::make_shared<SERVER>(serverId, url, certPath, configUserData);
        return new std::shared_ptr<SERVER>(std::move(server));
    }

    static obx_err delegateStart(void* serverUserData, uint64_t* outPort) {
        try {
            uint64_t port = sharedPtr(serverUserData)->start();
            if (outPort != nullptr) *outPort = port;
        } catch (...) {
            return OBX_ERROR_GENERAL;
        }
        return OBX_SUCCESS;
    }

    static void delegateStop(void* serverUserData) { sharedPtr(serverUserData)->stop(); }

    static void delegateShutdown(void* serverUserData) {
        sharedPtr(serverUserData)->shutdown();
        delete sharedPtrPtr(serverUserData);
    }

    static bool delegateClientConnSendAsync(OBX_bytes_lazy* cBytes, void* /*serverUserData*/,
                                            void* connectionUserData) {
        return refConnection(connectionUserData).sendAsync(BytesLazy(cBytes));
    }

    static void delegateClientConnClose(void* /*serverUserData*/, void* connectionUserData) {
        refConnection(connectionUserData).close();
    }

    static void delegateClientConnShutdown(void* connectionUserData) {
        refConnection(connectionUserData).shutdown();
        // Note: NOT deleting the connection here: it was created in user land, so we don't know how it was created
        //       there and if it was created at all... Thus, this must be taken care of in user land.
    }

    /// Create a OBX_custom_msg_server_functions struct according to the defined template delegates.
    static OBX_custom_msg_server_functions makeFunctions() {
        OBX_custom_msg_server_functions functions{};
        functions.version = sizeof(OBX_custom_msg_server_functions);
        functions.func_create = &delegateCreate;
        functions.func_start = &delegateStart;
        functions.func_stop = &delegateStop;
        functions.func_shutdown = &delegateShutdown;
        functions.func_conn_send_async = &delegateClientConnSendAsync;
        functions.func_conn_close = &delegateClientConnClose;
        functions.func_conn_shutdown = &delegateClientConnShutdown;
        return functions;
    }

    /// Must be called to register a protocol for your custom messaging server. Call before starting a server.
    /// @param protocol the communication protocol to use, e.g. "tcp"
    static void registerProtocol(const char* protocol, void* configUserData = nullptr) {
        OBX_custom_msg_server_functions functions = makeFunctions();
        internal::checkErrOrThrow(obx_custom_msg_server_register(protocol, &functions, configUserData));
    }
};

/// Typically used together with CustomMsgServerDelegate e.g. to ensure a matching interface.
/// Subclasses represent a connection of a custom server.
class AbstractCustomMsgConnection {
    const uint64_t serverId_;  ///< ID of the custom message server instance associated with this connection.
    uint64_t id_;              ///< Connection ID

public:
    explicit AbstractCustomMsgConnection(uint64_t serverId, uint64_t id = 0) : serverId_(serverId), id_(id) {}
    virtual ~AbstractCustomMsgConnection() = default;

    uint64_t serverId() const { return serverId_; }
    uint64_t id() const { return id_; }
    void setId(uint64_t id) { id_ = id; }

    /// The connection closing itself (to be implemented by concrete subclass).
    virtual void close() = 0;

    /// The connection shall shutdown; e.g. it may delete itself.
    /// Note that there is no "automatic" deletion triggered from the custom msg system:
    /// often, connections are intertwined with the server and thus deletion must be managed at the implementing side.
    virtual void shutdown() = 0;

    /// Offers bytes to be sent asynchronously to the client (to be implemented by concrete subclass).
    /// @param message the message bytes.
    /// @returns true if the operation was successful.
    /// @returns false in case the operation encountered an issue.
    virtual bool sendAsync(BytesLazy&& message) = 0;

    /// Clear all outgoing messages (to be implemented by concrete subclass).
    virtual void clearOutgoingMessages() = 0;
};

/// Used internally to decouple the lifetime of the user connection object from the one "managed" (created/deleted) by
/// AbstractCustomMsgServer/CustomMsgServerDelegate. This way, the user connection can be clear at any time.
class CustomMsgConnectionDelegate : public AbstractCustomMsgConnection {
    std::weak_ptr<AbstractCustomMsgConnection> delegate_;
    const bool deleteThisOnShutdown_;

public:
    explicit CustomMsgConnectionDelegate(uint64_t serverId,
                                         const std::shared_ptr<AbstractCustomMsgConnection>& connection,
                                         bool deleteThisOnShutdown)
        : AbstractCustomMsgConnection(serverId, 0),
          delegate_(connection),
          deleteThisOnShutdown_(deleteThisOnShutdown) {}

    ~CustomMsgConnectionDelegate() override = default;

    inline void close() override {
        std::shared_ptr<obx::AbstractCustomMsgConnection> delegate = delegate_.lock();
        if (delegate) delegate->close();
    };

    inline void shutdown() override {
        std::shared_ptr<obx::AbstractCustomMsgConnection> delegate = delegate_.lock();
        if (delegate) delegate->shutdown();
        if (deleteThisOnShutdown_) delete this;
    };

    inline bool sendAsync(BytesLazy&& message) override {
        std::shared_ptr<obx::AbstractCustomMsgConnection> delegate = delegate_.lock();
        return delegate && delegate->sendAsync(std::move(message));
    };

    inline void clearOutgoingMessages() override {
        std::shared_ptr<obx::AbstractCustomMsgConnection> delegate = delegate_.lock();
        if (delegate) delegate->clearOutgoingMessages();
    };
};

/// Typically used together with CustomMsgServerDelegate e.g. to ensure a matching interface.
/// Subclasses represent a custom server.
/// \note At this point, the overridden methods must not throw unless specified otherwise.
/// \note All virtual methods are pure virtual; no default implementation (e.g. {}) is provided to ensure the
///       implementor is aware of all the interactions and thus shall explicitly provide at least empty implementations.
class AbstractCustomMsgServer {
    const uint64_t id_;

public:
    explicit AbstractCustomMsgServer(const uint64_t id) : id_(id) {}
    virtual ~AbstractCustomMsgServer() = default;

    /// ID for this server instance (was passed via the constructor).
    uint64_t id() const { return id_; }

    /// The custom server shall do any preparations to start (to be implemented by concrete subclass).
    /// The implementation may throw an exception to signal that starting was not successful;
    /// note that exception specifics are ignored, e.g. the type and any exception message.
    /// @returns The custom server can optionally supply a "port";
    ///          the value is arbitrary and, for now, is only used for debug logs.
    virtual uint64_t start() = 0;

    /// The custom server shall do any preparations to stop (to be implemented by concrete subclass).
    /// E.g. signal asynchronous resources (e.g. threads, async IO, ...) to stop.
    /// Note that there's no need to wait for asynchronous resources here; better use shutdown() for this.
    virtual void stop() = 0;

    /// The custom server shall do any preparations to shut down (to be implemented by concrete subclass).
    /// Ensure that everything is ready for the custom server to be destroyed:
    /// the custom server will be deleted right after this call (by the CustomMsgServerDelegate).
    virtual void shutdown() = 0;

    /// Must be called from the custom server when a new client connection becomes available.
    /// If successful, the ID is also set at the given connection.
    /// @param connection will be held internally as a weak ptr, so it should affect its lifetime only mildly.
    ///        Only when a callback is delegated, it will have hold a strong (shared_ptr) reference for that time.
    /// @returns client connection ID (never 0; also set at the given connection)
    /// @throws Exception in case the operation encountered an exceptional issue
    inline uint64_t addConnection(const std::shared_ptr<AbstractCustomMsgConnection>& connection) {
        if (!connection) throw IllegalArgumentException("No connection was provided");
        auto delegate =
            std::unique_ptr<CustomMsgConnectionDelegate>(new CustomMsgConnectionDelegate(id(), connection, true));
        uint64_t connectionId = obx_custom_msg_server_add_client_connection(id(), delegate.get());
        internal::checkIdOrThrow(connectionId, "Could not add custom server connection");
        delegate->setId(connectionId);
        delegate.release();  // Safe to release now  // NOLINT(bugprone-unused-return-value)
        connection->setId(connectionId);
        return connectionId;
    }

    /// Must be called from the custom server when a client connection becomes inactive (e.g. closed) and can be
    /// removed.
    /// @param connectionId ID of the connection the messages originated from.
    /// @returns true if the given message could be forwarded.
    /// @returns false in case the operation encountered an issue.
    /// @throws Exception in case the operation encountered an exceptional issue
    inline bool removeConnection(uint64_t connectionId) {
        obx_err err = obx_custom_msg_server_remove_client_connection(id(), connectionId);
        return internal::checkSuccessOrThrow(err);
    }

    /// Short hand for removeConnection(connection.id()).
    inline bool removeConnection(const AbstractCustomMsgConnection& connection) {
        return removeConnection(connection.id());
    }

    /// The custom msg server must call this whenever a message is received from a client connection.
    /// @param connectionId ID of the connection the messages originated from.
    /// @param messageData the message bytes.
    /// @param messageSize the number of message bytes.
    /// @returns true if the given message could be forwarded.
    /// @returns false in case the operation encountered an issue.
    /// @throws Exception in case the operation encountered an exceptional issue
    inline bool forwardReceivedMessageFromClient(uint64_t connectionId, const void* messageData, size_t messageSize) {
        obx_err err = obx_custom_msg_server_receive_message_from_client(id(), connectionId, messageData, messageSize);
        return internal::checkSuccessOrThrow(err);
    }
};

/**@}*/  // end of doxygen group
}  // namespace obx