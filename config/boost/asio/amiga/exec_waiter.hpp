//
// amiga/exec_waiter.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// GFOpenBoost: wait for Exec signals or a MsgPort in a Boost.Asio
// io_context, in the same wait as its sockets and timers (the Amiga reactor,
// on event.library). So an Intuition window, an ARexx port or a
// commodity's port runs in the program's one event loop:
//
//   boost::asio::io_context io;
//   boost::asio::exec_waiter w(io);
//   w.async_wait_port(win->UserPort,
//       [&](boost::system::error_code ec, unsigned long) {
//         while (struct IntuiMessage* m = (struct IntuiMessage*)GetMsg(win->UserPort))
//           { ...; ReplyMsg(&m->ExecMessage); }
//         ... wait again ...
//       });
//   w.async_wait_signals(SIGBREAKF_CTRL_C,
//       [&](boost::system::error_code ec, unsigned long got) { io.stop(); });
//   io.run();
//
// Needs C++14 or later. A wait is one-shot, as Asio's: call async_wait_* again for the next.
// The handler gets the signals that came (async_wait_signals) or 1
// (async_wait_port: a message is there; the handler takes it with GetMsg()),
// or operation_aborted after cancel() or when the waiter goes.
// The loop takes the signals it reports, as Wait() does. A port's message
// stays on the port until the handler takes it.
//
// (In boost::asio itself: the compiler defines "amiga" and "amigaos" as
// macros, so neither can be a namespace.)
//
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
//

#ifndef BOOST_ASIO_AMIGA_EXEC_WAITER_HPP
#define BOOST_ASIO_AMIGA_EXEC_WAITER_HPP

#include <boost/asio/detail/config.hpp>

#if defined(BOOST_ASIO_HAS_AMIGA_EVENT)

#include <cstddef>
#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detail/amiga_event_reactor.hpp>
#include <boost/asio/detail/amiga_wait_op.hpp>
#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/fenced_block.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_work.hpp>
#include <boost/asio/detail/memory.hpp>
#include <boost/system/error_code.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

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

class exec_waiter
{
public:
  typedef io_context::executor_type executor_type;

  explicit exec_waiter(io_context& ctx)
    : executor_(ctx.get_executor()),
      reactor_(boost::asio::use_service<detail::amiga_event_reactor>(ctx))
  {
    reactor_.init_task();   // so the io_context runs the reactor's wait
  }

  ~exec_waiter()
  {
    cancel();
  }

  executor_type get_executor() const noexcept
  {
    return executor_;
  }

  // Wait for any of mask's signals. Handler: void(error_code, unsigned long got)
  template <typename WaitToken>
  auto async_wait_signals(unsigned long mask, WaitToken&& token)
  {
    return async_initiate<WaitToken,
      void (boost::system::error_code, unsigned long)>(
        initiate_wait(this), token, mask, (void*)0);
  }

  // Wait for a message on port (a struct MsgPort *).
  // Handler: void(error_code, unsigned long) (1 when a message is there)
  template <typename WaitToken>
  auto async_wait_port(void* port, WaitToken&& token)
  {
    return async_initiate<WaitToken,
      void (boost::system::error_code, unsigned long)>(
        initiate_wait(this), token, 0ul, port);
  }

  // End every wait this waiter started (handlers get operation_aborted).
  std::size_t cancel()
  {
    return reactor_.cancel_exec_waits(this);
  }

private:
  exec_waiter(const exec_waiter&) = delete;
  exec_waiter& operator=(const exec_waiter&) = delete;

  class initiate_wait
  {
  public:
    explicit initiate_wait(exec_waiter* self) : self_(self) {}

    template <typename Handler>
    void operator()(Handler&& handler, unsigned long mask, void* port) const
    {
      typedef typename std::decay<Handler>::type handler_type;
      handler_type h(static_cast<Handler&&>(handler));
      typedef detail::amiga_wait_handler<handler_type, executor_type> op;
      typename op::ptr p = { boost::asio::detail::addressof(h),
        op::ptr::allocate(h), 0 };
      p.p = new (p.v) op(h, self_->executor_);
      p.p->mask_ = mask;
      p.p->port_ = port;
      p.p->owner_ = self_;
      self_->reactor_.start_exec_wait(p.p);
      p.v = p.p = 0;
    }

  private:
    exec_waiter* self_;
  };

  executor_type executor_;
  detail::amiga_event_reactor& reactor_;
};

} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // defined(BOOST_ASIO_HAS_AMIGA_EVENT)

#endif // BOOST_ASIO_AMIGA_EXEC_WAITER_HPP
