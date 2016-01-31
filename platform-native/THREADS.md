Threading Model
===============

## Introduction

Avalanche uses a relatively rich threading model to support diverse use-cases.
It is also designed so that exceptions can sanely be handled across threads of
execution and to provide simple but safe ways to terminate tasks.

This section describes how different classes of threads of execution are
modelled; other concurrency issues, such as a memory model, are discussed in
other sections.

The threading model has the following components:

- Process. Corresponds to a process as defined by the underlying operating
  system.

- Sub-process. Usually one per process, but other possibilities exist.
  Sub-processes cannot share information easily. A sub-process exists within
  exactly one process.

- Thread pool. A group of threads which execute fibres. Exists in exactly one
  sub-process. Has one or more threads, and any number of fibres. It is common
  for sub-processes to have only one thread pool. Unlike sub-processes, there
  is no isolation.

- Thread. Corresponds to a thread as defined by the underlying operating
  system. Exists in exactly one thread pool.

- Fibre. A single execution stack. A fibre belongs to exactly one thread pool.
  A fibre owns any number of strands, but most commonly exactly 1. Fibres have
  weak affinity to the last thread to have executed them.

- Strand. A thread of execution as visible to user code. Each strand belongs to
  exactly one fibre. This is the "thread of execution" primitive exposed to
  user code.

## Threads and Thread Pools

Threads in Avalanche are a means, not an end. That is, user code does not say
"create a thread and do X on it". Rather, threads simply provide a vehicle for
fibres to be executed, potentially concurrently.

The number of threads in a thread pool determines the maximum concurrency of
that thread pool. Whenever a fibre within the same pool becomes runnable, the
last thread to have executed it picks it up, or any other idle thread in the
thread pool if that one is unavailable. When the fibre a thread is executing
ceases to be runnable, the thread looks for other work, possibly causing other
fibres to become runnable.

Initially, a sub-process has one thread pool with one thread, which is suitable
for the majority of programs. New thread pools can be created dynamically, and
thread pools can be resized at any time, though destroying a thread cannot take
effect until its currently-executing fibre becomes non-runnable. Destroying a
thread-pool requires terminating all strands within it.

## Fibres and Strands

A fibre is simply a container of a single execution stack within a thread pool.
A fibre owns any number of strands, but at most one strand is _active_ at a
time. The active strand is the one whose execution stack is contained within
the fibre's execution stack. Other strands in the fibre are _inactive_ and have
their execution stack moved to another location.

A fibre with more than one strand is termed _overloaded_.

Execution stacks are not relocatable. That is, there is no way to move an
execution stack to another location and expect to be able to continue executing
it there. Therefore, a strand is necessarilly pinned to one fibre. If more than
one strand belongs to the same fibre, at most one can execute at a time, no
matter how many threads might be free.

Switching the active strand in a fibre means copying the current strand's stack
elsewhere, then copying the new one's into the fibre's execution stack. This
makes context switches between strands potentially expensive. The benefit is
that an inactive fibre only occupies the stack space it is actually using,
whereas a fibre occupies the maximum stack space that has ever been used.

In many applications, every fibre contains exactly one strand. Some
applications can benefit from overloading fibres. For example, if the
application generally has an extremely large number of strands waiting with a
small call stack, but when one occasionally wakes up creates a deep call stack,
and potential activation latency can be tolerated, mapping many strands onto a
smaller number of fibres would be advantageous.

## State Machine

A thread has three states:

- Busy. The thread is currently executing a fibre. When the fibre ceases to be
  runnable, the thread transitions to the idle state.

- Idle. The thread is currently doing nothing. While in this state, the thread
  is waiting on a condition to either transition to the polling state or to be
  assigned a fibre by the poller to transition into the busy state.

- Polling. The thread is waiting on conditions or system calls for a fibre in
  the thread pool which is not currently executing to become runnable.

A thread scheduled for termination does so in the idle or polling states.

A fibre is runnable iff it has at least one strand which is runnable.

A strand is either "runnable" or "blocked". Additional information is recorded
about _why_ a strand is blocked (eg, sleeping, timed wait, waiting for
condition),but these are not distinct states.

## Strand Lifecycle

Strands have strict limits as to how they may be used, to ensure that complete
termination of a task is possible and to make it impossible for a strand to die
silently while leaving the rest of the process running but paralysed (cf
threads in Java).

Strands are organised into a tree, with each child strand being anchored to a
particular call frame of its parent. Each sub-program has exactly one root
(parentless) strand.

A call frame cannot exit until all fibres rooted at that frame have terminated.
While this makes constructs like daemon threads initially more awkward, it also
makes them more explicit. Furthermore, it ensures that any exceptions escaping
a strand have someplace to propagate to (except for the root strand, beyond
which any thrown exception terminates the process as one would expect). If a
call frame _does_ attempt to exit (be it by returning or throwing an exception)
before all child strands are given interrupted uncancellably with the
`parent-strand-exit` exception type, and the call frame blocks uninterruptably
until all children have terminated.

A strand may be interrupted at any time. An interruption causes a particular
exception to be thrown in the strand at any point where it enters an
interruptable blocked state. An interruption may be cancellable, which allows
the strand that it has acknowledged the interruption (with whatever thing it
was coordinating with) and wishes to resume normal operation.

If an exception of any kind escapes a strand, the parent will be issued a
cancellable interruption with the escaped exception. High-level constructs
abstract this by handling the interruption and letting the exception propagate
normally.

If there was already an interruption when a strand is interrupted, the old
exception is appended to the `previous-interruption` field of the new
exception and the new exception replaces the exception thrown to indicate
interruption.

Note that this setup provides a nice set of assurances:

- An uncaught exception by default terminates the whole program. Something
  _must_ be prepared to handle the exception. However, handling cross-strand
  exceptions is also reasonably straight-forward. This eliminates the
  silent-thread-death issue from systems like Java while also providing a way
  to recover, unlike C++ which terminates the whole process.

- Issuing an uncancellable interruption to a strand and waiting for it to
  terminate guarantees that all work it has spawned has either terminated or
  was deliberately handed off to a different subsystem.

- Accidentally cancelling an interrupt is difficult, and impossible for
  uncancellable interrupts. This has an obvious contrast to Java, where it only
  takes one person doing
  ```
    try {
      Thread.sleep(42);
    } catch (InterruptedException e) {
      // ???
    }
  ```
  or wrapping the interruption in another exception something else
  (unknowingly) "handles" to ruin the whole concept.

- Accidentally returning from a function without waiting for spawned strands
  results in an exception.

- Interrupting a strand not prepared for being interrupted results in an
  exception propagating out.

There are, of course, still ways of continuing a thread after interruption,
simply by not doing any interruptable things. Infinite loops, long stretches of
numeric computation, or calling into native code are easy examples of
accomplishing this. However, there is no general way to cause termination more
expediently without compromising at least two of saftey, performance, and
guarantees like "finally" blocks. This is not expected to be a serious issue in
most applications.

Memory Model
============

The native platform's memory model is exactly that of LLVM. By default, all
operations are assumed to be done in the absence of concurrency, and violating
this assumption has undefined effect. Specific atomic operations allow
concurrent interaction, and allow specifying memory ordering.

