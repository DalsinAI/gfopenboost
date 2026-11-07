//
// detail/amiga_wait_op.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// GFOpenBoost: a wait for Exec signals or a MsgPort, completed by the Amiga
// reactor (amiga_event_reactor) with the signals that came. Its handler
// type (amiga_wait_handler) is in boost/asio/amiga/exec_waiter.hpp.
//
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
//

#ifndef BOOST_ASIO_DETAIL_AMIGA_WAIT_OP_HPP
#define BOOST_ASIO_DETAIL_AMIGA_WAIT_OP_HPP

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/operation.hpp>
#include <boost/system/error_code.hpp>

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

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_AMIGA_WAIT_OP_HPP
