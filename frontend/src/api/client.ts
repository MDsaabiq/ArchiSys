import type { ArchitecturePayload } from '../types/architecture'

const BASE = 'http://localhost:8765'

export async function startSimulation(payload: ArchitecturePayload): Promise<void> {
  await fetch(`${BASE}/start`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify(payload),
  })
}

export async function stopSimulation(): Promise<void> {
  await fetch(`${BASE}/stop`, { method: 'POST' })
}

export async function healthCheck(): Promise<boolean> {
  try {
    const res = await fetch(`${BASE}/health`)
    return res.ok
  } catch {
    return false
  }
}
