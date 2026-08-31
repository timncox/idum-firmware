# SuperLooper trigger-owned mutation scheduler

SuperLooper separates permission to mutate from the lifetime of the audible
mutation.

1. Every raw clock edge evaluates Chance and opens or closes an eligibility
   window until the next raw clock edge.
2. A trigger edge inside an open window starts that trigger lane immediately.
3. Mode and L are sampled at the trigger edge. Because one SuperLooper stage
   contains N raw clocks, the lane runs for a complete
   `L * N * measured raw-clock period` beginning at that trigger, even when the
   trigger is off-grid or arrives late in the eligibility window.
4. PARAM remains live under the existing Freeze PARAM option. Ball also lets
   live L reshape its curve without changing the sampled endpoint.
5. Another trigger with the same selected Mode does not renew the lane's
   lifetime. The active mode may still interpret the raw input as part of its
   own behavior.
6. A trigger with a different selected Mode preempts the active lane only when
   the current clock window is open. The new mode receives a fresh sampled L.
7. Changing Mode without a trigger never interrupts an active lane.

The clock lane follows the same launch/preemption rule using CLOCK IN as its
edge. Its L lifetime is counted as `L * N` raw clock edges for transport and
external-sequencer maintenance. Thus `N=2, L=8` produces sixteen raw clocks and
eight external advances, returning an eight-step sequencer to its starting
stage. Loop entry and exit keep their existing clock-synchronous behavior.

The classic runner retains the original clock-owned mutation-window model so
the established firmware behavior remains available for comparison.
