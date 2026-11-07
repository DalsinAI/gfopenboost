//
// detail/amiga_wait_op.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// GFOpenBoost: a wait for Exec signals or a MsgPort, completed by the Amiga
// reactor (amiga_event_reactor) with the signals that came.
//
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
//

#ifndef BOOST_ASIO_DETAIL_AMIGA_WAIT_OP_HPP
#define BOOST_ASIO_DETAIL_AMIGA_WAIT_OP_HPP

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/fenced_block.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_work.hpp>
#include <boost/asio/detail/memory.hpp>
#include <boost/asio/detail/operation.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

class amiga_wait_op
  : public operation
{
public:
  boost::system::error_code ec_;
  unsigned long mask_;   // signals to wait for (0: a port wait)
  void* port_;           // the struct MsgPort * to wait on (0: a signal wait)
  unsigned long got_;    // the signals that came; 1 for a port with a message
  void* owner_;          // the waiter that started it, for cancel()

protected:
  amiga_wait_op(func_type func)
    : operation(func), mask_(0), port_(0), got_(0), owner_(0)
  {
  }
};

template <typename Handler, typename IoExecutor>
class amiga_wait_handler : public amiga_wait_op
{
public:
  BOOST_ASIO_DEFINE_HANDLER_PTR(amiga_wait_handler);

  amiga_wait_handler(Handler& h, const IoExecutor& io_ex)
    : amiga_wait_op(&amiga_wait_handler::do_complete),
      handler_(static_cast<Handler&&>(h)),
      work_(handler_, io_ex)
  {
  }

  static void do_complete(void* owner, operation* base,
      const boost::system::error_code& /*ec*/,
      std::size_t /*bytes_transferred*/)
  {
    amiga_wait_handler* h(static_cast<amiga_wait_handler*>(base));
    ptr p = { boost::asio::detail::addressof(h->handler_), h, h };

    handler_work<Handler, IoExecutor> w(
        static_cast<handler_work<Handler, IoExecutor>&&>(h->work_));

    detail::binder2<Handler, boost::system::error_code, unsigned long>
      handler(h->handler_, h->ec_, h->got_);
    p.h = boost::asio::detail::addressof(handler.handler_);
    p.reset();

    if (owner)
    {
      fenced_block b(fenced_block::half);
      w.complete(handler, handler.handler_);
    }
  }

private:
  Handler handler_;
  handler_work<Handler, IoExecutor> work_;
};

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_AMIGA_WAIT_OP_HPP
