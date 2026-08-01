import React from 'react'
import type { NodeData } from '../store/useStore'
import { COMP_META } from '../store/useStore'
import { useStore } from '../store/useStore'

// SVG icons matching the sidebar (reused inline)
const NODE_ICONS: Record<string, React.ReactNode> = {
  client: (
    <svg width="16" height="16" viewBox="0 0 28 28" fill="none">
      <circle cx="14" cy="10" r="4.5" fill="none" stroke="currentColor" strokeWidth="2"/>
      <path d="M5 24c0-4.97 4.03-9 9-9s9 4.03 9 9" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round"/>
    </svg>
  ),
  server: (
    <svg width="16" height="16" viewBox="0 0 28 28" fill="none">
      <rect x="3" y="7" width="22" height="6" rx="2" fill="none" stroke="currentColor" strokeWidth="2"/>
      <rect x="3" y="15" width="22" height="6" rx="2" fill="none" stroke="currentColor" strokeWidth="2"/>
      <circle cx="7.5" cy="10" r="1.4" fill="currentColor"/>
      <circle cx="7.5" cy="18" r="1.4" fill="currentColor"/>
    </svg>
  ),
  database: (
    <svg width="16" height="16" viewBox="0 0 28 28" fill="none">
      <ellipse cx="14" cy="9" rx="9" ry="3.5" fill="none" stroke="currentColor" strokeWidth="2"/>
      <path d="M5 9v10c0 1.93 4.03 3.5 9 3.5s9-1.57 9-3.5V9" fill="none" stroke="currentColor" strokeWidth="2"/>
    </svg>
  ),
  loadbalancer: (
    <svg width="16" height="16" viewBox="0 0 28 28" fill="none">
      <circle cx="7" cy="14" r="3" fill="none" stroke="currentColor" strokeWidth="2"/>
      <circle cx="21" cy="8" r="3" fill="none" stroke="currentColor" strokeWidth="2"/>
      <circle cx="21" cy="20" r="3" fill="none" stroke="currentColor" strokeWidth="2"/>
      <line x1="10" y1="14" x2="18" y2="9.5" stroke="currentColor" strokeWidth="1.6"/>
      <line x1="10" y1="14" x2="18" y2="18.5" stroke="currentColor" strokeWidth="1.6"/>
    </svg>
  ),
  redis: (
    <svg width="16" height="16" viewBox="0 0 28 28" fill="none">
      <polygon points="14,4 24,9 24,19 14,24 4,19 4,9" fill="none" stroke="currentColor" strokeWidth="2"/>
    </svg>
  ),
  queue: (
    <svg width="16" height="16" viewBox="0 0 28 28" fill="none">
      <rect x="3" y="11" width="5" height="6" rx="1" fill="none" stroke="currentColor" strokeWidth="2"/>
      <rect x="11" y="11" width="5" height="6" rx="1" fill="none" stroke="currentColor" strokeWidth="2"/>
      <rect x="19" y="11" width="5" height="6" rx="1" fill="none" stroke="currentColor" strokeWidth="2"/>
      <line x1="8" y1="14" x2="11" y2="14" stroke="currentColor" strokeWidth="1.6"/>
      <line x1="16" y1="14" x2="19" y2="14" stroke="currentColor" strokeWidth="1.6"/>
    </svg>
  ),
}

interface Props {
  id:       string
  data:     NodeData
  selected: boolean
}

function heat(v: number): string {
  if (v > 80) return '#ef4444'
  if (v > 60) return '#f59e0b'
  return '#22c55e'
}

function MiniBar({ pct, color }: { pct: number; color: string }) {
  return (
    <div style={{ height:3, background:'#f0f1f5', borderRadius:2, overflow:'hidden', marginTop:2 }}>
      <div style={{ height:'100%', width:`${Math.min(100,pct)}%`, background:color, borderRadius:2, transition:'width 0.35s' }} />
    </div>
  )
}

function MetricRow({ label, value, pct }: { label: string; value: string; pct?: number }) {
  const c = pct !== undefined ? heat(pct) : '#374151'
  return (
    <div style={{ marginBottom: 5 }}>
      <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center' }}>
        <span style={{ fontSize:10, color:'#9ca3af', fontWeight:500 }}>{label}</span>
        <span style={{ fontSize:11, fontFamily:'monospace', fontWeight:700, color: c }}>{value}</span>
      </div>
      {pct !== undefined && <MiniBar pct={pct} color={c} />}
    </div>
  )
}

const BaseNode = React.memo(({ id, data, selected }: Props) => {
  const removeNode = useStore(s => s.removeNode)
  const selectNode = useStore(s => s.selectNode)
  const meta       = COMP_META[data.compType]
  const m          = data.metrics

  return (
    <div
      className="node-card"
      style={{
        background: '#ffffff',
        border: `1.5px solid ${selected ? meta.color : '#e2e4e9'}`,
        borderRadius: 10,
        width: 168,
        boxShadow: selected
          ? `0 0 0 2px ${meta.color}33, 0 4px 16px rgba(0,0,0,0.10)`
          : '0 1px 4px rgba(0,0,0,0.08)',
        cursor: 'move',
        position: 'relative',
        transition: 'box-shadow 0.15s, border-color 0.15s',
      }}
      onClick={() => selectNode(id)}
    >
      {/* Top colour stripe */}
      <div style={{ height: 3, background: meta.color, borderRadius: '8px 8px 0 0' }} />

      {/* Header */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: 6,
        padding: '6px 8px 5px',
        borderBottom: '1px solid #f0f1f5',
      }}>
        <div style={{ color: meta.color, lineHeight: 0, flexShrink: 0 }}>
          {NODE_ICONS[data.compType]}
        </div>
        <span style={{
          fontSize: 11, fontWeight: 700, color: '#1b1b1f',
          flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
        }}>
          {data.label}
        </span>
        <button
          onClick={(e) => { e.stopPropagation(); removeNode(id) }}
          style={{
            background: 'transparent', border: 'none', color: '#c1c5d0',
            cursor: 'pointer', fontSize: 14, lineHeight: 1, padding: 0,
            display: 'flex', alignItems: 'center',
          }}
          title="Delete"
        >
          <svg width="12" height="12" viewBox="0 0 12 12" fill="none">
            <line x1="2" y1="2" x2="10" y2="10" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"/>
            <line x1="10" y1="2" x2="2" y2="10" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"/>
          </svg>
        </button>
      </div>

      {/* Metrics body */}
      <div style={{ padding: '7px 9px 9px' }}>
        <MetricRow label="CPU"     value={m ? `${m.cpuUsagePct.toFixed(0)}%`      : '—'} pct={m?.cpuUsagePct} />
        <MetricRow label="Queue"   value={m ? String(m.queueDepth)                : '—'} pct={m ? (m.queueDepth/Math.max(1,m.maxQueue))*100 : undefined} />
        <MetricRow label="Latency" value={m ? `${m.avgProcessingMs.toFixed(0)} ms` : '—'} />
      </div>
    </div>
  )
})

export default BaseNode
