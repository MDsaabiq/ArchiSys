export type ComponentType = 'client' | 'server' | 'database' | 'loadbalancer' | 'redis' | 'queue'

export interface NodeConfig {
  cpuCores:  number
  procTime:  number
  maxQueue:  number
  instances: number
  // client-only
  requestRate?:   number   // requests per second to inject
  totalRequests?: number   // stop after this many (0 = unlimited)
}

export interface ArchNode {
  id:     number
  type:   ComponentType
  name:   string
  x:      number
  y:      number
  config: NodeConfig
}

export interface ArchEdge {
  id:     number
  fromId: number
  toId:   number
}

export interface SimulationConfig {
  durationSec:        number
  tickSec:            number
  requestRatePerSec:  number
  totalRequests:      number
  seed:               number
}

export interface ArchitecturePayload {
  nodes:      ArchNode[]
  edges:      ArchEdge[]
  simulation: SimulationConfig
}
