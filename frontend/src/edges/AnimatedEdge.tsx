import { memo } from 'react'
import { getBezierPath } from 'reactflow'
import type { EdgeProps } from 'reactflow'

// Animated packet edge — dark dashed line with a moving packet dot (works on light canvas)
const AnimatedEdge = memo(({
  id, sourceX, sourceY, targetX, targetY,
  sourcePosition, targetPosition,
}: EdgeProps) => {
  const [edgePath] = getBezierPath({ sourceX, sourceY, sourcePosition, targetX, targetY, targetPosition })

  return (
    <g>
      {/* Shadow/glow path for contrast */}
      <path
        d={edgePath}
        fill="none"
        stroke="#ffffff"
        strokeWidth={4}
        strokeOpacity={0.5}
      />
      {/* Main dashed line */}
      <path
        id={`edge-path-${id}`}
        d={edgePath}
        fill="none"
        stroke="#6366f1"
        strokeWidth={2}
        strokeOpacity={0.75}
        strokeDasharray="6 4"
        style={{ animation: 'dash-anim 1.2s linear infinite' }}
      />
      {/* Packet dot */}
      <circle r={4} fill="#6366f1" opacity={0.9}
        style={{ filter: 'drop-shadow(0 0 3px #6366f180)' }}>
        <animateMotion dur="1.4s" repeatCount="indefinite" rotate="auto">
          <mpath href={`#edge-path-${id}`} />
        </animateMotion>
      </circle>
    </g>
  )
})

export const edgeTypes = { animated: AnimatedEdge }
