import { Handle, Position } from 'reactflow'
import BaseNode from './BaseNode'
import type { NodeProps } from 'reactflow'
import type { NodeData } from '../store/useStore'

// Single shared node component — BaseNode handles all visuals.
// React Flow wraps require Handles to be inside the custom component.
const ArchNode = (props: NodeProps<NodeData>) => (
  <>
    <Handle type="target" position={Position.Left}  style={{ background:'#30363d', border:'2px solid #3b82f6', width:10, height:10 }} />
    <BaseNode id={props.id} data={props.data} selected={props.selected} />
    <Handle type="source" position={Position.Right} style={{ background:'#30363d', border:'2px solid #3b82f6', width:10, height:10 }} />
  </>
)

export const nodeTypes = { archNode: ArchNode }
