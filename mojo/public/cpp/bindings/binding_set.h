// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

template <typename BindingType>
struct BindingSetTraits;

template <typename Interface>
struct BindingSetTraits<Binding<Interface>> {
  using ProxyType = InterfacePtr<Interface>;
  using RequestType = InterfaceRequest<Interface>;

  static RequestType MakeRequest(ProxyType* proxy) {
    return mojo::MakeRequest(proxy);
  }
};

using BindingId = size_t;

template <typename ContextType>
struct BindingSetContextTraits {
  using Type = ContextType;

  static constexpr bool SupportsContext() { return true; }
};

template <>
struct BindingSetContextTraits<void> {
  // NOTE: This choice of Type only matters insofar as it affects the size of
  // the |context_| field of a BindingSetBase::Entry with void context. The
  // context value is never used in this case.
  using Type = bool;

  static constexpr bool SupportsContext() { return false; }
};

// Generic definition used for BindingSet and AssociatedBindingSet to own a
// collection of bindings which point to the same implementation.
//
// If |ContextType| is non-void, then every added binding must include a context
// value of that type, and |dispatch_context()| will return that value during
// the extent of any message dispatch targeting that specific binding.
template <typename Interface, typename BindingType, typename ContextType>
class BindingSetBase {
 public:
  using ContextTraits = BindingSetContextTraits<ContextType>;
  using Context = typename ContextTraits::Type;
  using PreDispatchCallback = base::Callback<void(const Context&)>;
  using Traits = BindingSetTraits<BindingType>;
  using ProxyType = typename Traits::ProxyType;
  using RequestType = typename Traits::RequestType;

  BindingSetBase() {}

  void set_connection_error_handler(const base::Closure& error_handler) {
    error_handler_ = error_handler;
    error_with_reason_handler_.Reset();
  }

  void set_connection_error_with_reason_handler(
      const ConnectionErrorWithReasonCallback& error_handler) {
    error_with_reason_handler_ = error_handler;
    error_handler_.Reset();
  }

  // Sets a callback to be invoked immediately before dispatching any message or
  // error received by any of the bindings in the set. This may only be used
  // with a non-void |ContextType|.
  void set_pre_dispatch_handler(const PreDispatchCallback& handler) {
    static_assert(ContextTraits::SupportsContext(),
                  "Pre-dispatch handler usage requires non-void context type.");
    pre_dispatch_handler_ = handler;
  }

  // Adds a new binding to the set which binds |request| to |impl| with no
  // additional context.
  BindingId AddBinding(Interface* impl, RequestType request) {
    static_assert(!ContextTraits::SupportsContext(),
                  "Context value required for non-void context type.");
    return AddBindingImpl(impl, std::move(request), false);
  }

  // Adds a new binding associated with |context|.
  BindingId AddBinding(Interface* impl, RequestType request, Context context) {
    static_assert(ContextTraits::SupportsContext(),
                  "Context value unsupported for void context type.");
    return AddBindingImpl(impl, std::move(request), std::move(context));
  }

  // Removes a binding from the set. Note that this is safe to call even if the
  // binding corresponding to |id| has already been removed.
  //
  // Returns |true| if the binding was removed and |false| if it didn't exist.
  bool RemoveBinding(BindingId id) {
    auto it = bindings_.find(id);
    if (it == bindings_.end())
      return false;
    bindings_.erase(it);
    return true;
  }

  // Returns a proxy bound to one end of a pipe whose other end is bound to
  // |this|. If |id_storage| is not null, |*id_storage| will be set to the ID
  // of the added binding.
  ProxyType CreateInterfacePtrAndBind(Interface* impl,
                                      BindingId* id_storage = nullptr) {
    ProxyType proxy;
    BindingId id = AddBinding(impl, Traits::MakeRequest(&proxy));
    if (id_storage)
      *id_storage = id;
    return proxy;
  }

  void CloseAllBindings() { bindings_.clear(); }

  bool empty() const { return bindings_.empty(); }

  // Implementations may call this when processing a dispatched message or
  // error. During the extent of message or error dispatch, this will return the
  // context associated with the specific binding which received the message or
  // error. Use AddBinding() to associated a context with a specific binding.
  const Context& dispatch_context() const {
    static_assert(ContextTraits::SupportsContext(),
                  "dispatch_context() requires non-void context type.");
    DCHECK(dispatch_context_);
    return *dispatch_context_;
  }

  void FlushForTesting() {
    for (auto& binding : bindings_)
      binding.second->FlushForTesting();
  }

 private:
  friend class Entry;

  class Entry {
   public:
    Entry(Interface* impl,
          RequestType request,
          BindingSetBase* binding_set,
          BindingId binding_id,
          Context context)
        : binding_(impl, std::move(request)),
          binding_set_(binding_set),
          binding_id_(binding_id),
          context_(std::move(context)) {
      if (ContextTraits::SupportsContext())
        binding_.AddFilter(base::MakeUnique<DispatchFilter>(this));
      binding_.set_connection_error_with_reason_handler(
          base::Bind(&Entry::OnConnectionError, base::Unretained(this)));
    }

    void FlushForTesting() { binding_.FlushForTesting(); }

   private:
    class DispatchFilter : public MessageReceiver {
     public:
      explicit DispatchFilter(Entry* entry) : entry_(entry) {}
      ~DispatchFilter() override {}

     private:
      // MessageReceiver:
      bool Accept(Message* message) override {
        entry_->WillDispatch();
        return true;
      }

      Entry* entry_;

      DISALLOW_COPY_AND_ASSIGN(DispatchFilter);
    };

    void WillDispatch() {
      DCHECK(ContextTraits::SupportsContext());
      binding_set_->SetDispatchContext(&context_);
    }

    void OnConnectionError(uint32_t custom_reason,
                           const std::string& description) {
      if (ContextTraits::SupportsContext())
        WillDispatch();
      binding_set_->OnConnectionError(binding_id_, custom_reason, description);
    }

    BindingType binding_;
    BindingSetBase* const binding_set_;
    const BindingId binding_id_;
    Context const context_;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  void SetDispatchContext(const Context* context) {
    DCHECK(ContextTraits::SupportsContext());
    dispatch_context_ = context;
    if (!pre_dispatch_handler_.is_null())
      pre_dispatch_handler_.Run(*context);
  }

  BindingId AddBindingImpl(Interface* impl,
                           RequestType request,
                           Context context) {
    BindingId id = next_binding_id_++;
    DCHECK_GE(next_binding_id_, 0u);
    auto entry = base::MakeUnique<Entry>(
        impl, std::move(request), this, id, std::move(context));
    bindings_.insert(std::make_pair(id, std::move(entry)));
    return id;
  }

  void OnConnectionError(BindingId id,
                         uint32_t custom_reason,
                         const std::string& description) {
    auto it = bindings_.find(id);
    DCHECK(it != bindings_.end());

    // We keep the Entry alive throughout error dispatch.
    std::unique_ptr<Entry> entry = std::move(it->second);
    bindings_.erase(it);

    if (!error_handler_.is_null())
      error_handler_.Run();
    else if (!error_with_reason_handler_.is_null())
      error_with_reason_handler_.Run(custom_reason, description);
  }

  base::Closure error_handler_;
  ConnectionErrorWithReasonCallback error_with_reason_handler_;
  PreDispatchCallback pre_dispatch_handler_;
  BindingId next_binding_id_ = 0;
  std::map<BindingId, std::unique_ptr<Entry>> bindings_;
  const Context* dispatch_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BindingSetBase);
};

template <typename Interface, typename ContextType = void>
using BindingSet = BindingSetBase<Interface, Binding<Interface>, ContextType>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
