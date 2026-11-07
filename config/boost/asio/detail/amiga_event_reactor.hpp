//
// detail/amiga_event_reactor.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// GFOpenBoost: Boost.Asio's reactor for AmigaOS, on event.library. One wait
// (bsdsocket.library's WaitSelect() with a signal mask, inside
// event.library) covers the program's sockets, Exec signals and MsgPorts,
// so a GUI or ARexx port shares the io_context with the network and nothing
// polls. The same op queues and timer queues as select_reactor; only the
// wait, the wake (a signal, not a socket pair) and the Exec waits
// (boost/asio/amiga/exec_waiter.hpp) are new.
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
//

#ifndef BOOST_ASIO_DETAIL_AMIGA_EVENT_REACTOR_HPP
#define BOOST_ASIO_DETAIL_AMIGA_EVENT_REACTOR_HPP

#include <boost/asio/detail/config.hpp>

#if defined(BOOST_ASIO_HAS_AMIGA_EVENT)

#include <cstddef>
#include <vector>
#include <boost/asio/detail/amiga_event_api.hpp>
#include <boost/asio/detail/amiga_wait_op.hpp>
#include <boost/asio/detail/limits.hpp>
#include <boost/asio/detail/mutex.hpp>
#include <boost/asio/detail/op_queue.hpp>
#include <boost/asio/detail/reactor_op.hpp>
#include <boost/asio/detail/reactor_op_queue.hpp>
#include <boost/asio/detail/scheduler_task.hpp>
#include <boost/asio/detail/socket_types.hpp>
#include <boost/asio/detail/timer_queue_base.hpp>
#include <boost/asio/detail/timer_queue_set.hpp>
#include <boost/asio/detail/wait_op.hpp>
#include <boost/asio/execution_context.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

class amiga_event_reactor
  : public execution_context_service_base<amiga_event_reactor>,
    public scheduler_task
{
public:
  enum op_types { read_op = 0, write_op = 1, except_op = 2,
    max_select_ops = 3, connect_op = 1, max_ops = 3 };

  struct per_descriptor_data
  {
  };

  BOOST_ASIO_DECL amiga_event_reactor(boost::asio::execution_context& ctx);
  BOOST_ASIO_DECL ~amiga_event_reactor();
  BOOST_ASIO_DECL void shutdown();
  BOOST_ASIO_DECL void notify_fork(
      boost::asio::execution_context::fork_event fork_ev);
  BOOST_ASIO_DECL void init_task();
  BOOST_ASIO_DECL int register_descriptor(socket_type, per_descriptor_data&);
  BOOST_ASIO_DECL int register_internal_descriptor(
      int op_type, socket_type descriptor,
      per_descriptor_data& descriptor_data, reactor_op* op);

  void post_immediate_completion(operation* op, bool is_continuation) const;
  BOOST_ASIO_DECL static void call_post_immediate_completion(
      operation* op, bool is_continuation, const void* self);

  BOOST_ASIO_DECL void start_op(int op_type, socket_type descriptor,
      per_descriptor_data&, reactor_op* op, bool is_continuation, bool,
      void (*on_immediate)(operation*, bool, const void*),
      const void* immediate_arg);

  void start_op(int op_type, socket_type descriptor,
      per_descriptor_data& descriptor_data, reactor_op* op,
      bool is_continuation, bool allow_speculative)
  {
    start_op(op_type, descriptor, descriptor_data,
        op, is_continuation, allow_speculative,
        &amiga_event_reactor::call_post_immediate_completion, this);
  }

  BOOST_ASIO_DECL void cancel_ops(socket_type descriptor, per_descriptor_data&);
  BOOST_ASIO_DECL void cancel_ops_by_key(socket_type descriptor,
      per_descriptor_data& descriptor_data,
      int op_type, void* cancellation_key);
  BOOST_ASIO_DECL void deregister_descriptor(socket_type descriptor,
      per_descriptor_data&, bool closing);
  BOOST_ASIO_DECL void deregister_internal_descriptor(
      socket_type descriptor, per_descriptor_data&);
  BOOST_ASIO_DECL void cleanup_descriptor_data(per_descriptor_data&);
  BOOST_ASIO_DECL void move_descriptor(socket_type descriptor,
      per_descriptor_data& target_descriptor_data,
      per_descriptor_data& source_descriptor_data);

  template <typename TimeTraits, typename Allocator>
  void add_timer_queue(timer_queue<TimeTraits, Allocator>& queue);
  template <typename TimeTraits, typename Allocator>
  void remove_timer_queue(timer_queue<TimeTraits, Allocator>& queue);
  template <typename TimeTraits, typename Allocator>
  void schedule_timer(timer_queue<TimeTraits, Allocator>& queue,
      const typename TimeTraits::time_type& time,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& timer,
      wait_op* op);
  template <typename TimeTraits, typename Allocator>
  std::size_t cancel_timer(timer_queue<TimeTraits, Allocator>& queue,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& timer,
      std::size_t max_cancelled = (std::numeric_limits<std::size_t>::max)());
  template <typename TimeTraits, typename Allocator>
  void cancel_timer_by_key(timer_queue<TimeTraits, Allocator>& queue,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data* timer,
      void* cancellation_key);
  template <typename TimeTraits, typename Allocator>
  void move_timer(timer_queue<TimeTraits, Allocator>& queue,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& target,
      typename timer_queue<TimeTraits, Allocator>::per_timer_data& source);

  // Exec waits (boost::asio::exec_waiter): op->mask_ signals, or a
  // message on op->port_. Completed with the signals that came (got_).
  BOOST_ASIO_DECL void start_exec_wait(amiga_wait_op* op);

  // Cancel the Exec waits that owner started. Returns how many.
  BOOST_ASIO_DECL std::size_t cancel_exec_waits(void* owner);

  // One wait, at most usec microseconds (-1: until something is ready).
  BOOST_ASIO_DECL void run(long usec, op_queue<operation>& ops);

  // End the wait (a signal: from any task).
  BOOST_ASIO_DECL void interrupt();

private:
  BOOST_ASIO_DECL void do_add_timer_queue(timer_queue_base& queue);
  BOOST_ASIO_DECL void do_remove_timer_queue(timer_queue_base& queue);
  BOOST_ASIO_DECL long get_timeout_usec(long usec);
  BOOST_ASIO_DECL void cancel_ops_unlocked(socket_type descriptor,
      const boost::system::error_code& ec);

  typedef class scheduler scheduler_type;
  scheduler_type& scheduler_;
  boost::asio::detail::mutex mutex_;
  gfob_ev* ev_;
  reactor_op_queue<socket_type> op_queue_[max_ops];
  timer_queue_set timer_queues_;
  std::vector<amiga_wait_op*> exec_waits_;

  struct round_socket { socket_type fd; int slot; };
  std::vector<round_socket> round_sockets_;
  std::vector<int> round_ports_;
  int round_signals_;

  bool shutdown_;
};

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#include <boost/asio/detail/impl/amiga_event_reactor.hpp>
#if defined(BOOST_ASIO_HEADER_ONLY)
# include <boost/asio/detail/impl/amiga_event_reactor.ipp>
#endif // defined(BOOST_ASIO_HEADER_ONLY)

#endif // defined(BOOST_ASIO_HAS_AMIGA_EVENT)

#endif // BOOST_ASIO_DETAIL_AMIGA_EVENT_REACTOR_HPP
