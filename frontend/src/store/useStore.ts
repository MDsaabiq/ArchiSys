import { create } from 'zustand'
import type { Node, Edge } from 'reactflow'
import type { ComponentMetrics, SystemMetricsMsg } from '../types/metrics'
import type { NodeConfig, ComponentType } from '../types/architecture'

export interface NodeData {
  label:       string
  componentId: number
  compType:    ComponentType
  config:      NodeConfig
  metrics?:    ComponentMetrics
}

export type SimStatus = 'idle' | 'running' | 'stopped'

export interface SimSettings {
  requestRate:    number   // req/s
  totalRequests:  number   // 0 = unlimited
  seed:           number
}

interface SimState {
  nodes:          Node<NodeData>[]
  edges:          Edge[]
  selectedNodeId: string | null

  simStatus:         SimStatus
  latestMetrics:     SystemMetricsMsg | null
  componentMetrics:  Map<number, ComponentMetrics>
  simSettings:       SimSettings

  nextId: number

  setNodes:         (nodes: Node<NodeData>[]) => void
  setEdges:         (edges: Edge[]) => void
  selectNode:       (id: string | null) => void
  addNode:          (type: ComponentType, x: number, y: number) => void
  removeNode:       (id: string) => void
  updateNodeConfig: (id: string, config: Partial<NodeConfig>) => void
  updateNodeLabel:  (id: string, label: string) => void

  setSimStatus:   (s: SimStatus) => void
  setSimSettings: (s: Partial<SimSettings>) => void
  applyMetrics:   (msg: SystemMetricsMsg) => void
  resetMetrics:   () => void
}

// ── Component metadata ────────────────────────────────────────────────────────
export const COMP_META: Record<ComponentType, { label: string; icon: string; color: string; config: NodeConfig }> = {
  client:       { label: 'Client',       icon: '👤', color: '#0ea5e9', config: { cpuCores:1, procTime:0,  maxQueue:99999, instances:1, requestRate:100, totalRequests:0 } },
  server:       { label: 'App Server',   icon: '🖥',  color: '#3b82f6', config: { cpuCores:4, procTime:50,  maxQueue:200,  instances:2 } },
  database:     { label: 'Database',     icon: '🗄',  color: '#8b5cf6', config: { cpuCores:1, procTime:20,  maxQueue:50,   instances:1 } },
  loadbalancer: { label: 'Load Balancer',icon: '⚖',  color: '#22c55e', config: { cpuCores:4, procTime:2,   maxQueue:500,  instances:4 } },
  redis:        { label: 'Redis Cache',  icon: '⚡',  color: '#f59e0b', config: { cpuCores:2, procTime:1,   maxQueue:1000, instances:4 } },
  queue:        { label: 'Msg Queue',    icon: '📨',  color: '#ef4444', config: { cpuCores:1, procTime:5,   maxQueue:10000,instances:1 } },
}

export const useStore = create<SimState>((set, get) => ({
  nodes:          [],
  edges:          [],
  selectedNodeId: null,
  simStatus:      'idle',
  latestMetrics:  null,
  componentMetrics: new Map(),
  nextId: 1,
  simSettings: { requestRate: 100, totalRequests: 1000, seed: 42 },

  setNodes: (nodes) => set({ nodes }),
  setEdges: (edges) => set({ edges }),
  selectNode: (id) => set({ selectedNodeId: id }),

  addNode: (type, x, y) => {
    const { nodes, nextId } = get()
    const meta = COMP_META[type]
    const newNode: Node<NodeData> = {
      id:   String(nextId),
      type: 'archNode',
      position: { x, y },
      data: {
        label:       meta.label,
        componentId: nextId,
        compType:    type,
        config:      { ...meta.config },
      },
    }
    set({ nodes: [...nodes, newNode], nextId: nextId + 1 })
  },

  removeNode: (id) => {
    const { nodes, edges, selectedNodeId } = get()
    set({
      nodes: nodes.filter(n => n.id !== id),
      edges: edges.filter(e => e.source !== id && e.target !== id),
      selectedNodeId: selectedNodeId === id ? null : selectedNodeId,
    })
  },

  updateNodeConfig: (id, config) => {
    set(s => ({
      nodes: s.nodes.map(n =>
        n.id === id ? { ...n, data: { ...n.data, config: { ...n.data.config, ...config } } } : n
      )
    }))
  },

  updateNodeLabel: (id, label) => {
    set(s => ({
      nodes: s.nodes.map(n =>
        n.id === id ? { ...n, data: { ...n.data, label } } : n
      )
    }))
  },

  setSimStatus: (simStatus) => set({ simStatus }),
  setSimSettings: (s) => set(st => ({ simSettings: { ...st.simSettings, ...s } })),

  applyMetrics: (msg) => {
    const map = new Map<number, ComponentMetrics>()
    for (const cm of msg.components) map.set(cm.id, cm)
    set(s => ({
      latestMetrics:    msg,
      componentMetrics: map,
      nodes: s.nodes.map(n => {
        const cm = map.get(n.data.componentId)
        if (!cm) return n
        return { ...n, data: { ...n.data, metrics: cm } }
      }),
    }))
  },

  resetMetrics: () => set(s => ({
    latestMetrics:    null,
    componentMetrics: new Map(),
    // Also clear metrics off every node so cards go back to idle (—)
    nodes: s.nodes.map(n => ({ ...n, data: { ...n.data, metrics: undefined } })),
  })),
}))
