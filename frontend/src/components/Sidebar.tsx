import React from 'react'
import { COMP_META } from '../store/useStore'
import type { ComponentType } from '../types/architecture'

const SECTIONS = [
  { title: 'Entry Point',    types: ['client'] as ComponentType[] },
  { title: 'Infrastructure', types: ['server', 'database', 'loadbalancer'] as ComponentType[] },
  { title: 'Messaging',      types: ['redis', 'queue'] as ComponentType[] },
]

// Small icon SVGs per type (geometric, Excalidraw-style)
const TYPE_SHAPES: Record<ComponentType, React.ReactNode> = {
  client: (
    <svg width="28" height="28" viewBox="0 0 28 28" fill="none">
      <circle cx="14" cy="10" r="4.5" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <path d="M5 24c0-4.97 4.03-9 9-9s9 4.03 9 9" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round"/>
    </svg>
  ),
  server: (
    <svg width="28" height="28" viewBox="0 0 28 28" fill="none">
      <rect x="3" y="7" width="22" height="6" rx="2" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <rect x="3" y="15" width="22" height="6" rx="2" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <circle cx="7.5" cy="10" r="1.2" fill="currentColor"/>
      <circle cx="7.5" cy="18" r="1.2" fill="currentColor"/>
    </svg>
  ),
  database: (
    <svg width="28" height="28" viewBox="0 0 28 28" fill="none">
      <ellipse cx="14" cy="9" rx="9" ry="3.5" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <path d="M5 9v10c0 1.93 4.03 3.5 9 3.5s9-1.57 9-3.5V9" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <path d="M5 14c0 1.93 4.03 3.5 9 3.5s9-1.57 9-3.5" fill="none" stroke="currentColor" strokeWidth="1.2" strokeDasharray="2 2"/>
    </svg>
  ),
  loadbalancer: (
    <svg width="28" height="28" viewBox="0 0 28 28" fill="none">
      <circle cx="7" cy="14" r="3" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <circle cx="21" cy="8" r="3" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <circle cx="21" cy="20" r="3" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <line x1="10" y1="14" x2="18" y2="9.5" stroke="currentColor" strokeWidth="1.4"/>
      <line x1="10" y1="14" x2="18" y2="18.5" stroke="currentColor" strokeWidth="1.4"/>
    </svg>
  ),
  redis: (
    <svg width="28" height="28" viewBox="0 0 28 28" fill="none">
      <polygon points="14,4 24,9 24,19 14,24 4,19 4,9" fill="none" stroke="currentColor" strokeWidth="1.6"/>
      <line x1="14" y1="4" x2="14" y2="24" stroke="currentColor" strokeWidth="1" strokeDasharray="2 2"/>
    </svg>
  ),
  queue: (
    <svg width="28" height="28" viewBox="0 0 28 28" fill="none">
      <rect x="3" y="11" width="5" height="6" rx="1" fill="none" stroke="currentColor" strokeWidth="1.5"/>
      <rect x="11" y="11" width="5" height="6" rx="1" fill="none" stroke="currentColor" strokeWidth="1.5"/>
      <rect x="19" y="11" width="5" height="6" rx="1" fill="none" stroke="currentColor" strokeWidth="1.5"/>
      <line x1="8" y1="14" x2="11" y2="14" stroke="currentColor" strokeWidth="1.4"/>
      <line x1="16" y1="14" x2="19" y2="14" stroke="currentColor" strokeWidth="1.4"/>
    </svg>
  ),
}

const SUBTITLES: Record<ComponentType, string> = {
  client:       'Entry point · Sets request rate',
  server:       'Multi-instance · CPU bounded',
  database:     'Read/write latency modes',
  loadbalancer: 'Round-robin · LeastConn',
  redis:        'Hit ratio 80% · Instant hit',
  queue:        'Async FIFO broker',
}

export default function Sidebar() {
  const onDragStart = (e: React.DragEvent, type: ComponentType) => {
    e.dataTransfer.setData('archisys/type', type)
    e.dataTransfer.effectAllowed = 'copy'
  }

  return (
    <div style={{
      width: 210,
      background: '#ffffff',
      borderRight: '1px solid #e2e4e9',
      overflowY: 'auto',
      flexShrink: 0,
      display: 'flex',
      flexDirection: 'column',
    }}>
      {/* Header */}
      <div style={{ padding:'12px 14px 6px', borderBottom:'1px solid #f0f1f5' }}>
        <span style={{ fontSize:11, fontWeight:700, color:'#9ca3af', letterSpacing:'1px', textTransform:'uppercase' }}>
          Components
        </span>
        <div style={{ fontSize:11, color:'#c1c5d0', marginTop:2 }}>Drag onto canvas</div>
      </div>

      <div style={{ padding:'8px 10px', flex:1 }}>
        {SECTIONS.map(sec => (
          <div key={sec.title} style={{ marginBottom: 6 }}>
            <div style={{
              fontSize: 10, fontWeight: 700, color: '#9ca3af',
              letterSpacing: '1px', textTransform: 'uppercase',
              padding: '8px 4px 4px',
            }}>
              {sec.title}
            </div>
            {sec.types.map(type => {
              const m = COMP_META[type]
              return (
                <SidebarCard
                  key={type}
                  type={type}
                  label={m.label}
                  color={m.color}
                  subtitle={SUBTITLES[type]}
                  shape={TYPE_SHAPES[type]}
                  onDragStart={e => onDragStart(e, type)}
                />
              )
            })}
          </div>
        ))}
      </div>
    </div>
  )
}

function SidebarCard({ type, label, color, subtitle, shape, onDragStart }: {
  type: ComponentType
  label: string
  color: string
  subtitle: string
  shape: React.ReactNode
  onDragStart: (e: React.DragEvent) => void
}) {
  const [hovered, setHovered] = React.useState(false)
  return (
    <div
      draggable
      onDragStart={onDragStart}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: 10,
        padding: '8px 10px',
        background: hovered ? '#f5f6ff' : '#fafafa',
        border: `1px solid ${hovered ? color + '55' : '#e9eaef'}`,
        borderLeft: `3px solid ${color}`,
        borderRadius: 7,
        marginBottom: 5,
        cursor: 'grab',
        transition: 'all 0.12s',
        userSelect: 'none',
      }}
    >
      <div style={{ color, flexShrink:0, lineHeight:0 }}>{shape}</div>
      <div>
        <div style={{ fontSize: 12, fontWeight: 600, color: '#1b1b1f' }}>{label}</div>
        <div style={{ fontSize: 10, color: '#9ca3af', marginTop: 1, lineHeight: 1.3 }}>{subtitle}</div>
      </div>
    </div>
  )
}
