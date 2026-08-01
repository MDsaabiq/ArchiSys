import { useStore } from '../store/useStore'

export default function BottomBar() {
  const m        = useStore(s => s.latestMetrics)
  const nodes    = useStore(s => s.nodes)
  const settings = useStore(s => s.simSettings)

  // Compute theoretical capacity from server/LB nodes
  const clientNode = nodes.find(n => n.data.compType === 'client')
  const rate = clientNode?.data.config.requestRate ?? settings.requestRate

  // Warn if drop rate > 2%
  const dropPct = m && m.totalRequests > 0 ? m.dropped / m.totalRequests : 0
  const hasDropWarning = dropPct > 0.02

  const items = [
    { label: 'Total Req',   value: m ? m.totalRequests.toLocaleString()              : '0' },
    { label: 'Completed',   value: m ? m.completed.toLocaleString()                  : '0',  dot: '#22c55e' },
    { label: 'Dropped',     value: m ? m.dropped.toLocaleString()                    : '0',  dot: m && m.dropped > 0 ? '#ef4444' : '#9ca3af', warn: hasDropWarning },
    { label: 'In Flight',   value: m ? m.inFlight.toLocaleString()                   : '0' },
    { label: 'Avg Latency', value: m ? `${m.avgLatencyMs.toFixed(1)} ms`             : '—',  dot: '#6366f1' },
    { label: 'Throughput',  value: m ? `${m.throughputPerSec.toFixed(1)} req/s`      : '—' },
    { label: 'Req Rate',    value: `${rate} req/s`,                                          dot: '#0ea5e9' },
    { label: 'Sim Time',    value: m ? `${m.simTimeSec.toFixed(1)} s`                : '0.0 s' },
  ]

  return (
    <div style={{
      background: '#ffffff',
      borderTop: '1px solid #e2e4e9',
      flexShrink: 0,
    }}>
      {/* Drop warning banner */}
      {hasDropWarning && m && (
        <div style={{
          background: '#fef2f2', borderBottom: '1px solid #fecaca',
          padding: '4px 14px', fontSize: 11, color: '#b91c1c',
          display: 'flex', alignItems: 'center', gap: 6,
        }}>
          <span style={{ fontWeight: 700 }}>⚠ {(dropPct * 100).toFixed(1)}% requests dropped</span>
          <span style={{ color: '#9ca3af' }}>— Request rate ({rate} req/s) exceeds server capacity. Reduce rate or add more server instances.</span>
        </div>
      )}

      {/* Metrics strip */}
      <div style={{
        height: 38,
        display: 'flex',
        alignItems: 'center',
        padding: '0 10px',
        overflow: 'hidden',
        gap: 0,
      }}>
        {items.map((item, i) => (
          <div key={item.label} style={{
            display: 'flex', alignItems: 'center', gap: 5,
            padding: '0 12px',
            borderRight: i < items.length - 1 ? '1px solid #f0f1f5' : 'none',
            flexShrink: 0,
          }}>
            {item.dot && (
              <div style={{ width: 6, height: 6, borderRadius: '50%', background: item.dot, flexShrink: 0 }} />
            )}
            <span style={{ fontSize: 11, color: '#9ca3af', fontWeight: 500 }}>{item.label}</span>
            <span style={{
              fontSize: 12, fontFamily: 'monospace', fontWeight: 700,
              color: item.warn ? '#ef4444' : '#1b1b1f',
            }}>{item.value}</span>
          </div>
        ))}
      </div>
    </div>
  )
}
