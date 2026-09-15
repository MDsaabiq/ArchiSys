# ArchiSys Simulation Engine: Concepts, Algorithms & Metrics

## Simulation Core Concepts

The ArchiSys simulation engine is a discrete-time / discrete-tick event simulator. Rather than using wall-clock time, the engine advances a synthetic simulation clock `simNow` by a configurable time increment $\Delta t$ (default: `0.01s` or `10ms` per tick).

### The 5-Phase Simulation Tick

Every iteration of the simulation loop executes five discrete phases in strict chronological order:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. generateRequests()                                       │
│    - Compute Poisson traffic arrival: E = rate * dt         │
│    - Inject new requests into Client / Entry-Point node     │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. updateComponents()                                       │
│    - Complete slots where finishAt <= simNow                │
│    - Promote waiting requests from FIFO queue to slots      │
│    - Forward finished requests to downstream components     │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. streamTelemetry() (onTick Hook)                          │
│    - Snapshot ComponentMetrics and SystemMetrics            │
│    - Deliver frame to Python / WebSocket broadcaster        │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. advanceSimulationTime()                                  │
│    - Advance simulation clock: simNow += dt                 │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. checkTermination()                                       │
│    - Check if durationSec elapsed or totalRequests drained  │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Modeling & Concurrency

### 1. Queues and Instance Concurrency
Each component maintains:
- **`waitQueue_` (FIFO Buffer)**: Stores requests waiting for an available worker instance.
  - If `waitQueue_.size() >= maxQueue`, incoming requests are dropped (`RequestStatus::Dropped`, `requestsDropped++`).
- **`activeSlots_` (Worker Instances)**: Contains up to `instances` concurrent processing slots.
  - When a slot is free and the queue is non-empty:
    1. A request is dequeued: `req = waitQueue_.pop()`.
    2. Queue wait duration is recorded: $\text{waitMs} = (\text{simNow} - \text{arrivalTime}) \times 1000$.
    3. Processing finish time is scheduled: $\text{finishAt} = \text{simNow} + \frac{\text{procDurationMs}}{1000}$.

### 2. CPU Contention & Utilization
- **CPU Utilization (%)**:
  $$\text{CPU Usage} = \min\left(100.0, \frac{\text{busySlots}}{\text{instances}} \times 100.0\right)$$
- **CPU Core Contention**:
  If a node is configured with `cpuCores` and concurrent active worker slots exceed available cores ($\text{busySlots} > \text{cpuCores}$), processing time is scaled realistically to reflect CPU contention:
  $$\text{effectiveDuration} = \text{baseDuration} \times \min\left(2.5, \frac{\text{busySlots}}{\text{cpuCores}}\right)$$

---

## Component-Specific Behaviors

| Component | Role | Latency & Routing Behavior |
| :--- | :--- | :--- |
| **Client** | Traffic Generator | $0\text{ ms}$ latency pass-through. Generates requests according to Poisson distribution based on `requestRate` and `totalRequests`. |
| **Server** | Application Compute | Multi-instance worker pool. Processes requests for `procTime` ms concurrently up to `instances` lanes. |
| **Database** | Persistence Storage | Processes read queries in `procTime` ms; write transactions (20% of traffic) take $2 \times \text{procTime}$ ms. Strict connection pool limit (`maxQueue`). |
| **LoadBalancer** | Traffic Routing | Routing overhead $\sim 1\text{-}2\text{ ms}$. Dispatches requests using `LeastConnections` (chooses downstream node with lowest queue + busy slots), `RoundRobin`, or `Random`. |
| **RedisCache** | In-Memory Cache | Cache hits (80%) complete in `procTime` ($1\text{ ms}$) without querying downstream DB. Cache misses incur $3\times$ lookup overhead and forward downstream to the Database. |
| **MessageQueue** | Asynchronous Broker | Large capacity buffer (`maxQueue = 10000`). Buffers burst traffic and dispatches to consumers according to drain rate. |

---

## Metric Formulas & Mathematical Definitions

1. **Queue Wait Latency**:
   $$\text{Queue Wait Time} = (\text{processingStart} - \text{arrivalTime}) \times 1000.0\text{ ms}$$
2. **Component Processing Duration**:
   $$\text{Processing Duration} = (\text{departureTime} - \text{processingStart}) \times 1000.0\text{ ms}$$
3. **End-to-End Round-Trip Latency**:
   For any completed request across all hops:
   $$\text{Total Latency} = (\text{completedAt} - \text{createdAt}) \times 1000.0\text{ ms}$$
4. **Throughput (Requests/sec)**:
   $$\text{Throughput} = \frac{\text{Completed Requests in Window}}{\Delta t_{\text{window}}}$$
5. **99th Percentile Latency (P99)**:
   Calculated over the sorted distribution of completed request latencies $L$:
   $$\text{Index}_{P99} = \lfloor 0.99 \times (|L| - 1) \rfloor$$
   $$P99 = L[\text{Index}_{P99}]$$

---

## Bugs Identified & Corrected

1. **Redis Cache Latency Double Counting**:
   - *Old Behavior*: `RedisCache::processRequest` manually added latency to `req->totalLatencyMs`, and `Component::tick()` added `processingMs` again.
   - *New Behavior*: Latency is recorded once upon slot completion based on exact start and finish timestamps.
2. **Database Processing State Mutation**:
   - *Old Behavior*: `Database::processRequest` modified `this->processingMs` globally, overriding user-configured parameters for subsequent requests.
   - *New Behavior*: `computeProcessingDurationMs` computes per-request query vs write duration without mutating member fields.
3. **Missing P99 Metric**:
   - *Old Behavior*: `p99LatencyMs` was declared in `SystemMetrics` but never calculated (always 0.0).
   - *New Behavior*: Accurately tracks all completed request round-trips and performs percentile calculation via `std::nth_element`.
