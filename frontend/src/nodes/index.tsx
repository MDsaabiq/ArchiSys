import { Handle, Position } from 'reactflow'
import BaseNode from './BaseNode'
import type { NodeProps } from 'reactflow'
import type { NodeData } from '../store/useStore'

const ArchNode = (props: NodeProps<NodeData>) => (
  <>
    <Handle
      type="target"
      position={Position.Left}
      style={{ background: '#fff', border: '2px solid #6366f1', width: 10, height: 10, top: 46 }}
    />
    <BaseNode id={props.id} data={props.data} selected={props.selected} />
    <Handle
      type="source"
      position={Position.Right}
      style={{ background: '#fff', border: '2px solid #6366f1', width: 10, height: 10, top: 46 }}
    />
  </>
)

export const nodeTypes = { archNode: ArchNode }
