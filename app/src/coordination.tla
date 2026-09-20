-------------------------- MODULE Coordination --------------------------
EXTENDS Integers, FiniteSets, Sequences

\* One production cycle: receiver A, unrelated task B, producer C.
\* Mode = "CV": ordinary Mesa condition variable + PI mutex + queue.
\* Mode = "RK0": named asynchronous messages, optionally with a ceiling.
\* Time unit = 1 ms of CPU execution. Kernel/application bookkeeping is
\* instantaneous. C's preparation is outside the monitor's mutex.
CONSTANTS Mode, Ceiling, CWork, BWork, BPhase, Deadline, ExpectedLatency

Tasks == {"A", "B", "C"}
Buffers == {1, 2}
None == "None"
Base(t) == CASE t = "A" -> 2 [] t = "B" -> 5 [] t = "C" -> 8
Min(S) == CHOOSE n \in S : \A m \in S : n <= m

ASSUME /\ Mode \in {"CV", "RK0"}
       /\ Ceiling \in BOOLEAN
       /\ CWork \in Nat \ {0}
       /\ BWork \in Nat \ {0}
       /\ BPhase \in Nat
       /\ Deadline \in Nat
       /\ ExpectedLatency \in Nat

VARIABLE s
vars == <<s>>

Init == s = [
    now       |-> 0,
    pc        |-> [t \in Tasks |->
                    CASE t = "A" -> IF Mode = "CV" THEN "Lock" ELSE "Receive"
                      [] t = "B" -> "Sleep"
                      [] t = "C" -> IF Mode = "CV" THEN "Begin" ELSE "Alloc"],
    left      |-> [t \in Tasks |->
                    CASE t = "A" -> 0 [] t = "B" -> BWork [] t = "C" -> CWork],
    mutex     |-> None,
    queue     |-> <<>>,
    owner     |-> [b \in Buffers |-> None],
    slot      |-> 0,
    prepared  |-> FALSE,
    started   |-> -1,
    finished  |-> -1,
    delivered |-> -1]

MutexWaiters == {t \in Tasks : s.pc[t] = "MutexWait"}
Owns(t) == \E b \in Buffers : s.owner[b] = t

\* Mutex donation includes MUTEX waiters only, never condition waiters.
\* One mutex: there are no nested/transitive mutex chains in this model.
Effective(t) ==
    IF Mode = "CV" /\ s.mutex = t
    THEN Min({Base(t)} \cup {Base(w) : w \in MutexWaiters})
    ELSE IF Mode = "RK0" /\ Ceiling /\ Owns(t)
         THEN Min({Base(t), 2})
         ELSE Base(t)

BlockedPC == {"Sleep", "CondWait", "MutexWait", "MsgWait", "Done", "Skipped"}
Ready == {t \in Tasks : s.pc[t] \notin BlockedPC}
Selected == IF Ready = {} THEN None
            ELSE CHOOSE t \in Ready : \A u \in Ready : Effective(t) <= Effective(u)

\* A release cannot be postponed by a high-priority task: it becomes due
\* at the clock boundary, before any further task step can execute.
ReleaseDue == s.pc["B"] = "Sleep" /\ s.now >= BPhase
Running(t) == ~ReleaseDue /\ Selected = t

ReleaseB == /\ ReleaseDue
            /\ s' = [s EXCEPT !.pc["B"] = "Work"]

Compute(t) ==
    /\ t \in {"B", "C"}
    /\ Running(t)
    /\ s.pc[t] = "Work"
    /\ s.left[t] > 0
    /\ s' = [s EXCEPT
        !.now = @ + 1,
        !.left[t] = @ - 1,
        !.pc[t] = IF s.left[t] = 1
                  THEN IF t = "B" THEN "Done"
                       ELSE IF Mode = "CV" THEN "Lock" ELSE "Send"
                  ELSE @,
        !.prepared = IF t = "C" /\ s.left[t] = 1 THEN TRUE ELSE @,
        !.finished = IF t = "C" /\ s.left[t] = 1 THEN s.now + 1 ELSE @]

\* ---------------- Ordinary condition variable + queue ----------------

BeginCV == /\ Mode = "CV" /\ Running("C") /\ s.pc["C"] = "Begin"
           /\ s' = [s EXCEPT !.pc["C"] = "Work", !.started = s.now, !.slot = 1]

Lock(t) ==
    /\ Mode = "CV" /\ t \in {"A", "C"}
    /\ Running(t) /\ s.pc[t] = "Lock"
    /\ IF s.mutex = None
       THEN s' = [s EXCEPT !.mutex = t,
                  !.pc[t] = IF t = "A" THEN "Check" ELSE "Put"]
       ELSE s' = [s EXCEPT !.pc[t] = "MutexWait"]

\* Unlock makes any mutex waiter ready to attempt acquisition again.
ReleasePC(t, next) ==
    [u \in Tasks |-> IF u = t THEN next
                     ELSE IF s.pc[u] = "MutexWait" THEN "Lock" ELSE s.pc[u]]

WaitCV ==
    /\ Mode = "CV" /\ Running("A") /\ s.pc["A"] = "Check"
    /\ s.mutex = "A" /\ Len(s.queue) = 0
    \* One atomic step: join the condition wait set and release the mutex.
    /\ s' = [s EXCEPT !.mutex = None, !.pc = ReleasePC("A", "CondWait")]

TakeCV ==
    /\ Mode = "CV" /\ Running("A") /\ s.pc["A"] = "Check"
    /\ s.mutex = "A" /\ Len(s.queue) > 0
    /\ s' = [s EXCEPT !.queue = Tail(@), !.delivered = s.now,
                      !.pc["A"] = "Unlock"]

PutCV ==
    /\ Mode = "CV" /\ Running("C") /\ s.pc["C"] = "Put"
    /\ s.mutex = "C" /\ Len(s.queue) < Cardinality(Buffers)
    /\ s' = [s EXCEPT !.queue = Append(@, s.slot), !.pc["C"] = "Signal"]

SignalCV ==
    /\ Mode = "CV" /\ Running("C") /\ s.pc["C"] = "Signal"
    /\ s.mutex = "C"
    /\ s' = [s EXCEPT !.pc["C"] = "Unlock",
                      !.pc["A"] = IF @ = "CondWait" THEN "Lock" ELSE @]
    \* Signal grants no mutex ownership. A must acquire and recheck.

Unlock(t) ==
    /\ Mode = "CV" /\ t \in {"A", "C"}
    /\ Running(t) /\ s.pc[t] = "Unlock" /\ s.mutex = t
    /\ s' = [s EXCEPT !.mutex = None, !.pc = ReleasePC(t, "Done")]

CVStep == BeginCV \/ WaitCV \/ TakeCV \/ PutCV \/ SignalCV
          \/ (\E t \in {"A", "C"} : Lock(t) \/ Unlock(t))

\* ---------------- RK0 asynchronous named messages ---------------------

FreeBuffers == {b \in Buffers : s.owner[b] = None}

AllocRK(b) ==
    /\ Mode = "RK0" /\ Running("C") /\ s.pc["C"] = "Alloc"
    /\ b \in FreeBuffers
    /\ s' = [s EXCEPT !.owner[b] = "C", !.slot = b,
                      !.pc["C"] = "Work", !.started = s.now]
    \* Successful allocation establishes ceiling ownership before work.

AllocEmptyRK ==
    /\ Mode = "RK0" /\ Running("C") /\ s.pc["C"] = "Alloc"
    /\ FreeBuffers = {}
    /\ s' = [s EXCEPT !.pc["C"] = "Skipped"]
    \* RK_NO_WAIT does not block. Unreachable in this initial one-shot model.

WaitRK ==
    /\ Mode = "RK0" /\ Running("A") /\ s.pc["A"] = "Receive"
    /\ Len(s.queue) = 0
    /\ s' = [s EXCEPT !.pc["A"] = "MsgWait"]

SendRK ==
    /\ Mode = "RK0" /\ Running("C") /\ s.pc["C"] = "Send"
    /\ s.owner[s.slot] = "C"
    /\ s' = [s EXCEPT !.owner[s.slot] = "A",
                      !.queue = Append(@, s.slot),
                      !.pc["C"] = "Done",
                      !.pc["A"] = IF @ = "MsgWait" THEN "Receive" ELSE @]
    \* Ownership (and its ceiling contribution) transfers at SEND time.
    \* Direct delivery to a waiter is abstracted by an instantaneous dequeue.

TakeRK ==
    /\ Mode = "RK0" /\ Running("A") /\ s.pc["A"] = "Receive"
    /\ Len(s.queue) > 0
    /\ s' = [s EXCEPT !.queue = Tail(@), !.delivered = s.now, !.pc["A"] = "Free"]

FreeRK ==
    /\ Mode = "RK0" /\ Running("A") /\ s.pc["A"] = "Free"
    /\ s.owner[s.slot] = "A"
    /\ s' = [s EXCEPT !.owner[s.slot] = None, !.pc["A"] = "Done"]

RKStep == (\E b \in Buffers : AllocRK(b)) \/ AllocEmptyRK
          \/ WaitRK \/ SendRK \/ TakeRK \/ FreeRK

Idle == /\ Ready = {} /\ s.pc["B"] = "Sleep" /\ s.now < BPhase
        /\ s' = [s EXCEPT !.now = BPhase]

Finished == \A t \in Tasks : s.pc[t] = "Done"
Next == ReleaseB \/ (\E t \in {"B", "C"} : Compute(t))
        \/ CVStep \/ RKStep \/ Idle \/ (Finished /\ UNCHANGED s)

\* Weak fairness excludes infinite stuttering while progress is enabled.
Spec == Init /\ [][Next]_vars /\ WF_vars(Next)

\* ------------------------- Properties --------------------------------

PCs == {"Lock", "Check", "Sleep", "Begin", "Alloc", "Work", "Send",
        "Receive", "MutexWait", "CondWait", "MsgWait", "Put", "Signal",
        "Unlock", "Free", "Done", "Skipped"}

TypeOK ==
    /\ s.now \in Nat
    /\ s.pc \in [Tasks -> PCs]
    /\ s.left \in [Tasks -> Nat]
    /\ s.mutex \in {None, "A", "C"}
    /\ s.queue \in Seq(Buffers)
    /\ s.owner \in [Buffers -> {None, "A", "C"}]
    /\ s.slot \in {0} \cup Buffers
    /\ s.prepared \in BOOLEAN
    /\ s.started \in {-1} \cup Nat
    /\ s.finished \in {-1} \cup Nat
    /\ s.delivered \in {-1} \cup Nat

QueueBound == Len(s.queue) <= Cardinality(Buffers)
PreparedBeforePublish == (Len(s.queue) > 0 \/ s.delivered >= 0) => s.prepared
DeliveryAfterPreparation == s.delivered >= 0 =>
                           (s.started >= 0 /\ s.finished >= s.started
                            /\ s.delivered >= s.finished)

MutexDiscipline == Mode = "CV" =>
    /\ (s.pc["A"] \in {"Check", "Unlock"} => s.mutex = "A")
    /\ (s.pc["C"] \in {"Put", "Signal", "Unlock"} => s.mutex = "C")

MessageOwnership == Mode = "RK0" =>
    /\ (s.pc["C"] \in {"Work", "Send"} => s.owner[s.slot] = "C")
    /\ (\A i \in 1..Len(s.queue) : s.owner[s.queue[i]] = "A")
    /\ (s.pc["A"] = "Free" => s.owner[s.slot] = "A")

\* No equal effective priorities among ready tasks arise in this experiment.
\* Thus it makes no assumptions about RK0's tie-breaking policy.
NoReadyTies == \A t, u \in Ready : Effective(t) = Effective(u) => t = u

NoMediumInterference ==
    (Mode = "RK0" /\ Ceiling /\ s.pc["C"] = "Work") => Selected # "B"

ExpectedDelivery == s.delivered = -1 \/ s.delivered - s.started = ExpectedLatency
DeliveryWithinDeadline == s.delivered = -1 \/ s.delivered - s.started <= Deadline
EventuallyDelivered == <>(s.delivered >= 0)
EventuallyFinished == <>Finished
=============================================================================
