import React from 'react'
import { useStore } from '../store/useStore'
import { useSimulation } from '../hooks/useSimulation'

export default function Toolbar() {
  const { simStatus, nodes, edges, simSettings, setSimSettings } = useStore()
  const { start, stop } = useSimulation()
  const [showSettings, setShowSettings] = React.useState(false)

  const isRunning = simStatus === 'running'

  // Check if canvas has a Client node — if so, settings come from that node
  const hasClientNode = nodes.some(n => n.data.compType === 'client')

  const exportJson = () => {
    const data = {
      nodes: nodes.map(n => ({
        id: n.data.componentId, type: n.data.compType, name: n.data.label,
        x: n.position.x, y: n.position.y, config: n.data.config,
      })),
      edges: edges.map((e, i) => {
        const from = nodes.find(n => n.id === e.source)
        const to   = nodes.find(n => n.id === e.target)
        return { id: i+1, fromId: from?.data.componentId??0, toId: to?.data.componentId??0 }
      }),
    }
    const blob = new Blob([JSON.stringify(data, null, 2)], { type:'application/json' })
    const a = document.createElement('a'); a.href = URL.createObjectURL(blob)
    a.download = 'architecture.json'; a.click()
  }

  return (
    <div style={{ position: 'relative', zIndex: 200, flexShrink: 0 }}>
      <div style={{
        height: 48,
        background: '#ffffff',
        borderBottom: '1px solid #e2e4e9',
        display: 'flex',
        alignItems: 'center',
        padding: '0 14px',
        gap: 8,
        boxShadow: '0 1px 3px rgba(0,0,0,0.06)',
      }}>
        {/* Logo */}
        <div style={{ display:'flex', alignItems:'center', gap:7, marginRight:14 }}>
          <svg width="22" height="22" viewBox="0 0 22 22" fill="none">
            <rect x="1" y="1" width="8" height="8" rx="2" fill="#6366f1"/>
            <rect x="13" y="1" width="8" height="8" rx="2" fill="#22c55e"/>
            <rect x="1" y="13" width="8" height="8" rx="2" fill="#f59e0b"/>
            <rect x="13" y="13" width="8" height="8" rx="2" fill="#ef4444"/>
          </svg>
          <span style={{ fontWeight: 700, fontSize: 14, color: '#1b1b1f' }}>ArchiSys</span>
        </div>

        <div style={{ width:1, height:22, background:'#e2e4e9', marginRight:4 }} />

        {/* Run / Stop */}
        {!isRunning
          ? <ToolBtn onClick={start} accent="#6366f1" filled>▶ Run Simulation</ToolBtn>
          : <ToolBtn onClick={stop}  accent="#ef4444" filled>■ Stop</ToolBtn>
        }

        {/* Sim Settings toggle — only when no Client node */}
        {!hasClientNode && !isRunning && (
          <ToolBtn onClick={() => setShowSettings(v => !v)} accent={showSettings ? '#6366f1' : undefined}>
            ⚙ Sim Settings
          </ToolBtn>
        )}

        <ToolBtn onClick={() => useStore.getState().setNodes([])}>✕ Clear</ToolBtn>
        <ToolBtn onClick={exportJson}>↓ Export JSON</ToolBtn>

        {/* Status pill */}
        <div style={{ marginLeft:'auto', display:'flex', alignItems:'center', gap:6, fontSize:11, color:'#6b7280' }}>
          <div style={{
            width:8, height:8, borderRadius:'50%',
            background: isRunning ? '#22c55e' : '#9ca3af',
            animation: isRunning ? 'pulse 1.4s ease-in-out infinite' : 'none',
          }} />
          <span style={{ fontWeight:600, letterSpacing:'0.5px', fontSize:11 }}>
            {isRunning ? 'SIMULATING' : simStatus === 'stopped' ? 'STOPPED' : 'IDLE'}
          </span>
        </div>
      </div>

      {/* ── Sim Settings dropdown ── */}
      {showSettings && !isRunning && !hasClientNode && (
        <div style={{
          position: 'absolute', top: 48, left: 0, right: 0,
          background: '#ffffff',
          borderBottom: '1px solid #e2e4e9',
          boxShadow: '0 4px 12px rgba(0,0,0,0.08)',
          display: 'flex', alignItems: 'center', gap: 0,
          padding: '10px 18px',
          zIndex: 150,
        }}>
          <span style={{ fontSize:11, fontWeight:700, color:'#9ca3af', textTransform:'uppercase', letterSpacing:'0.8px', marginRight:20, whiteSpace:'nowrap' }}>
            Simulation Settings
          </span>

          <SettingField
            label="Request Rate"
            hint="req/s"
            value={simSettings.requestRate}
            min={1} max={2000}
            onChange={v => setSimSettings({ requestRate: v })}
          />

          <div style={{ width:1, height:32, background:'#f0f1f5', margin:'0 16px' }} />

          <SettingField
            label="Total Requests"
            hint="0 = unlimited"
            value={simSettings.totalRequests}
            min={0} max={100000}
            step={100}
            onChange={v => setSimSettings({ totalRequests: v })}
          />

          <div style={{ width:1, height:32, background:'#f0f1f5', margin:'0 16px' }} />

          <SettingField
            label="Random Seed"
            hint=""
            value={simSettings.seed}
            min={1} max={9999}
            onChange={v => setSimSettings({ seed: v })}
          />

          <div style={{ marginLeft:'auto', fontSize:11, color:'#9ca3af', maxWidth:240 }}>
            {simSettings.totalRequests === 0
              ? '⚠ Runs until you click Stop'
              : `Will stop after ${simSettings.totalRequests.toLocaleString()} requests`}
          </div>
        </div>
      )}

      {/* Hint when has Client node */}
      {hasClientNode && !isRunning && (
        <div style={{
          position:'absolute', top:48, left:0, right:0,
          background:'#eff6ff', borderBottom:'1px solid #bfdbfe',
          padding:'6px 18px', fontSize:11, color:'#1d4ed8', zIndex:150,
        }}>
          ℹ Request rate and total requests are controlled by the <strong>Client</strong> node — edit them in the Properties panel.
        </div>
      )}
    </div>
  )
}

function SettingField({ label, hint, value, min, max, step = 1, onChange }: {
  label: string; hint: string; value: number; min: number; max: number
  step?: number; onChange: (v: number) => void
}) {
  return (
    <div style={{ display:'flex', alignItems:'center', gap:8 }}>
      <div>
        <div style={{ fontSize:10, fontWeight:600, color:'#6b7280', textTransform:'uppercase', letterSpacing:'0.5px', marginBottom:2 }}>
          {label}
        </div>
        <div style={{ fontSize:10, color:'#c1c5d0' }}>{hint}</div>
      </div>
      <input
        type="number"
        value={value}
        min={min}
        max={max}
        step={step}
        onChange={e => onChange(Number(e.target.value))}
        style={{
          width: 90, background:'#f8f9fa', border:'1px solid #e2e4e9',
          borderRadius:6, color:'#1b1b1f', fontSize:13, fontWeight:600,
          padding:'5px 8px', outline:'none', fontFamily:'monospace',
        }}
      />
    </div>
  )
}

function ToolBtn({ children, onClick, accent, filled }: {
  children: React.ReactNode; onClick: () => void; accent?: string; filled?: boolean
}) {
  const [hovered, setHovered] = React.useState(false)
  const bg = filled
    ? (hovered ? accent + 'dd' : accent)
    : (hovered ? '#f0f1f5' : '#ffffff')
  return (
    <button
      onClick={onClick}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        background: bg,
        border: `1px solid ${filled ? 'transparent' : (accent ? accent + '66' : '#d1d5db')}`,
        color: filled ? '#ffffff' : (accent ? accent : '#374151'),
        padding: '5px 12px', borderRadius: 6, cursor: 'pointer',
        fontSize: 12, fontFamily: 'inherit', fontWeight: filled ? 600 : 500,
        transition: 'background 0.12s', whiteSpace: 'nowrap',
      }}
    >{children}</button>
  )
}
