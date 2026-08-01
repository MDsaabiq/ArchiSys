import React, { useCallback, useRef, useEffect } from 'react'
import ReactFlow, {
  Background, Controls, BackgroundVariant,
  addEdge, useNodesState, useEdgesState,
  type Connection,
  ReactFlowProvider,
} from 'reactflow'
import 'reactflow/dist/style.css'

import { useStore } from './store/useStore'
import { nodeTypes } from './nodes/index'
import { edgeTypes } from './edges/AnimatedEdge'
import Toolbar from './components/Toolbar'
import Sidebar from './components/Sidebar'
import PropertiesPanel from './components/PropertiesPanel'
import BottomBar from './components/BottomBar'
import { useWebSocket } from './hooks/useWebSocket'
import type { ComponentType } from './types/architecture'

import './App.css'

// ── Canvas (must be inside ReactFlowProvider) ─────────────────────────────────
function Canvas() {
  const { nodes, edges, setNodes, setEdges, addNode } = useStore()
  const reactFlowWrapper = useRef<HTMLDivElement>(null)
  const [rfNodes, setRfNodes, onNodesChange] = useNodesState(nodes as any)
  const [rfEdges, setRfEdges, onEdgesChange] = useEdgesState(edges as any)

  // ── Sync Zustand → React Flow on EVERY node data change (metrics updates)
  // Use a ref to avoid stale closure; compare by reference to avoid loop.
  const prevNodesRef = useRef(nodes)
  useEffect(() => {
    if (nodes !== prevNodesRef.current) {
      prevNodesRef.current = nodes
      setRfNodes(nodes as any)
    }
  })

  // Sync edge additions/removals
  const prevEdgesRef = useRef(edges)
  useEffect(() => {
    if (edges !== prevEdgesRef.current) {
      prevEdgesRef.current = edges
      setRfEdges(edges as any)
    }
  })

  const onConnect = useCallback((connection: Connection) => {
    const newEdge = { ...connection, type: 'animated', id: `e-${connection.source}-${connection.target}` }
    const updated = addEdge(newEdge, rfEdges)
    setRfEdges(updated)
    setEdges(updated)
  }, [rfEdges, setEdges])

  const onDragOver = useCallback((e: React.DragEvent) => {
    e.preventDefault()
    e.dataTransfer.dropEffect = 'copy'
  }, [])

  const onDrop = useCallback((e: React.DragEvent) => {
    e.preventDefault()
    const type = e.dataTransfer.getData('archisys/type') as ComponentType
    if (!type || !reactFlowWrapper.current) return
    const rect = reactFlowWrapper.current.getBoundingClientRect()
    addNode(type, e.clientX - rect.left - 80, e.clientY - rect.top - 50)
  }, [addNode])

  return (
    <div ref={reactFlowWrapper} style={{ flex: 1, position: 'relative' }}>
      <ReactFlow
        nodes={rfNodes}
        edges={rfEdges}
        onNodesChange={(ch) => { onNodesChange(ch); setNodes(rfNodes as any) }}
        onEdgesChange={(ch) => { onEdgesChange(ch); setEdges(rfEdges as any) }}
        onConnect={onConnect}
        onDragOver={onDragOver}
        onDrop={onDrop}
        nodeTypes={nodeTypes}
        edgeTypes={edgeTypes}
        defaultEdgeOptions={{ type: 'animated' }}
        fitView
        style={{ background: '#ffffff' }}
        proOptions={{ hideAttribution: true }}
      >
        <Background variant={BackgroundVariant.Dots} color="#c8cad0" gap={20} size={1.2} />
        <Controls style={{ borderRadius: 8, overflow: 'hidden', boxShadow: '0 1px 6px rgba(0,0,0,0.1)' }} />
      </ReactFlow>
    </div>
  )
}

// ── App root ─────────────────────────────────────────────────────────────────
export default function App() {
  useWebSocket()
  return (
    <ReactFlowProvider>
      <div style={{ display:'flex', flexDirection:'column', height:'100vh', overflow:'hidden', background:'#f0f1f5' }}>
        <Toolbar />
        <div style={{ flex:1, display:'flex', overflow:'hidden' }}>
          <Sidebar />
          <Canvas />
          <PropertiesPanel />
        </div>
        <BottomBar />
      </div>
    </ReactFlowProvider>
  )
}
