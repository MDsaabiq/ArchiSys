import React from 'react'
import { useStore } from '../store/useStore'
import { COMP_META } from '../store/useStore'

export default function PropertiesPanel() {
  const { selectedNodeId, nodes, updateNodeConfig, updateNodeLabel } = useStore()
  const node = nodes.find(n => n.id === selectedNodeId)

  if (!node) {
    return (
      <div style={{
        width: 240, background: '#ffffff', borderLeft: '1px solid #e2e4e9',
        flexShrink: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
      }}>
        <div style={{ textAlign: 'center', padding: 24 }}>
          <svg width="36" height="36" viewBox="0 0 36 36" fill="none" style={{ display:'block', margin:'0 auto 10px' }}>
            <rect x="6" y="6" width="10" height="10" rx="2" fill="#e2e4e9"/>
            <rect x="20" y="6" width="10" height="10" rx="2" fill="#e2e4e9"/>
            <rect x="6" y="20" width="10" height="10" rx="2" fill="#e2e4e9"/>
            <rect x="20" y="20" width="10" height="10" rx="2" fill="#e9eaef"/>
          </svg>
          <div style={{ color: '#9ca3af', fontSize: 12, fontWeight: 500 }}>Select a component</div>
          <div style={{ color: '#c1c5d0', fontSize: 11, marginTop: 4 }}>to edit its properties</div>
        </div>
      </div>
    )
  }

  const { config, metrics, label, compType } = node.data
  const meta = COMP_META[compType]

  return (
    <div style={{
      width: 240, background: '#ffffff', borderLeft: '1px solid #e2e4e9',
      flexShrink: 0, overflowY: 'auto', display: 'flex', flexDirection: 'column',
    }}>
      {/* Header — colored top bar */}
      <div style={{ height: 4, background: meta.color, flexShrink: 0 }} />
      <div style={{ padding:'10px 14px 8px', borderBottom:'1px solid #f0f1f5', flexShrink: 0 }}>
        <div style={{ fontSize: 10, fontWeight: 700, color: '#9ca3af', letterSpacing: '1px', textTransform: 'uppercase', marginBottom: 6 }}>
          Properties
        </div>
        <input
          value={label}
          onChange={e => updateNodeLabel(node.id, e.target.value)}
          style={{
            width: '100%', background: '#f8f9fa', border: '1px solid #e2e4e9',
            borderRadius: 6, color: '#1b1b1f', fontSize: 13, fontWeight: 600,
            padding: '6px 9px', outline: 'none', fontFamily: 'inherit',
          }}
        />
      </div>

      {/* Configuration */}
      <div style={{ padding:'12px 14px', borderBottom:'1px solid #f0f1f5' }}>
        <SectionTitle>Configuration</SectionTitle>
        {compType === 'client' ? (
          <>
            <SliderRow label="Request Rate"    unit=" r/s" min={1}   max={2000}  value={config.requestRate   ?? 100} onChange={v => updateNodeConfig(node.id, { requestRate:v })}   color={meta.color} />
            <SliderRow label="Total Requests"  unit=""     min={0}   max={50000} value={config.totalRequests ?? 0}   onChange={v => updateNodeConfig(node.id, { totalRequests:v })} color={meta.color} step={100} />
            <div style={{ fontSize:10, color:'#9ca3af', marginTop:-4, marginBottom:8 }}>Total Requests = 0 means run until stopped</div>
          </>
        ) : (
          <>
            <SliderRow label="CPU Cores"  unit=""   min={1}  max={16}   value={config.cpuCores}  onChange={v => updateNodeConfig(node.id, { cpuCores:v })}  color={meta.color} />
            <SliderRow label="Proc Time"  unit="ms" min={1}  max={500}  value={config.procTime}  onChange={v => updateNodeConfig(node.id, { procTime:v })}  color={meta.color} />
            <SliderRow label="Max Queue"  unit=""   min={10} max={1000} value={config.maxQueue}  onChange={v => updateNodeConfig(node.id, { maxQueue:v })}  color={meta.color} />
            <SliderRow label="Instances"  unit=""   min={1}  max={8}    value={config.instances} onChange={v => updateNodeConfig(node.id, { instances:v })} color={meta.color} />
          </>
        )}
      </div>

      {/* Live Metrics */}
      <div style={{ padding:'12px 14px' }}>
        <SectionTitle>Live Metrics</SectionTitle>
        <LiveMetric label="CPU Usage"   value={metrics ? `${metrics.cpuUsagePct.toFixed(0)}%`       : '—'} pct={metrics?.cpuUsagePct} />
        <LiveMetric label="Queue Depth" value={metrics ? String(metrics.queueDepth)                  : '—'} pct={metrics ? (metrics.queueDepth/Math.max(1,metrics.maxQueue))*100 : undefined} />
        <LiveMetric label="Avg Latency" value={metrics ? `${metrics.avgProcessingMs.toFixed(1)} ms`  : '—'} />
        <LiveMetric label="Throughput"  value={metrics ? `${metrics.throughputPerSec.toFixed(1)} r/s` : '—'} />
        <LiveMetric label="Completed"   value={metrics ? String(metrics.requestsCompleted)           : '—'} />
        <LiveMetric label="Dropped"     value={metrics ? String(metrics.requestsDropped)             : '—'} />
      </div>
    </div>
  )
}

function SectionTitle({ children }: { children: React.ReactNode }) {
  return (
    <div style={{ fontSize:10, fontWeight:700, color:'#9ca3af', letterSpacing:'1px', textTransform:'uppercase', marginBottom:10 }}>
      {children}
    </div>
  )
}

function SliderRow({ label, unit, min, max, value, onChange, color, step = 1 }: {
  label: string; unit: string; min: number; max: number; value: number
  onChange: (v: number) => void; color: string; step?: number
}) {
  return (
    <div style={{ marginBottom:12 }}>
      <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center', marginBottom:5 }}>
        <span style={{ fontSize:12, color:'#374151', fontWeight:500 }}>{label}</span>
        <span style={{
          fontSize:11, fontFamily:'monospace', fontWeight:700,
          color: '#ffffff', background: color,
          padding:'1px 7px', borderRadius:10, minWidth:28, textAlign:'center',
        }}>{value}{unit}</span>
      </div>
      <input type="range" min={min} max={max} step={step} value={value}
        onChange={e => onChange(Number(e.target.value))}
        style={{ width:'100%', cursor:'pointer', accentColor: color }} />
    </div>
  )
}

function LiveMetric({ label, value, pct }: { label: string; value: string; pct?: number }) {
  const heatColor = (v: number) => v > 80 ? '#ef4444' : v > 60 ? '#f59e0b' : '#22c55e'
  const c = pct !== undefined ? heatColor(pct) : '#6366f1'
  return (
    <div style={{ marginBottom:11 }}>
      <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center', marginBottom: pct !== undefined ? 4 : 0 }}>
        <span style={{ fontSize:12, color:'#6b7280' }}>{label}</span>
        <span style={{ fontSize:12, fontFamily:'monospace', fontWeight:700, color: c }}>{value}</span>
      </div>
      {pct !== undefined && (
        <div style={{ height:4, background:'#f0f1f5', borderRadius:3, overflow:'hidden' }}>
          <div style={{ height:'100%', width:`${Math.min(100,pct)}%`, background:c, borderRadius:3, transition:'width 0.4s' }} />
        </div>
      )}
    </div>
  )
}
