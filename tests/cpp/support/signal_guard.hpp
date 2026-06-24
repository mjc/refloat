#pragma once

#include <setjmp.h>
#include <signal.h>

using SignalHandler = void (*)(int);
SignalHandler signal(int signal_number, SignalHandler handler);

template <typename Fn>
bool run_without_signal(int signal_number, sigjmp_buf *env, void (*handler)(int), Fn &&fn) {
    SignalHandler previous_signal = signal(signal_number, handler);
    bool ok = true;
    if (sigsetjmp(*env, 1) == 0) {
        fn();
    } else {
        ok = false;
    }
    signal(signal_number, previous_signal);
    return ok;
}
