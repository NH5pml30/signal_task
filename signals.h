#pragma once
#include <functional>

#include "intrusive_list.h"

namespace signals
{
  template<typename T>
  class signal;

  template<typename... Args>
  class signal<void (Args...)>
  {
  private:
    using slot_t = std::function<void(Args...)>;

    class connection_base : public intrusive::list_element<struct connection_tag>
    {
      using base_t = intrusive::list_element<connection_tag>;

    public:
      connection_base() = default;
      connection_base(signal *sig) noexcept : sig(sig)
      {
        sig->connections.push_front(*this);
      }

      connection_base(connection_base &&other) noexcept : base_t(std::move(other)), sig(other.sig)
      {
        change_enclosing_iterators(
            other, [res = sig->connections.get_iterator(*this)](auto) { return res; });
      }

      connection_base &operator=(connection_base &&other) noexcept
      {
        base_t::operator=(std::move(other));
        sig = other.sig;
        change_enclosing_iterators(
            other, [res = sig->connections.get_iterator(*this)](auto) { return res; });
        return *this;
      }

      void disconnect() noexcept
      {
        change_enclosing_iterators(*this, [&](auto cur) { return std::next(cur); });
        unlink();
      }

      ~connection_base()
      {
        change_enclosing_iterators(*this, [&](auto cur) { return std::next(cur); });
      }

    private:
      template<class IteratorSupplier>
      void change_enclosing_iterators(const connection_base &other,
                                      IteratorSupplier supplier) const noexcept
      {
        if (is_connected())
          for (iteration_stack *el = sig->top_iterator; el; el = el->next)
            if (el->current != sig->connections.end() && &*el->current == &other)
              el->current = supplier(el->current);
      }

      signal *sig;
    };

  public:
    class connection : public connection_base
    {
      friend class signal;

    public:
      connection() = default;

      connection(signal *sig, slot_t slot) noexcept : connection_base(sig), slot(std::move(slot))
      {
      }

      connection(connection &&) noexcept = default;
      connection &operator=(connection &&) noexcept = default;
      ~connection() = default;

    private:
      slot_t slot;
    };

    signal() = default;
    signal(const signal &) = delete;
    signal &operator=(const signal &) = delete;

    ~signal()
    {
      if (top_iterator != nullptr)
        top_iterator->is_destroyed = true;
    }

    connection connect(slot_t slot) noexcept
    {
      return connection(this, slot);
    }

    void operator()(Args... args) const
    {
      typename connections_t::const_iterator current = connections.end();
      iteration_stack el = {current, top_iterator};
      top_iterator = &el;
      try
      {
        el.current = connections.begin();
        while (el.current != connections.end())
        {
          auto copy = el.current;
          ++el.current;
          static_cast<const connection &>(*copy).slot(args...);
          if (el.is_destroyed)
            return;
        }
      }
      catch (...)
      {
        if (!el.is_destroyed)
          top_iterator = el.next;
        throw;
      }

      if (!el.is_destroyed)
        top_iterator = el.next;
    }

  private:
    using connections_t = intrusive::list<connection_base, connection_tag>;

    struct iteration_stack
    {
      typename connections_t::const_iterator current;
      iteration_stack *next;
      bool is_destroyed = false;

      ~iteration_stack()
      {
        if (is_destroyed && next != nullptr)
          next->is_destroyed = true;
      }
    };

    connections_t connections;
    mutable iteration_stack *top_iterator = nullptr;
  };
}  // namespace signals
