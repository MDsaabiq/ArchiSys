import React from 'react'
import type { NodeData } from '../store/useStore'
import { COMP_META } from '../store/useStore'
import { useStore } from '../store/useStore'

// ─────────────────────────────────────────────────────────────────────────────
// Each component type renders as its real architectural symbol in SVG.
// The shape itself is clickable/selectable — no wrapping rectangle.
// ─────────────────────────────────────────────────────────────────────────────

function heat(v: number) { return v > 80 ? '#ef4444' : v > 60 ? '#f59e0b' : '#22c55e' }

// Arc-based CPU ring (SVG circle with dasharray)
function CpuRing({ pct, color }: { pct: number; color: string }) {
  const r = 9, cx = 13, cy = 13, circ = 2 * Math.PI * r
  const dash = Math.min(1, pct / 100) * circ
  return (
    <svg width="26" height="26" viewBox="0 0 26 26" style={{ transform: 'rotate(-90deg)', flexShrink: 0 }}>
      <circle cx={cx} cy={cy} r={r} fill="none" stroke="#e5e7eb" strokeWidth="2.5"/>
      <circle cx={cx} cy={cy} r={r} fill="none" stroke={color} strokeWidth="2.5"
        strokeDasharray={`${dash} ${circ}`} strokeLinecap="round"
        style={{ transition: 'stroke-dasharray 0.4s' }}/>
    </svg>
  )
}

// Thin horizontal bar for queue fill
function QBar({ pct, color }: { pct: number; color: string }) {
  return (
    <div style={{ flex: 1, height: 4, background: '#f0f1f5', borderRadius: 3, overflow: 'hidden' }}>
      <div style={{ height: '100%', width: `${Math.min(100, pct)}%`, background: color, borderRadius: 3, transition: 'width 0.35s' }}/>
    </div>
  )
}

// ─────────────────────────────────────────────────────────────────────────────
// Shape components — each returns an <svg> that IS the visual body of the node
// ─────────────────────────────────────────────────────────────────────────────

// CLIENT — person outline
function ShapeClient({ color, selected }: { color: string; selected: boolean }) {
  return (
    <svg width="80" height="80" viewBox="0 0 80 80" fill="none">
      <circle cx="40" cy="40" r="36"
        fill={selected ? `${color}15` : '#f8f9fa'}
        stroke={selected ? color : '#d1d5db'}
        strokeWidth={selected ? 2 : 1.5}/>
      <circle cx="40" cy="30" r="9" stroke={color} strokeWidth="2.2" fill="none"/>
      <path d="M18 64c0-12.15 9.85-22 22-22s22 9.85 22 22"
        stroke={color} strokeWidth="2.2" strokeLinecap="round" fill="none"/>
    </svg>
  )
}

// SERVER — rack unit stack (3 bars inside a box shape)
function ShapeServer({ color, selected }: { color: string; selected: boolean }) {
  return (
    <svg width="90" height="80" viewBox="0 0 90 80" fill="none">
      <rect x="4" y="4" width="82" height="72" rx="6"
        fill={selected ? `${color}12` : '#f8f9fa'}
        stroke={selected ? color : '#d1d5db'}
        strokeWidth={selected ? 2 : 1.5}/>
      {/* 3 rack units */}
      {[16, 36, 56].map((y, i) => (
        <g key={i}>
          <rect x="12" y={y} width="66" height="14" rx="2.5"
            fill={selected ? `${color}20` : '#ffffff'}
            stroke={color} strokeWidth="1.4"/>
          <circle cx="20" cy={y + 7} r="2.5" fill={color}/>
          <line x1="28" y1={y + 7} x2="70" y2={y + 7}
            stroke={color} strokeWidth="1" strokeDasharray="4 3" opacity="0.4"/>
        </g>
      ))}
    </svg>
  )
}

// DATABASE — proper cylinder (ellipse top + body + ellipse bottom)
function ShapeDatabase({ color, selected }: { color: string; selected: boolean }) {
  const fill = selected ? `${color}12` : '#f8f9fa'
  const stroke = selected ? color : '#d1d5db'
  const sw = selected ? 2 : 1.5
  return (
    <svg width="76" height="88" viewBox="0 0 76 88" fill="none">
      {/* cylinder body */}
      <path d="M6 22 Q6 88 38 88 Q70 88 70 22"
        fill={fill} stroke={stroke} strokeWidth={sw}/>
      {/* top ellipse */}
      <ellipse cx="38" cy="22" rx="32" ry="12"
        fill={selected ? `${color}22` : '#ffffff'}
        stroke={selected ? color : '#d1d5db'} strokeWidth={sw}/>
      {/* seam lines */}
      <path d="M6 40 Q6 52 38 52 Q70 52 70 40"
        fill="none" stroke={color} strokeWidth="1.2" strokeDasharray="4 3" opacity="0.4"/>
      <path d="M6 60 Q6 72 38 72 Q70 72 70 60"
        fill="none" stroke={color} strokeWidth="1.2" strokeDasharray="4 3" opacity="0.4"/>
    </svg>
  )
}

// LOAD BALANCER — diamond shape with flow arrows
function ShapeLoadBalancer({ color, selected }: { color: string; selected: boolean }) {
  return (
    <svg width="90" height="90" viewBox="0 0 90 90" fill="none">
      {/* diamond */}
      <polygon points="45,5 85,45 45,85 5,45"
        fill={selected ? `${color}15` : '#f8f9fa'}
        stroke={selected ? color : '#d1d5db'}
        strokeWidth={selected ? 2 : 1.5}/>
      {/* LB icon inside */}
      <circle cx="25" cy="45" r="6" stroke={color} strokeWidth="1.8" fill="none"/>
      <circle cx="65" cy="28" r="5" stroke={color} strokeWidth="1.8" fill="none"/>
      <circle cx="65" cy="62" r="5" stroke={color} strokeWidth="1.8" fill="none"/>
      <line x1="31" y1="45" x2="60" y2="30" stroke={color} strokeWidth="1.5"/>
      <line x1="31" y1="45" x2="60" y2="60" stroke={color} strokeWidth="1.5"/>
      {/* arrowheads */}
      <polyline points="56,27 60,30 56,33" fill="none" stroke={color} strokeWidth="1.3"/>
      <polyline points="56,57 60,60 56,63" fill="none" stroke={color} strokeWidth="1.3"/>
    </svg>
  )
}

// REDIS — hexagon (recognizable Redis cluster shape)
function ShapeRedis({ color, selected }: { color: string; selected: boolean }) {
  return (
    <svg width="82" height="82" viewBox="0 0 82 82" fill="none">
      {/* hexagon */}
      <polygon points="41,4 76,22.5 76,59.5 41,78 6,59.5 6,22.5"
        fill={selected ? `${color}15` : '#f8f9fa'}
        stroke={selected ? color : '#d1d5db'}
        strokeWidth={selected ? 2 : 1.5}/>
      {/* internal grid (lightning bolt = speed) */}
      <path d="M44 18 L32 42 H42 L38 64 L55 38 H44 L48 18 Z"
        fill={color} opacity={selected ? 0.9 : 0.65}/>
    </svg>
  )
}

// MESSAGE QUEUE — pipeline / queue symbol (left-to-right slots with arrow)
function ShapeQueue({ color, selected }: { color: string; selected: boolean }) {
  return (
    <svg width="96" height="72" viewBox="0 0 96 72" fill="none">
      <rect x="4" y="4" width="88" height="64" rx="6"
        fill={selected ? `${color}12` : '#f8f9fa'}
        stroke={selected ? color : '#d1d5db'}
        strokeWidth={selected ? 2 : 1.5}/>
      {/* 4 queue slots */}
      {[13, 31, 49, 67].map((x, i) => (
        <rect key={i} x={x} y="16" width="14" height="40" rx="3"
          fill={selected ? `${color}25` : '#ffffff'}
          stroke={color} strokeWidth="1.4"/>
      ))}
      {/* flow arrow */}
      <line x1="12" y1="56" x2="78" y2="56" stroke={color} strokeWidth="1.5" strokeDasharray="5 3" opacity="0.5"/>
      <polyline points="74,52 80,56 74,60" fill="none" stroke={color} strokeWidth="1.5" opacity="0.7"/>
    </svg>
  )
}

// ─────────────────────────────────────────────────────────────────────────────
// Map type → shape component
// ─────────────────────────────────────────────────────────────────────────────
const SHAPES: Record<string, React.ComponentType<{ color: string; selected: boolean }>> = {
  client:       ShapeClient,
  server:       ShapeServer,
  database:     ShapeDatabase,
  loadbalancer: ShapeLoadBalancer,
  redis:        ShapeRedis,
  queue:        ShapeQueue,
}

// ─────────────────────────────────────────────────────────────────────────────
// BaseNode — pure SVG shape + label + metrics panel
// ─────────────────────────────────────────────────────────────────────────────
interface Props { id: string; data: NodeData; selected: boolean }

const BaseNode = React.memo(({ id, data, selected }: Props) => {
  const removeNode = useStore(s => s.removeNode)
  const selectNode = useStore(s => s.selectNode)
  const meta = COMP_META[data.compType]
  const m    = data.metrics

  const cpuPct   = m?.cpuUsagePct ?? 0
  const queuePct = m ? (m.queueDepth / Math.max(1, m.maxQueue)) * 100 : 0
  const cpuColor = m ? heat(cpuPct) : '#d1d5db'
  const qColor   = m ? heat(queuePct) : '#d1d5db'

  const Shape = SHAPES[data.compType] ?? SHAPES['server']
  const isClient = data.compType === 'client'

  return (
    <div
      onClick={() => selectNode(id)}
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        cursor: 'move',
        userSelect: 'none',
        position: 'relative',
      }}
    >
      {/* ── Architectural shape (the node body) ── */}
      <div style={{ position: 'relative', lineHeight: 0 }}>
        <Shape color={meta.color} selected={selected}/>

        {/* Delete button — floats top-right of shape */}
        <button
          onClick={e => { e.stopPropagation(); removeNode(id) }}
          style={{
            position: 'absolute', top: -4, right: -4,
            width: 16, height: 16, borderRadius: '50%',
            background: '#ffffff', border: '1px solid #e2e4e9',
            color: '#9ca3af', cursor: 'pointer',
            fontSize: 10, lineHeight: '14px', textAlign: 'center',
            padding: 0, display: 'flex', alignItems: 'center', justifyContent: 'center',
            boxShadow: '0 1px 3px rgba(0,0,0,0.12)',
            opacity: selected ? 1 : 0,
            transition: 'opacity 0.15s',
          }}
          title="Delete"
          onMouseEnter={e => (e.currentTarget.style.opacity = '1')}
        >
          <svg width="8" height="8" viewBox="0 0 8 8" fill="none">
            <line x1="1" y1="1" x2="7" y2="7" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round"/>
            <line x1="7" y1="1" x2="1" y2="7" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round"/>
          </svg>
        </button>
      </div>

      {/* ── Label ── */}
      <div style={{
        marginTop: 5,
        fontSize: 11, fontWeight: 700, color: '#1b1b1f',
        textAlign: 'center', maxWidth: 100,
        overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
      }}>
        {data.label}
      </div>

      {/* ── Metrics panel — appears only when simulation is running ── */}
      {m && !isClient && (
        <div style={{
          marginTop: 5,
          width: 110,
          background: '#ffffff',
          border: `1px solid ${queuePct > 80 ? '#fecaca' : '#e2e4e9'}`,
          borderRadius: 7,
          padding: '6px 8px',
          boxShadow: '0 1px 4px rgba(0,0,0,0.07)',
        }}>
          {/* CPU ring + latency */}
          <div style={{ display: 'flex', alignItems: 'center', gap: 5, marginBottom: 5 }}>
            <CpuRing pct={cpuPct} color={cpuColor}/>
            <div style={{ flex: 1 }}>
              <div style={{ fontSize: 8, color: '#9ca3af', lineHeight: 1 }}>CPU</div>
              <div style={{ fontSize: 11, fontWeight: 700, fontFamily: 'monospace', color: cpuColor, lineHeight: 1.2 }}>
                {cpuPct.toFixed(0)}%
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{ fontSize: 8, color: '#9ca3af', lineHeight: 1 }}>Latency</div>
              <div style={{ fontSize: 11, fontWeight: 700, fontFamily: 'monospace', color: '#374151', lineHeight: 1.2 }}>
                {m.avgProcessingMs.toFixed(0)}<span style={{ fontSize: 8, color: '#9ca3af' }}> ms</span>
              </div>
            </div>
          </div>
          {/* Queue bar */}
          <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
            <span style={{ fontSize: 8, color: '#9ca3af', flexShrink: 0 }}>Q</span>
            <QBar pct={queuePct} color={qColor}/>
            <span style={{ fontSize: 9, fontFamily: 'monospace', fontWeight: 700, color: qColor, flexShrink: 0, minWidth: 20, textAlign: 'right' }}>
              {m.queueDepth}
            </span>
          </div>
        </div>
      )}

      {/* Idle hint when no metrics yet */}
      {!m && !isClient && (
        <div style={{ marginTop: 4, fontSize: 9, color: '#c1c5d0', fontWeight: 500, letterSpacing: '0.4px', textTransform: 'uppercase' }}>
          idle
        </div>
      )}
    </div>
  )
})

export default BaseNode
