//
// detail/impl/amiga_event_reactor.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// GFOpenBoost: the timer half of the Amiga reactor, as select_reactor's.
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
//

#ifndef BOOST_ASIO_DETAIL_IMPL_AMIGA_EVENT_REACTOR_HPP
#define BOOST_ASIO_DETAIL_IMPL_AMIGA_EVENT_REACTOR_HPP

#if defined(BOOST_ASIO_HAS_AMIGA_EVENT)

#include <boost/asio/detail/scheduler.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

inline void amiga_event_reactor::post_immediate_completion(
    operation* op, bool is_continuation) const
{
  scheduler_.post_immediate_completion(op, is_continuation);
}

template <typename TimeTraits, typename Allocator>
void amiga_event_reactor::add_timer_queue(
    timer_queue<TimeTraits, Allocator>& queue)
{
  do_add_timer_queue(queue);
}

// Remove a timer queue from the reactor.
template <typename TimeTraits, typename Allocator>
void amiga_event_reactor::remove_timer_queue(
    timer_queue<TimeTraits, Allocator>& queue)
{
  do_remove_timer_queue(queue);
}

template <typename TimeTraits, typename Allocator>
void amiga_event_reactor::schedule_timer(
    timer_queue<TimeTraits, Allocator>& queue,
    const typename TimeTraits::time_type& time,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& timer,
    wait_op* op)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);

  if (shutdown_)
  {
    scheduler_.post_immediate_completion(op, false);
    return;
  }

  bool earliest = queue.enqueue_timer(time, timer, op);
  scheduler_.work_started();
  if (earliest)
    interrupt();
}

template <typename TimeTraits, typename Allocator>
std::size_t amiga_event_reactor::cancel_timer(
    timer_queue<TimeTraits, Allocator>& queue,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& timer,
    std::size_t max_cancelled)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  std::size_t n = queue.cancel_timer(timer, ops, max_cancelled);
  lock.unlock();
  scheduler_.post_deferred_completions(ops);
  return n;
}

template <typename TimeTraits, typename Allocator>
void amiga_event_reactor::cancel_timer_by_key(
    timer_queue<TimeTraits, Allocator>& queue,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data* timer,
    void* cancellation_key)
{
  mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  queue.cancel_timer_by_key(timer, ops, cancellation_key);
  lock.unlock();
  scheduler_.post_deferred_completions(ops);
}

template <typename TimeTraits, typename Allocator>
void amiga_event_reactor::move_timer(timer_queue<TimeTraits, Allocator>& queue,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& target,
    typename timer_queue<TimeTraits, Allocator>::per_timer_data& source)
{
  boost::asio::detail::mutex::scoped_lock lock(mutex_);
  op_queue<operation> ops;
  queue.cancel_timer(target, ops);
  queue.move_timer(target, source);
  lock.unlock();
  scheduler_.post_deferred_completions(ops);
}

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // defined(BOOST_ASIO_HAS_AMIGA_EVENT)

#endif // BOOST_ASIO_DETAIL_IMPL_AMIGA_EVENT_REACTOR_HPP
