export interface ComponentMetrics {
  id:                 number
  name:               string
  type:               string
  cpuUsagePct:        number
  queueDepth:         number
  maxQueue:           number
  requestsReceived:   number
  requestsCompleted:  number
  requestsDropped:    number
  avgProcessingMs:    number
  avgQueueWaitMs:     number
  throughputPerSec:   number
}

export interface SystemMetricsMsg {
  type:             'metrics'
  simTimeSec:       number
  totalRequests:    number
  completed:        number
  failed:           number
  dropped:          number
  inFlight:         number
  avgLatencyMs:     number
  p99LatencyMs:     number
  throughputPerSec: number
  components:       ComponentMetrics[]
}
