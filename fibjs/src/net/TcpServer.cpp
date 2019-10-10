/*
 * TcpServer.cpp
 *
 *  Created on: Aug 8, 2012
 *      Author: lion
 */

#include "object.h"
#include "TcpServer.h"
#include "ifs/mq.h"
#include "ifs/console.h"

namespace fibjs {

result_t _new_tcpServer(exlib::string addr, int32_t port,
    Handler_base* listener, obj_ptr<TcpServer_base>& retVal,
    v8::Local<v8::Object> This)
{
    obj_ptr<TcpServer> svr = new TcpServer();
    svr->wrap(This);

    result_t hr = svr->create(addr, port, listener);
    if (hr < 0)
        return hr;

    retVal = svr;

    return 0;
}

result_t TcpServer_base::_new(int32_t port, Handler_base* listener,
    obj_ptr<TcpServer_base>& retVal, v8::Local<v8::Object> This)
{
    return _new("", port, listener, retVal, This);
}

result_t TcpServer_base::_new(exlib::string addr, int32_t port,
    Handler_base* listener, obj_ptr<TcpServer_base>& retVal,
    v8::Local<v8::Object> This)
{
    return _new_tcpServer(addr, port, listener, retVal, This);
}

TcpServer::TcpServer()
{
    m_running = false;
}

result_t TcpServer::create(exlib::string addr, int32_t port,
    Handler_base* listener)
{
    result_t hr;

    hr = Socket_base::_new(net_base::_AF_INET, net_base::_SOCK_STREAM,
        m_socket);
    if (hr < 0)
        return hr;

    hr = m_socket->bind(addr, port, false);
    if (hr < 0)
        return hr;

    hr = m_socket->listen(1024);
    if (hr < 0)
        return hr;

    set_handler(listener);

    return 0;
}

result_t TcpServer::start()
{
    class asyncInvoke : public AsyncState {
    public:
        asyncInvoke(TcpServer* pThis, Socket_base* pSock, ValueHolder* holder)
            : AsyncState(NULL)
            , m_pThis(pThis)
            , m_sock(pSock)
            , m_holder(holder)
        {
            next(invoke);
        }

    public:
        static int32_t invoke(AsyncState* pState, int32_t n)
        {
            asyncInvoke* pThis = (asyncInvoke*)pState;

            return mq_base::invoke(pThis->m_pThis->m_hdlr, pThis->m_sock, pThis->next(close));
        }

        static int32_t close(AsyncState* pState, int32_t n)
        {
            asyncInvoke* pThis = (asyncInvoke*)pState;

            return pThis->m_sock->close(pThis->next());
        }

        virtual int32_t error(int32_t v)
        {
            errorLog("TcpServer: " + getResultMessage(v));
            return next(close);
        }

        virtual Isolate* isolate()
        {
            return m_pThis->holder();
        }

    private:
        obj_ptr<TcpServer> m_pThis;
        obj_ptr<Socket_base> m_sock;
        obj_ptr<Handler_base> m_hdlr;
        obj_ptr<ValueHolder> m_holder;
    };

    class asyncAccept : public AsyncState {
    public:
        asyncAccept(TcpServer* pThis, ValueHolder* holder)
            : AsyncState(NULL)
            , m_pThis(pThis)
            , m_holder(holder)
        {
            m_pThis->holder()->Ref();
            next(accept);
        }

    public:
        static int32_t accept(AsyncState* pState, int32_t n)
        {
            asyncAccept* pThis = (asyncAccept*)pState;

            return pThis->m_pThis->m_socket->accept(pThis->m_accept, pThis->next(invoke));
        }

        static int32_t invoke(AsyncState* pState, int32_t n)
        {
            asyncAccept* pThis = (asyncAccept*)pState;

            if (pThis->m_accept) {
                (new asyncInvoke(pThis->m_pThis, pThis->m_accept, pThis->m_holder))->apost(0);
                pThis->m_accept.Release();
            }

            return pThis->m_pThis->m_socket->accept(pThis->m_accept, pThis);
        }

        virtual int32_t error(int32_t v)
        {
            if (v == CALL_E_BAD_FILE || v == CALL_E_INVALID_CALL) {
                m_pThis->holder()->Unref();
                return next();
            }

            errorLog("TcpServer: " + getResultMessage(v));
            return 0;
        }

    private:
        obj_ptr<TcpServer> m_pThis;
        obj_ptr<Socket_base> m_accept;
        obj_ptr<ValueHolder> m_holder;
    };

    if (!m_socket)
        return CHECK_ERROR(CALL_E_INVALID_CALL);

    if (m_running)
        return CHECK_ERROR(CALL_E_INVALID_CALL);

    m_running = true;

    obj_ptr<ValueHolder> holder = new ValueHolder(wrap());
    (new asyncAccept(this, holder))->apost(0);
    return 0;
}

result_t TcpServer::stop(AsyncEvent* ac)
{
    if (!m_socket)
        return CHECK_ERROR(CALL_E_INVALID_CALL);

    return m_socket->close(ac);
}

result_t TcpServer::get_socket(obj_ptr<Socket_base>& retVal)
{
    if (!m_socket)
        return CHECK_ERROR(CALL_E_INVALID_CALL);

    retVal = m_socket;
    return 0;
}

result_t TcpServer::get_handler(obj_ptr<Handler_base>& retVal)
{
    retVal = m_hdlr;
    return 0;
}

result_t TcpServer::set_handler(Handler_base* newVal)
{
    SetPrivate("handler", newVal->wrap());
    m_hdlr = newVal;

    return 0;
}

} /* namespace fibjs */
