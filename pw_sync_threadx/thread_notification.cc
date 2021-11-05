// Copyright 2020 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_sync/thread_notification.h"

#include <mutex>

#include "pw_assert/check.h"
#include "pw_interrupt/context.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "tx_api.h"

namespace pw::sync {
namespace backend {

InterruptSpinLock thread_notification_isl;

}  // namespace backend

void ThreadNotification::acquire() {
  // Enforce the pw::sync::ThreadNotification IRQ contract.
  PW_DCHECK(!interrupt::InInterruptContext());

  // Enforce that only a single thread can block at a time.
  PW_DCHECK(native_type_.blocked_thread == nullptr);

  {
    std::lock_guard lock(backend::thread_notification_isl);
    if (native_type_.notified) {
      native_type_.notified = false;
      return;
    }
    // Not notified yet, register the thread for a one-time notification.
    native_type_.blocked_thread = tx_thread_identify();
  }

  PW_CHECK_UINT_EQ(tx_thread_sleep(TX_WAIT_FOREVER), TX_WAIT_ABORTED);
  {
    // The thread pointer was cleared by the notifier.
    // Note that this may hide another notification, however this is considered
    // a form of notification saturation just like as if this happened before
    // acquire() was invoked.
    std::lock_guard lock(backend::thread_notification_isl);
    native_type_.notified = false;
  }
}

}  // namespace pw::sync
