import { useStore } from '../store/useStore'
import { startSimulation, stopSimulation } from '../api/client'
import { openWebSocketFirst } from './useWebSocket'
import type { ArchNode, ArchEdge, ArchitecturePayload } from '../types/architecture'
import type { ComponentType } from '../types/architecture'

export function useSimulation() {
  const { nodes, edges, setSimStatus, resetMetrics, simSettings } = useStore()

  const start = async () => {
    if (nodes.length === 0) {
      alert('Add at least one component to the canvas first.')
      return
    }

    // If user has a Client node, its requestRate overrides the global setting
    const clientNode = nodes.find(n => n.data.compType === 'client')
    const rate         = clientNode?.data.config.requestRate  ?? simSettings.requestRate
    const totalReqs    = clientNode?.data.config.totalRequests ?? simSettings.totalRequests
    // durationSec: if totalRequests > 0 use generous upper bound; otherwise 9999 (run until stopped)
    const durationSec  = totalReqs > 0 ? Math.max(600, (totalReqs / rate) * 4) : 9999

    const archNodes: ArchNode[] = nodes.map(n => ({
      id:     n.data.componentId,
      type:   n.data.compType as ComponentType,
      name:   n.data.label,
      x:      n.position.x,
      y:      n.position.y,
      config: n.data.config,
    }))

    const archEdges: ArchEdge[] = edges.map((e, i) => {
      const fromNode = nodes.find(n => n.id === e.source)
      const toNode   = nodes.find(n => n.id === e.target)
      return {
        id:     i + 1,
        fromId: fromNode?.data.componentId ?? 0,
        toId:   toNode?.data.componentId   ?? 0,
      }
    })

    const payload: ArchitecturePayload = {
      nodes: archNodes,
      edges: archEdges,
      simulation: {
        durationSec,
        tickSec:           0.01,
        requestRatePerSec: rate,
        totalRequests:     totalReqs,
        seed:              simSettings.seed,
      },
    }

    try {
      // Open WebSocket FIRST — engine may finish before browser opens WS otherwise
      await openWebSocketFirst()
      // Now start the simulation — WebSocket is already listening
      await startSimulation(payload)
      setSimStatus('running')
    } catch (err) {
      console.error('Failed to start simulation:', err)
      alert('Could not reach the FastAPI backend at http://localhost:8000.\nMake sure the FastAPI server is running.')
    }
  }

  const stop = async () => {
    try { await stopSimulation() } catch {}
    setSimStatus('stopped')
    resetMetrics()
  }

  return { start, stop }
}
