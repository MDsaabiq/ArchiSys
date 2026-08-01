import { useEffect, useRef } from 'react'
import { useStore } from '../store/useStore'
import type { SystemMetricsMsg } from '../types/metrics'

const WS_URL = 'ws://localhost:8765/ws'

// Singleton WS reference shared between useWebSocket and openWebSocketFirst()
let globalWs: WebSocket | null = null

// Opens the WebSocket and returns a promise that resolves when it's connected.
// Used by useSimulation to ensure WS is open BEFORE POST /start.
export function openWebSocketFirst(): Promise<WebSocket> {
  return new Promise((resolve, reject) => {
    if (globalWs && globalWs.readyState === WebSocket.OPEN) {
      resolve(globalWs)
      return
    }
    const ws = new WebSocket(WS_URL)
    globalWs = ws
    ws.onopen = () => resolve(ws)
    ws.onerror = () => reject(new Error('WebSocket connection failed'))
    const timeout = setTimeout(() => reject(new Error('WS connect timeout')), 3000)
    ws.onopen = () => { clearTimeout(timeout); resolve(ws) }
  })
}

export function useWebSocket() {
  const applyMetrics = useStore(s => s.applyMetrics)
  const setSimStatus = useStore(s => s.setSimStatus)
  const simStatus    = useStore(s => s.simStatus)
  const wsRef        = useRef<WebSocket | null>(null)

  useEffect(() => {
    if (simStatus !== 'running') {
      // Clean up on stop
      if (wsRef.current) {
        wsRef.current.close()
        wsRef.current = null
        globalWs = null
      }
      return
    }

    // Re-use the already-open socket from openWebSocketFirst()
    const ws = globalWs ?? new WebSocket(WS_URL)
    globalWs = ws
    wsRef.current = ws

    ws.onmessage = (ev) => {
      try {
        const msg = JSON.parse(ev.data) as SystemMetricsMsg
        if (msg.type === 'metrics') applyMetrics(msg)
      } catch {}
    }
    ws.onerror = () => console.warn('[WS] error')
    ws.onclose = () => {
      console.log('[WS] disconnected')
      globalWs = null
      wsRef.current = null
      setSimStatus('stopped')
      // Clear node metrics so cards reset to idle (—) when sim ends
      useStore.getState().resetMetrics()
    }

    return () => {
      // Don't close here — let stop() handle it so we don't cut the stream
    }
  }, [simStatus])

  return wsRef
}
