//
// detail/impl/amiga_event_reactor.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// GFOpenBoost: the Amiga reactor (see detail/amiga_event_reactor.hpp).
// Built from select_reactor.ipp: the op and timer handling is Asio's; the
// wait is one event.library round.
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
//

#ifndef BOOST_ASIO_DETAIL_IMPL_AMIGA_EVENT_REACTOR_IPP
#define BOOST_ASIO_DETAIL_IMPL_AMIGA_EVENT_REACTOR_IPP

#include <boost/asio/detail/config.hpp>

#if defined(BOOST_ASIO_HAS_AMIGA_EVENT)

#include <boost/asio/detail/amiga_event_reactor.hpp>
#include <boost/asio/detail/scheduler.hpp>
#include <boost/asio/detail/throw_error.hpp>
#include <boost/asio/error.hpp>
#include <cerrno>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

amiga_event_reactor::amiga_event_reactor(boost::asio::execution_context& ctx)
  : execution_context_service_base<amiga_event_reactor>(ctx),
    scheduler_(use_service<scheduler_type>(ctx)),
    mutex_(),
    ev_(gfob_ev_create()),
    round_signals_(-1),
    shutdown_(false)
{
  if (!ev_)
  {
    // event.library 1 (LIBS:) is the reactor's wait; no fallback.
    boost::system::error_code ec(errno ? errno : ENOSYS,
        boost::asio::error::get_system_category());
    boost::asio::detail::throw_error(ec, gfob_ev_error());
  }
}

amiga_event_reactor::~amiga_event_reactor()
{
  shutdown();
  gfob_ev_delete(ev_);
}

void amiga_event_reactor::shutdown()
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  shutdown_ = true;
  lock.unlock();

  op_queue<operation> ops;
  for (int i = 0; i < max_ops; ++i)
    op_queue_[i].get_all_operations(ops);
  for (std::size_t i = 0; i < exec_waits_.size(); ++i)
    ops.push(exec_waits_[i]);
  exec_waits_.clear();
  timer_queues_.get_all_timers(ops);
  scheduler_.abandon_operations(ops);
}

void amiga_event_reactor::notify_fork(boost::asio::execution_context::fork_event)
{
}

void amiga_event_reactor::init_task()
{
  scheduler_.init_task();
}

int amiga_event_reactor::register_descriptor(socket_type,
    amiga_event_reactor::per_descriptor_data&)
{
  return 0;
}

int amiga_event_reactor::register_internal_descriptor(
    int op_type, socket_type descriptor,
    amiga_event_reactor::per_descriptor_data&, reactor_op* op)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  op_queue_[op_type].enqueue_operation(descriptor, op);
  interrupt();
  return 0;
}

void amiga_event_reactor::move_descriptor(socket_type,
    amiga_event_reactor::per_descriptor_data&,
    amiga_event_reactor::per_descriptor_data&)
{
}

void amiga_event_reactor::call_post_immediate_completion(
    operation* op, bool is_continuation, const void* self)
{
  static_cast<const amiga_event_reactor*>(self)->post_immediate_completion(
      op, is_continuation);
}

void amiga_event_reactor::start_op(int op_type, socket_type descriptor,
    amiga_event_reactor::per_descriptor_data&, reactor_op* op,
    bool is_continuation, bool,
    void (*on_immediate)(operation*, bool, const void*),
    const void* immediate_arg)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);

  if (shutdown_)
  {
    on_immediate(op, is_continuation, immediate_arg);
    return;
  }

  bool first = op_queue_[op_type].enqueue_operation(descriptor, op);
  scheduler_.work_started();
  if (first)
    interrupt();
}

void amiga_event_reactor::cancel_ops(socket_type descriptor,
    amiga_event_reactor::per_descriptor_data&)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  cancel_ops_unlocked(descriptor, boost::asio::error::operation_aborted);
}

void amiga_event_reactor::cancel_ops_by_key(socket_type descriptor,
    amiga_event_reactor::per_descriptor_data&,
    int op_type, void* cancellation_key)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  bool need_interrupt = op_queue_[op_type].cancel_operations_by_key(
      descriptor, ops, cancellation_key, boost::asio::error::operation_aborted);
  scheduler_.post_deferred_completions(ops);
  if (need_interrupt)
    interrupt();
}

void amiga_event_reactor::deregister_descriptor(socket_type descriptor,
    amiga_event_reactor::per_descriptor_data&, bool)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  cancel_ops_unlocked(descriptor, boost::asio::error::operation_aborted);
}

void amiga_event_reactor::deregister_internal_descriptor(
    socket_type descriptor, amiga_event_reactor::per_descriptor_data&)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  for (int i = 0; i < max_ops; ++i)
    op_queue_[i].cancel_operations(descriptor, ops);
}

void amiga_event_reactor::cleanup_descriptor_data(
    amiga_event_reactor::per_descriptor_data&)
{
}

void amiga_event_reactor::start_exec_wait(amiga_wait_op* op)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);

  if (shutdown_)
  {
    scheduler_.post_immediate_completion(op, false);
    return;
  }

  exec_waits_.push_back(op);
  scheduler_.work_started();
  interrupt();
}

std::size_t amiga_event_reactor::cancel_exec_waits(void* owner)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  std::size_t n = 0, j = 0;
  for (std::size_t i = 0; i < exec_waits_.size(); ++i)
  {
    amiga_wait_op* op = exec_waits_[i];
    if (op->owner_ == owner)
    {
      op->ec_ = boost::asio::error::operation_aborted;
      ops.push(op);
      ++n;
    }
    else
      exec_waits_[j++] = op;
  }
  exec_waits_.resize(j);
  lock.unlock();
  scheduler_.post_deferred_completions(ops);
  return n;
}

void amiga_event_reactor::run(long usec, op_queue<operation>& ops)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);

  // Name this round's sockets, signals and ports.
  gfob_ev_begin(ev_);
  round_sockets_.clear();
  round_ports_.clear();
  round_signals_ = -1;

  bool have_work_to_do = !timer_queues_.all_empty() || !exec_waits_.empty();
  for (int i = 0; i < max_select_ops; ++i)
  {
    have_work_to_do = have_work_to_do || !op_queue_[i].empty();
    reactor_op_queue<socket_type>::iterator it = op_queue_[i].begin();
    while (it != op_queue_[i].end())
    {
      reactor_op_queue<socket_type>::iterator op_iter = it++;
      int slot = gfob_ev_add_socket(ev_, op_iter->first, 1u << i);
      if (slot < 0)
      {
        boost::system::error_code ec(slot == -1
            ? boost::asio::error::not_socket : boost::asio::error::no_memory);
        op_queue_[i].cancel_operations(op_iter, ops, ec);
        continue;
      }
      round_socket rs = { op_iter->first, slot };
      round_sockets_.push_back(rs);
    }
  }
  for (std::size_t i = 0; i < exec_waits_.size(); ++i)
  {
    amiga_wait_op* op = exec_waits_[i];
    if (op->port_)
      round_ports_.push_back(gfob_ev_add_port(ev_, op->port_));
    else
    {
      round_ports_.push_back(-1);
      round_signals_ = gfob_ev_add_signals(ev_, op->mask_);
    }
  }

  if (!usec && !have_work_to_do)
    return;

  long wait_usec = usec ? get_timeout_usec(usec) : 0;

  lock.unlock();
  long n = gfob_ev_run_once(ev_, wait_usec);
  lock.lock();

  if (n != 0)
  {
    // Sockets: each slot names one descriptor and one op type. Except,
    // write, then read, as select_reactor performs them.
    for (int i = max_select_ops - 1; i >= 0; --i)
    {
      for (std::size_t k = 0; k < round_sockets_.size(); ++k)
      {
        unsigned long r = gfob_ev_ready(ev_, round_sockets_[k].slot);
        if (r & (1u << i))
          op_queue_[i].perform_operations(round_sockets_[k].fd, ops);
      }
    }

    // Exec waits: each ends with the signals that came, or its port's
    // message (left on the port for the handler to GetMsg()).
    unsigned long sigs = round_signals_ >= 0
      ? gfob_ev_ready(ev_, round_signals_) : 0;
    std::size_t j = 0;
    for (std::size_t i = 0; i < exec_waits_.size(); ++i)
    {
      amiga_wait_op* op = exec_waits_[i];
      bool done = false;
      if (op->port_)
      {
        if (i < round_ports_.size() && round_ports_[i] >= 0
            && gfob_ev_ready(ev_, round_ports_[i]))
        {
          op->got_ = 1;
          done = true;
        }
      }
      else if (i < round_ports_.size() && (sigs & op->mask_))
      {
        op->got_ = sigs & op->mask_;
        done = true;
      }
      if (done)
      {
        op->ec_ = boost::system::error_code();
        ops.push(op);
      }
      else
        exec_waits_[j++] = op;
    }
    exec_waits_.resize(j);
  }
  timer_queues_.get_ready_timers(ops);
}

void amiga_event_reactor::interrupt()
{
  gfob_ev_wake(ev_);
}

void amiga_event_reactor::do_add_timer_queue(timer_queue_base& queue)
{
  mutex::scoped_lock lock(mutex_);
  timer_queues_.insert(&queue);
}

void amiga_event_reactor::do_remove_timer_queue(timer_queue_base& queue)
{
  mutex::scoped_lock lock(mutex_);
  timer_queues_.erase(&queue);
}

long amiga_event_reactor::get_timeout_usec(long usec)
{
  const long max_usec = 5 * 60 * 1000 * 1000;
  return timer_queues_.wait_duration_usec(
      (usec < 0 || max_usec < usec) ? max_usec : usec);
}

void amiga_event_reactor::cancel_ops_unlocked(socket_type descriptor,
    const boost::system::error_code& ec)
{
  bool need_interrupt = false;
  op_queue<operation> ops;
  for (int i = 0; i < max_ops; ++i)
    need_interrupt = op_queue_[i].cancel_operations(
        descriptor, ops, ec) || need_interrupt;
  scheduler_.post_deferred_completions(ops);
  if (need_interrupt)
    interrupt();
}

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // defined(BOOST_ASIO_HAS_AMIGA_EVENT)

#endif // BOOST_ASIO_DETAIL_IMPL_AMIGA_EVENT_REACTOR_IPP
