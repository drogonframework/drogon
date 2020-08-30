/**
 *
 *  @file CouchBaseConnection.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#define LCB_IOPS_V12_NO_DEPRECATE
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#include "CouchBaseConnection.h"
#include "CouchBaseResultImpl.h"
#include <trantor/net/EventLoop.h>
#include <trantor/net/Channel.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <libcouchbase/plugins/io/bsdio-inl.c>
using namespace drogon::nosql;

void CouchBaseConnection::lcbDestroyIoOpts(struct lcb_io_opt_st *iops)
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    self->ioop_.reset();
}
void CouchBaseConnection::lcbDeleteEvent(struct lcb_io_opt_st *iops,
                                         lcb_socket_t sock,
                                         void *event)
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    if (self->channelPtr_)
    {
        int ev = *static_cast<int *>(event);
        int events = self->channelPtr_->events();
        if (ev & trantor::Channel::kWriteEvent)
        {
            events &= ~trantor::Channel::kWriteEvent;
        }
        if (ev & trantor::Channel::kReadEvent)
        {
            events &= ~trantor::Channel::kReadEvent;
        }
        if (events != self->channelPtr_->events())
        {
            self->channelPtr_->updateEvents(events);
        }
    }
    (void)sock;
}

void CouchBaseConnection::lcbDeleteTimer(struct lcb_io_opt_st *iops,
                                         void *event)
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    auto timeId = static_cast<trantor::TimerId *>(event);
    self->loop_->invalidateTimer(*timeId);
    self->timerMap_.erase(*timeId);
}

int CouchBaseConnection::lcbUpdateEvent(struct lcb_io_opt_st *iops,
                                        lcb_socket_t sock,
                                        void *event,
                                        short flags,
                                        void *cb_data,
                                        void (*handler)(lcb_socket_t sock,
                                                        short which,
                                                        void *cb_data))
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    self->loop_->assertInLoopThread();
    int *evt = static_cast<int *>(event);
    if (!self->channelPtr_)
    {
        self->channelPtr_ =
            std::make_unique<trantor::Channel>(self->loop_, sock);
    }
    int events = 0;

    if (flags & LCB_READ_EVENT)
    {
        events |= trantor::Channel::kReadEvent;
    }
    if (flags & LCB_WRITE_EVENT)
    {
        events |= trantor::Channel::kWriteEvent;
    }
    *evt = events;
    if (self->channelPtr_->events() == events &&
        std::pair<EventHandler, void *>(handler, cb_data) ==
            self->handlerMap_[events])
    {
        /* no change! */
        return 0;
    }
    if (self->handlerMap_[events] !=
        std::pair<EventHandler, void *>(handler, cb_data))
    {
        self->handlerMap_[events] = std::make_pair(handler, cb_data);
        self->channelPtr_->setEventCallback([self, sock, cb_data, handler]() {
            auto revent = self->channelPtr_->revents();
            short which{0};
            if (revent & trantor::Channel::kReadEvent)
            {
                which |= LCB_READ_EVENT;
            }
            if (revent & trantor::Channel::kWriteEvent)
            {
                which |= LCB_WRITE_EVENT;
            }
            handler(sock, which, cb_data);
        });
    }
    if (self->channelPtr_->events() != events)
        self->channelPtr_->updateEvents(events);
    return 0;
}
int CouchBaseConnection::lcbUpdateTimer(struct lcb_io_opt_st *iops,
                                        void *timer,
                                        lcb_uint32_t usec,
                                        void *cb_data,
                                        void (*handler)(lcb_socket_t sock,
                                                        short which,
                                                        void *cb_data))
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    auto timerId = static_cast<trantor::TimerId *>(timer);
    auto iter = self->timerMap_.find(*timerId);
    if (iter != self->timerMap_.end() && iter->second == handler)
    {
        return 0;
    }
    if (iter != self->timerMap_.end())
    {
        self->timerMap_.erase(iter);
        self->loop_->invalidateTimer(*timerId);
    }
    *timerId =
        self->loop_->runAfter(std::chrono::microseconds(usec),
                              [handler, cb_data]() { handler(0, 0, cb_data); });
    assert(*timerId);
    self->timerMap_[*timerId] = handler;
    return 0;
}
void *CouchBaseConnection::lcbCreateEvent(struct lcb_io_opt_st *iops)
{
    int *event = new int(0);
    (void)iops;
    return event;
}
void *CouchBaseConnection::lcbCreateTimer(struct lcb_io_opt_st *iops)
{
    trantor::TimerId *id = new trantor::TimerId(0);
    (void)iops;
    return id;
}
void CouchBaseConnection::lcbDestroyEvent(struct lcb_io_opt_st *iops,
                                          void *event)
{
    delete static_cast<int *>(event);
}
void CouchBaseConnection::lcbDestroyTimer(struct lcb_io_opt_st *iops,
                                          void *event)
{
    lcbDeleteTimer(iops, event);
    delete static_cast<trantor::TimerId *>(event);
}
void nullFunction(lcb_io_opt_t iops)
{
}
void CouchBaseConnection::procs2TrantorCallback(
    int version,
    lcb_loop_procs *loop_procs,
    lcb_timer_procs *timer_procs,
    lcb_bsd_procs *bsd_procs,
    lcb_ev_procs *ev_procs,
    lcb_completion_procs *completion_procs,
    lcb_iomodel_t *iomodel)
{
    ev_procs->cancel = CouchBaseConnection::lcbDeleteEvent;
    ev_procs->create = CouchBaseConnection::lcbCreateEvent;
    ev_procs->watch = CouchBaseConnection::lcbUpdateEvent;
    ev_procs->destroy = CouchBaseConnection::lcbDestroyEvent;

    timer_procs->create = CouchBaseConnection::lcbCreateTimer;
    timer_procs->cancel = CouchBaseConnection::lcbDeleteTimer;
    timer_procs->schedule = CouchBaseConnection::lcbUpdateTimer;
    timer_procs->destroy = CouchBaseConnection::lcbDestroyTimer;

    loop_procs->start = nullFunction;
    loop_procs->stop = nullFunction;
    loop_procs->tick = nullFunction;

    *iomodel = LCB_IOMODEL_EVENT;
    wire_lcb_bsd_impl2(bsd_procs, version);
}

CouchBaseConnection::CouchBaseConnection(const drogon::string_view &connStr,
                                         const drogon::string_view &username,
                                         const drogon::string_view &password,
                                         const drogon::string_view &bucket,
                                         trantor::EventLoop *loop)
    : loop_(loop),
      connString_(connStr),
      userName_(username),
      password_(password),
      bucket_(bucket)
{
    LOG_DEBUG << "Connect to " << connString_;
    loop->queueInLoop([this]() { connect(); });
}

void CouchBaseConnection::connect()
{
    ioop_ = std::make_unique<lcb_io_opt_st>();
    /* setup io iops! */
    ioop_->version = 3;
    ioop_->dlhandle = NULL;
    ioop_->destructor = CouchBaseConnection::lcbDestroyIoOpts;
    ioop_->v.v3.get_procs = procs2TrantorCallback;

    /* consider that struct isn't allocated by the library,
     * `need_cleanup' flag might be set in lcb_create() */
    ioop_->v.v3.need_cleanup = 0;

    assert(loop_);
    ioop_->v.v3.cookie = this;

    wire_lcb_bsd_impl(ioop_.get());

    lcb_STATUS error;
    lcb_CREATEOPTS *options = NULL;

    lcb_createopts_create(&options, LCB_TYPE_BUCKET);
    if (!connString_.empty())
    {
        lcb_createopts_connstr(options, connString_.data(), connString_.size());
    }
    if (!userName_.empty())
    {
        lcb_createopts_credentials(options,
                                   userName_.data(),
                                   userName_.size(),
                                   password_.data(),
                                   password_.size());
    }
    if (!bucket_.empty())
    {
        lcb_createopts_bucket(options, bucket_.data(), bucket_.size());
    }
    lcb_createopts_io(options, ioop_.get());
    error = lcb_create(&instance_, options);
    lcb_createopts_destroy(options);
    if (error != LCB_SUCCESS)
    {
        LOG_ERROR << "Failed to create a libcouchbase instance: "
                  << lcb_strerror_short(error);
        handleClose();
        return;
    }
    lcb_set_cookie(instance_, this);

    /* Set up the callbacks */
    lcb_set_bootstrap_callback(instance_,
                               CouchBaseConnection::bootstrapCallback);
    lcb_install_callback(instance_,
                         LCB_CALLBACK_GET,
                         (lcb_RESPCALLBACK)CouchBaseConnection::getCallback);
    lcb_install_callback(instance_,
                         LCB_CALLBACK_STORE,
                         (lcb_RESPCALLBACK)CouchBaseConnection::storeCallback);

    if ((error = lcb_connect(instance_)) != LCB_SUCCESS)
    {
        LOG_ERROR << "Failed to connect libcouchbase instance: "
                  << lcb_strerror_short(error);
        lcb_destroy(instance_);
        handleClose();
    }
}
CouchBaseConnection::~CouchBaseConnection()
{
    LOG_DEBUG << "destroy connection";
}

void CouchBaseConnection::bootstrapCallback(lcb_INSTANCE *instance,
                                            lcb_STATUS err)
{
    CouchBaseConnection *self =
        (CouchBaseConnection *)(lcb_get_cookie(instance));
    if (err != LCB_SUCCESS)
    {
        LOG_ERROR << "bootstrap error: " << lcb_strerror_short(err);
        self->loop_->queueInLoop([self, instance]() {
            lcb_destroy(instance);
            self->handleClose();
        });
        return;
    }
    LOG_TRACE << "bootstrap successfully";
    self->okCallback_(self->shared_from_this());
}

void CouchBaseConnection::getCallback(lcb_INSTANCE *instance,
                                      int cbtype,
                                      const lcb_RESPGET *rg)
{
    const char *value;
    size_t nvalue;
    lcb_STATUS rc = lcb_respget_status(rg);

    if (rc != LCB_SUCCESS)
    {
        LOG_ERROR << "Failed to get key: " << lcb_strerror_short(rc);
        // TODO: exception callback here;
    }
    std::shared_ptr<CouchBaseResultImpl> resultImplPtr =
        std::make_shared<GetLcbResult>(rg);
    LOG_DEBUG << "Success to get key";
    CouchBaseConnection *self =
        (CouchBaseConnection *)(lcb_get_cookie(instance));
    self->callback_(CouchBaseResult(std::move(resultImplPtr)));
}

void CouchBaseConnection::storeCallback(lcb_INSTANCE *instance,
                                        int cbtype,
                                        const lcb_RESPSTORE *resp)
{
    lcb_STATUS rc = lcb_respstore_status(resp);
    if (rc != LCB_SUCCESS)
    {
        LOG_ERROR << "failed to store key: " << lcb_strerror_short(rc);
        // TODO:: call exception callback;
    }
    printf("stored key 'foo'\n");

    std::shared_ptr<CouchBaseResultImpl> resultImplPtr =
        std::make_shared<StoreLcbResult>(resp);
    // TODO: callback;
    (void)cbtype;
}

void CouchBaseConnection::getInLoop(const std::string &key,
                                    CBCallback &&callback,
                                    ExceptionCallback &&errorCallback)
{
    lcb_STATUS rc;
    lcb_CMDGET *gcmd;
    callback_ = std::move(callback);
    errorCallback_ = std::move(errorCallback);
    lcb_cmdget_create(&gcmd);
    lcb_cmdget_key(gcmd, key.data(), key.size());
    rc = lcb_get(instance_, NULL, gcmd);
    lcb_cmdget_destroy(gcmd);
    if (rc != LCB_SUCCESS)
    {
        LOG_ERROR << "failed to schedule get request:"
                  << lcb_strerror_short(rc);
        lcb_destroy(instance_);
        handleClose();
        return;
    }
}

void CouchBaseConnection::handleClose()
{
    loop_->assertInLoopThread();
    if (channelPtr_)
    {
        channelPtr_->disableAll();
        channelPtr_->remove();
        assert(closeCallback_);
        auto thisPtr = shared_from_this();
        closeCallback_(thisPtr);
    }
}