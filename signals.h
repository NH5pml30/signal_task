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
  public:
    class connection;

  private:
    using slot_t = std::function<void(Args...)>;

    class connection_base : public intrusive::list_element<struct connection_tag>
    {
      using base_t = intrusive::list_element<connection_tag>;

      connection * get_this()
      {
        return static_cast<connection *>(this);
      }

    public:
      connection_base() = default;
      connection_base(signal *sig) noexcept : sig(sig)
      {
        sig->connections.push_front(*get_this());
      }

      connection_base(connection_base &&other) noexcept : base_t(std::move(other)), sig(other.sig)
      {
        change_enclosing_iterators(
            other, [res = sig->connections.get_iterator(*get_this())](auto) {
              return res;
            });
      }

      connection_base &operator=(connection_base &&other) noexcept
      {
        base_t::operator=(std::move(other));
        sig = other.sig;
        change_enclosing_iterators(
            other, [res = sig->connections.get_iterator(*get_this())](auto) {
              return res;
            });
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
        top_iterator->sig = nullptr;
    }

    connection connect(slot_t slot) noexcept
    {
      return connection(this, slot);
    }

    void operator()(Args... args) const
    {
      iteration_stack el(this);
      while (el.is_valid() && el.current != connections.end())
        el.current++->slot(args...);
    }

  private:
    using connections_t = intrusive::list<connection, connection_tag>;

    struct iteration_stack
    {
      const signal *sig;
      typename connections_t::const_iterator current;
      iteration_stack *next;

      iteration_stack(const signal *sig)
          : sig(sig), current(sig->connections.begin()), next(sig->top_iterator)
      {
        sig->top_iterator = this;
      }

      bool is_valid() const
      {
        return sig != nullptr;
      }

      ~iteration_stack()
      {
        if (is_valid())
          sig->top_iterator = next;
        else if (next != nullptr)
          next->sig = nullptr;
      }
    };

    connections_t connections;
    mutable iteration_stack *top_iterator = nullptr;
  };
}  // namespace signals
