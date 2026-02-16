"use client";

import { useState, useCallback, useEffect, useMemo, useRef } from "react";
import Link from "next/link";
import {
  ReactFlow,
  useNodesState,
  useEdgesState,
  Handle,
  Position,
  getBezierPath,
  Background,
  BackgroundVariant,
  useReactFlow,
  ReactFlowProvider,
} from "@xyflow/react";
import "@xyflow/react/dist/style.css";

// =============================================================================
// DSL Parser - Matches Logicarium C++ parser syntax
// =============================================================================

type ParsedNode = {
  id: string;
  type: string;
  x: number;
  y: number;
  options?: string[];
};

type ParsedConnection = {
  sourceId: string;
  sourceSlot: string;
  targetId: string;
  targetSlot: string;
};

type ParsedCircuit = {
  nodes: ParsedNode[];
  connections: ParsedConnection[];
  errors: string[];
};

function parseCircuitDSL(script: string): ParsedCircuit {
  const nodes: ParsedNode[] = [];
  const connections: ParsedConnection[] = [];
  const errors: string[] = [];

  const lines = script.split("\n");

  for (let i = 0; i < lines.length; i++) {
    let line = lines[i].trim();
    const lineNum = i + 1;

    // Skip empty lines and comments
    if (!line || line.startsWith("//")) continue;

    // Connection: sourceId.slot -> targetId.slot
    if (line.includes("->")) {
      const arrowPos = line.indexOf("->");
      const left = line.substring(0, arrowPos).trim();
      const right = line.substring(arrowPos + 2).trim();

      const parseSlot = (s: string, isOutput: boolean): [string, string] => {
        const dotPos = s.indexOf(".");
        if (dotPos === -1) {
          return [s, isOutput ? "out" : "in"];
        }
        return [s.substring(0, dotPos).trim(), s.substring(dotPos + 1).trim()];
      };

      const [sourceId, sourceSlot] = parseSlot(left, true);
      const [targetId, targetSlot] = parseSlot(right, false);

      if (sourceId && targetId) {
        connections.push({ sourceId, sourceSlot, targetId, targetSlot });
      } else {
        errors.push(`Line ${lineNum}: Invalid connection`);
      }
    }
    // Node: TYPE id @ x, y [options]
    else if (line.includes("@")) {
      const atPos = line.indexOf("@");
      const beforeAt = line.substring(0, atPos).trim();
      const afterAt = line.substring(atPos + 1).trim();

      const parts = beforeAt.split(/\s+/);
      if (parts.length < 2) {
        errors.push(`Line ${lineNum}: Need TYPE and id`);
        continue;
      }

      const type = parts[0];
      const id = parts[1];

      const coordMatch = afterAt.match(/^\s*(-?\d+)\s*,\s*(-?\d+)/);
      if (!coordMatch) {
        errors.push(`Line ${lineNum}: Invalid coordinates`);
        continue;
      }

      const x = parseInt(coordMatch[1], 10);
      const y = parseInt(coordMatch[2], 10);

      const afterCoords = afterAt.substring(coordMatch[0].length).trim();
      const options = afterCoords
        ? afterCoords.split(/\s+/).filter(Boolean)
        : [];

      nodes.push({ id, type, x, y, options });
    }
  }

  return { nodes, connections, errors };
}

// =============================================================================
// Circuit Logic Evaluator
// =============================================================================

type GateLogic = (inputs: boolean[]) => boolean;

const GATE_LOGIC: Record<string, GateLogic> = {
  AND: (inputs) => inputs.every(Boolean),
  OR: (inputs) => inputs.some(Boolean),
  NOT: (inputs) => !inputs[0],
  NAND: (inputs) => !inputs.every(Boolean),
  NOR: (inputs) => !inputs.some(Boolean),
  XOR: (inputs) => inputs.filter(Boolean).length % 2 === 1,
  XNOR: (inputs) => inputs.filter(Boolean).length % 2 === 0,
  BUF: (inputs) => inputs[0] ?? false,
};

function evaluateCircuit(
  parsed: ParsedCircuit,
  inputStates: Record<string, boolean>,
): {
  nodeOutputs: Record<string, boolean>;
  edgeStates: Record<string, boolean>;
} {
  const nodeOutputs: Record<string, boolean> = {};
  const edgeStates: Record<string, boolean> = {};

  for (const node of parsed.nodes) {
    if (node.type === "In") {
      nodeOutputs[node.id] = inputStates[node.id] ?? false;
    }
  }

  const maxIterations = 100;
  for (let iter = 0; iter < maxIterations; iter++) {
    let changed = false;

    for (const node of parsed.nodes) {
      if (node.type === "In") continue;
      if (node.type === "Out") {
        const incomingConn = parsed.connections.find(
          (c) => c.targetId === node.id && c.targetSlot === "in",
        );
        if (incomingConn && nodeOutputs[incomingConn.sourceId] !== undefined) {
          const newVal = nodeOutputs[incomingConn.sourceId];
          if (nodeOutputs[node.id] !== newVal) {
            nodeOutputs[node.id] = newVal;
            changed = true;
          }
        }
        continue;
      }

      const gateType = node.type.toUpperCase();
      const logic = GATE_LOGIC[gateType];
      if (!logic) continue;

      const incomingConns = parsed.connections.filter(
        (c) => c.targetId === node.id,
      );

      if (gateType === "NOT" || gateType === "BUF") {
        const conn = incomingConns.find(
          (c) => c.targetSlot === "in" || c.targetSlot === "in0",
        );
        if (conn && nodeOutputs[conn.sourceId] !== undefined) {
          const newVal = logic([nodeOutputs[conn.sourceId]]);
          if (nodeOutputs[node.id] !== newVal) {
            nodeOutputs[node.id] = newVal;
            changed = true;
          }
        }
      } else {
        const connA = incomingConns.find(
          (c) => c.targetSlot === "in0" || c.targetSlot === "a",
        );
        const connB = incomingConns.find(
          (c) => c.targetSlot === "in1" || c.targetSlot === "b",
        );

        if (
          connA &&
          connB &&
          nodeOutputs[connA.sourceId] !== undefined &&
          nodeOutputs[connB.sourceId] !== undefined
        ) {
          const newVal = logic([
            nodeOutputs[connA.sourceId],
            nodeOutputs[connB.sourceId],
          ]);
          if (nodeOutputs[node.id] !== newVal) {
            nodeOutputs[node.id] = newVal;
            changed = true;
          }
        }
      }
    }

    if (!changed) break;
  }

  for (const conn of parsed.connections) {
    const edgeId = `${conn.sourceId}.${conn.sourceSlot}->${conn.targetId}.${conn.targetSlot}`;
    edgeStates[edgeId] = nodeOutputs[conn.sourceId] ?? false;
  }

  return { nodeOutputs, edgeStates };
}

// =============================================================================
// Demo Circuits (DSL scripts)
// =============================================================================

const DEMO_CIRCUITS: Record<
  string,
  { name: string; script: string; description: string }
> = {
  "half-adder": {
    name: "Half Adder",
    description: "Adds two bits. XOR=Sum, AND=Carry.",
    script: `// Half Adder
In A @ 60, 40
In B @ 60, 180

XOR xor1 @ 280, 20
AND and1 @ 280, 160

Out Sum @ 480, 80
Out Carry @ 480, 220

A -> xor1.a
B -> xor1.b
A -> and1.a
B -> and1.b
xor1 -> Sum
and1 -> Carry


// Try Editing...`,
  },

  "full-adder": {
    name: "Full Adder",
    description: "Adds 3 bits with carry. Core of ALU.",
    script: `// Full Adder
In A @ 40, 0
In B @ 40, 120
In Cin @ 40, 260

XOR xor1 @ 200, 0
XOR xor2 @ 380, 40
AND and1 @ 200, 160
AND and2 @ 380, 200
OR or1 @ 540, 180

Out Sum @ 560, 40
Out Cout @ 720, 240

A -> xor1.a
B -> xor1.b
xor1 -> xor2.a
Cin -> xor2.b
xor2 -> Sum

A -> and1.a
B -> and1.b
xor1 -> and2.a
Cin -> and2.b
and1 -> or1.a
and2 -> or1.b
or1 -> Cout

// Try Editing...`,
  },

  "mux-2to1": {
    name: "2:1 Multiplexer",
    description: "S=0 outputs D0, S=1 outputs D1.",
    script: `// 2:1 Multiplexer
In D0 @ 40, 20
In D1 @ 40, 140
In S @ 40, 280

NOT not1 @ 180, 240
AND and1 @ 320, 20
AND and2 @ 320, 160
OR or1 @ 480, 90

Out Y @ 660, 150

S -> not1
D0 -> and1.a
not1 -> and1.b
D1 -> and2.a
S -> and2.b
  and1 -> or1.a
and2 -> or1.b
or1 -> Y

// Try Editing...`,
  },

  "mini-cpu": {
    name: "Mini CPU",
    description: "A 1-bit ALU and control unit simulation.",
    script: `// 1-bit Mini CPU
In OpA @ 40, 40
In OpB @ 40, 140
In Ctrl @ 40, 280

AND and_gate @ 220, 20
XOR xor_gate @ 220, 120
NOT ctrl_inv @ 220, 240

AND sel1 @ 400, 60
AND sel2 @ 400, 180
OR final_out @ 580, 120

Out Result @ 760, 140

OpA -> and_gate.a
OpB -> and_gate.b
OpA -> xor_gate.a
OpB -> xor_gate.b

and_gate -> sel1.a
Ctrl -> sel1.b

xor_gate -> sel2.a
ctrl_inv -> sel2.b
Ctrl -> ctrl_inv

sel1 -> final_out.a
sel2 -> final_out.b
final_out -> Result

// Try Editing...`,
  },

  "decoder-2to4": {
    name: "2:4 Decoder",
    description: "Binary to one-hot decoder.",
    script: `// 2-to-4 Decoder
In A0 @ 40, 80
In A1 @ 40, 240

NOT n0 @ 180, 40
NOT n1 @ 180, 200
AND y0 @ 340, 0
AND y1 @ 340, 100
AND y2 @ 340, 200
AND y3 @ 340, 300

Out Y0 @ 520, 60
Out Y1 @ 520, 160
Out Y2 @ 520, 260
Out Y3 @ 520, 360

A0 -> n0
A1 -> n1
n0 -> y0.a
n1 -> y0.b
A0 -> y1.a
n1 -> y1.b
n0 -> y2.a
A1 -> y2.b
A0 -> y3.a
A1 -> y3.b
y0 -> Y0
y1 -> Y1
y2 -> Y2
y3 -> Y3

// Try Editing...`,
  },
};

// =============================================================================
// Dock Components
// =============================================================================

const dockItems = [
  { id: "and", label: "AND", color: "#b8d900" },
  { id: "or", label: "OR", color: "#00d9ff" },
  { id: "not", label: "NOT", color: "#ff6b6b" },
  { id: "nand", label: "NAND", color: "#b8d900" },
  { id: "nor", label: "NOR", color: "#00d9ff" },
  { id: "xor", label: "XOR", color: "#ff9f43" },
  { id: "in", label: "IN", color: "#888" },
  { id: "out", label: "OUT", color: "#888" },
];

function DockItem({
  item,
  isDownload,
}: {
  item: (typeof dockItems)[0];
  isDownload?: boolean;
}) {
  return (
    <div className="group relative flex flex-col items-center">
      <div
        className={`px-3 h-10 rounded-lg flex items-center justify-center cursor-grab active:cursor-grabbing transition-all duration-200 bg-[#1a1a1a] border border-white/5 active:scale-95 shadow-sm shadow-black/20 ${isDownload ? "bg-[#b8d900] hover:border-white/10  hover:bg-[#b8d900]/[0.8] cursor-pointer" : "hover:border-white/10  hover:bg-white/[0.03] cursor-grab"}`}
        draggable
        onDragStart={(e) => {
          e.dataTransfer.setData("application/reactflow", item.id);
          e.dataTransfer.effectAllowed = "move";
        }}
        style={{
          boxShadow: "inset 0 1px 0 rgba(255,255,255,0.02)",
        }}
      >
        <span
          className="text-[14px] font-bold font-mono tracking-tight"
          style={{ color: item.color }}
        >
          {item.label}
        </span>
      </div>

      {/* Tooltip on hover */}
      <div className="absolute -top-8 left-1/2 -translate-x-1/2 opacity-0 group-hover:opacity-100 pointer-events-none transition-all duration-200 translate-y-2 group-hover:translate-y-0">
        <div className="bg-[#0f0f0f] border border-white/10 px-2 py-0.5 rounded text-[9px] font-bold text-white/50 whitespace-nowrap shadow-xl">
          Add {item.label}
        </div>
      </div>
    </div>
  );
}

function Dock() {
  return (
    <div className="absolute bottom-6 left-1/2 -translate-x-1/2 z-20">
      {/* Background Plate / Instruction Layer */}
      <div
        className="relative flex items-center pt-6 px-0.5 pb-0.5 rounded-2xl border border-white/5 shadow-2xl bg-[#0a0a0a]"
        style={{
          background: "linear-gradient(to bottom, #181818, #0e0e0e)",
          boxShadow: "0 20px 50px rgba(0,0,0,0.8), inset 0 1px 0 rgba(255,255,255,0.05)",
        }}
      >
        {/* Instruction Hint */}
        <div className="absolute top-0 left-0 right-0 text-center">
          <span className="text-[12px] font-mono text-white/30 uppercase tracking-[0.3em]">
            Click nodes to toggle
          </span>
        </div>

        {/* Main Floating Dock */}
        <div
          className="flex items-center gap-1.5 p-1.5 rounded-xl border border-white/10 shadow-lg"
          style={{
            background: "linear-gradient(to bottom, #181818, #0e0e0e)",
            boxShadow: "0 4px 20px rgba(0,0,0,0.5), inset 0 1px 0 rgba(255,255,255,0.05)",
          }}
        >
          <div className="flex gap-1">
            {dockItems.map((item) => (
              <DockItem
                key={item.id}
                item={item}
                isDownload={false}
              />
            ))}
            <a
              href="https://github.com/tomlin7/Logicarium/releases"
              className="flex items-center"
            >
              <DockItem
                item={{
                  id: "download",
                  label: "Download",
                  color: "black",
                }}
                isDownload={true}
              />
            </a>
          </div>
        </div>
      </div>
    </div>
  );
}

// =============================================================================
// Node Components
// =============================================================================

function InputNode({ data, id }: any) {
  const isActive = data.value;
  return (
    <div
      className="bg-[#1a1a1a] border w-28 shadow-lg rounded-sm flex flex-col cursor-pointer select-none"
      style={{ borderColor: isActive ? "#b8d900" : "#333" }}
      onClick={() => data.onToggle?.(id)}
    >
      <div className="px-2 py-1 border-b border-[#333]">
        <span className="text-[12px] uppercase tracking-wide text-white">
          {data.label}
        </span>
      </div>
      <div className="px-2 py-1.5 flex items-center justify-between">
        <span
          className={`text-[10px] font-bold ${isActive ? "text-[#b8d900]" : "text-[#555]"}`}
        >
          {isActive ? "1" : "0"}
        </span>
        <span className="text-[10px] text-[#555] mr-2">BIT</span>
      </div>
      <Handle
        type="source"
        position={Position.Right}
        id="out"
        style={{
          width: 7,
          height: 7,
          borderRadius: "50%",
          border: `1px solid ${isActive ? "#b8d900" : "#555"}`,
          background: isActive ? "#b8d900" : "#080808",
          right: -3,
          top: "50%",
        }}
      />
    </div>
  );
}

function GateNode({ data }: any) {
  const { inputA, inputB, output } = data;
  return (
    <div className="bg-[#1a1a1a] border border-[#333] w-24 shadow-lg rounded-sm flex flex-col">
      <div className="px-2 py-1 border-b border-[#333]">
        <span className="text-[10px] uppercase tracking-wide text-white">
          {data.label}
        </span>
      </div>
      <div
        className="px-2 py-1 flex flex-col gap-0.5"
        style={{ minHeight: 38 }}
      >
        <div className="flex justify-between">
          <span className="text-[10px] text-[#555]">A</span>
          <span
            className={`text-[10px] ${inputA ? "text-[#b8d900]" : "text-[#444]"}`}
          >
            {inputA ? "1" : "0"}
          </span>
        </div>
        <div className="flex justify-between">
          <span className="text-[10px] text-[#555]">B</span>
          <span
            className={`text-[10px] ${output ? "text-[#b8d900]" : "text-[#444]"}`}
          >
            Q:{output ? "1" : "0"}
          </span>
        </div>
      </div>
      <Handle
        type="target"
        position={Position.Left}
        id="a"
        style={{
          width: 6,
          height: 6,
          borderRadius: "50%",
          border: `1px solid ${inputA ? "#b8d900" : "#444"}`,
          background: inputA ? "#b8d900" : "#080808",
          left: -3,
          top: 26,
        }}
      />
      <Handle
        type="target"
        position={Position.Left}
        id="b"
        style={{
          width: 6,
          height: 6,
          borderRadius: "50%",
          border: `1px solid ${inputB ? "#b8d900" : "#444"}`,
          background: inputB ? "#b8d900" : "#080808",
          left: -3,
          top: 46,
        }}
      />
      <Handle
        type="source"
        position={Position.Right}
        id="out"
        style={{
          width: 6,
          height: 6,
          borderRadius: "50%",
          border: `1px solid ${output ? "#b8d900" : "#444"}`,
          background: output ? "#b8d900" : "#080808",
          right: -3,
          top: 46,
        }}
      />
    </div>
  );
}

function NotGateNode({ data }: any) {
  const { input, output } = data;
  return (
    <div className="bg-[#1a1a1a] border border-[#333] w-20 shadow-lg rounded-sm flex flex-col">
      <div className="px-2 py-1 border-b border-[#333]">
        <span className="text-[8px] uppercase tracking-wide text-white">
          {data.label}
        </span>
      </div>
      <div className="px-2 py-1 flex justify-between">
        <span
          className={`text-[10px] ${input ? "text-[#b8d900]" : "text-[#444]"}`}
        >
          {input ? "1" : "0"}
        </span>
        <span
          className={`text-[10px] ${output ? "text-[#b8d900]" : "text-[#444]"}`}
        >
          {output ? "1" : "0"}
        </span>
      </div>
      <Handle
        type="target"
        position={Position.Left}
        id="in"
        style={{
          width: 6,
          height: 6,
          borderRadius: "50%",
          border: `1px solid ${input ? "#b8d900" : "#444"}`,
          background: input ? "#b8d900" : "#080808",
          left: -3,
          top: "50%",
        }}
      />
      <Handle
        type="source"
        position={Position.Right}
        id="out"
        style={{
          width: 6,
          height: 6,
          borderRadius: "50%",
          border: `1px solid ${output ? "#b8d900" : "#444"}`,
          background: output ? "#b8d900" : "#080808",
          right: -3,
          top: "50%",
        }}
      />
    </div>
  );
}

function OutputNode({ data }: any) {
  const isActive = data.value;
  return (
    <div
      className="bg-[#1a1a1a] border w-28 shadow-lg rounded-sm flex flex-col"
      style={{ borderColor: isActive ? "#b8d900" : "#333" }}
    >
      <div
        className="px-2 py-1 border-b border-[#333]"
        style={{ background: isActive ? "rgba(184,217,0,0.1)" : "transparent" }}
      >
        <span className="text-[10px] uppercase tracking-wide text-white">
          {data.label}
        </span>
      </div>
      <div className="px-2 py-1.5 flex items-center justify-between">
        <span className="text-[10px] text-[#555] ml-1">OUT</span>
        <span
          className={`text-[10px] font-bold ${isActive ? "text-[#b8d900]" : "text-[#555]"}`}
        >
          {isActive ? "1" : "0"}
        </span>
      </div>
      <Handle
        type="target"
        position={Position.Left}
        id="in"
        style={{
          width: 6,
          height: 6,
          borderRadius: "50%",
          border: `1px solid ${isActive ? "#b8d900" : "#444"}`,
          background: isActive ? "#b8d900" : "#080808",
          left: -3,
          top: "50%",
        }}
      />
    </div>
  );
}

function CustomEdge({ id, sourceX, sourceY, targetX, targetY, data }: any) {
  const [edgePath] = getBezierPath({
    sourceX,
    sourceY,
    targetX,
    targetY,
    sourcePosition: Position.Right,
    targetPosition: Position.Left,
  });
  const isActive = data?.active;
  return (
    <path
      id={id}
      d={edgePath}
      fill="none"
      stroke={isActive ? "#b8d900" : "#333"}
      strokeWidth={isActive ? 1.5 : 1}
      style={{ filter: isActive ? "drop-shadow(0 0 2px #b8d900)" : "none" }}
    />
  );
}

const nodeTypes = {
  inputNode: InputNode,
  gateNode: GateNode,
  notGateNode: NotGateNode,
  outputNode: OutputNode,
};
const edgeTypes = { custom: CustomEdge };

// =============================================================================
// Interactive Script Editor
// =============================================================================

function ScriptEditor({
  script,
  onChange,
  errors,
}: {
  script: string;
  onChange: (s: string) => void;
  errors: string[];
}) {
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const highlightRef = useRef<HTMLDivElement>(null);

  const syncScroll = () => {
    if (textareaRef.current && highlightRef.current) {
      highlightRef.current.scrollTop = textareaRef.current.scrollTop;
      highlightRef.current.scrollLeft = textareaRef.current.scrollLeft;
    }
  };

  const highlightedLines = useMemo(() => {
    return script.split("\n").map((line, i) => {
      const trimmed = line.trim();
      if (trimmed.startsWith("//")) {
        return (
          <div key={i} className="text-[#444]">
            {line || " "}
          </div>
        );
      }
      if (trimmed.includes("->")) {
        return (
          <div key={i} className="text-[#666]">
            {line}
          </div>
        );
      }
      if (trimmed.startsWith("In ") || trimmed.startsWith("Out ")) {
        const spaceIdx = line.indexOf(" ");
        return (
          <div key={i}>
            <span className="text-[#888]">{line.substring(0, spaceIdx)}</span>
            <span className="text-[#666]">{line.substring(spaceIdx)}</span>
          </div>
        );
      }
      if (trimmed.match(/^(AND|OR|NOT|NAND|NOR|XOR|XNOR)\s/i)) {
        const spaceIdx = line.search(/\s/);
        return (
          <div key={i}>
            <span className="text-[#b8d900]">
              {line.substring(0, spaceIdx)}
            </span>
            <span className="text-[#666]">{line.substring(spaceIdx)}</span>
          </div>
        );
      }
      return (
        <div key={i} className="text-[#666]">
          {line || " "}
        </div>
      );
    });
  }, [script]);

  return (
    <div className="flex-1 flex flex-col overflow-hidden ">
      <div className="relative flex-1 overflow-hidden">
        {/* Syntax highlighted background */}
        <div
          ref={highlightRef}
          className="absolute inset-0 p-3 font-mono text-[14px] leading-[1.6] whitespace-pre overflow-auto pointer-events-none"
          style={{ background: "#0a0a0a" }}
        >
          {highlightedLines}
        </div>
        {/* Actual textarea */}
        <textarea
          ref={textareaRef}
          value={script}
          onChange={(e) => onChange(e.target.value)}
          onScroll={syncScroll}
          className="absolute inset-0 w-full h-full p-3 font-mono text-[14px] leading-[1.6] bg-transparent text-transparent caret-[#b8d900] resize-none outline-none"
          spellCheck={false}
          style={{ caretColor: "#b8d900" }}
        />
      </div>
      {errors.length > 0 && (
        <div className="p-2 bg-[#1a0a0a] border-t border-[#400]">
          {errors.map((e, i) => (
            <div key={i} className="text-[8px] text-[#f66]">
              {e}
            </div>
          ))}
        </div>
      )}
    </div>
  );
}


// =============================================================================
// Properties Panel with Editor
// =============================================================================

function PropertiesPanel({
  script,
  onChange,
  errors,
  description,
}: {
  script: string;
  onChange: (s: string) => void;
  errors: string[];
  description: string;
}) {
  return (
    <aside className="bg-[#0b0b0b] border-l border-white/5 flex flex-col overflow-hidden w-[320px] font-mono">
      <div className="p-3 border-b border-white/5 flex items-center justify-between">
        <span className="text-[14px] font-bold uppercase tracking-wider text-white/40">
          Script Editor
        </span>
      </div>
      <ScriptEditor script={script} onChange={onChange} errors={errors} />
      <div className="p-4 border-t border-white/5">
        <div className="text-[14px] font-bold uppercase tracking-wider text-white/40 mb-3">
          About
        </div>
        <p className="text-[14px] text-white/30 leading-relaxed font-medium">
          {description}
        </p>
      </div>
    </aside>
  );
}

// =============================================================================
// Top Bar
// =============================================================================

function TopBar() {
  return (
    <header className="fixed top-0 left-0 right-0 h-14 border-b border-white/5 bg-[#080808]/50 backdrop-blur-md z-50 font-mono">
      <div className="max-w-5xl mx-auto h-full flex items-center justify-between px-6">
        <Link href="/" className="flex items-center gap-2.5 group">
          <div className="relative">
            <img src="/logo.gif" alt="Logicarium Logo" className="w-8 h-8 rounded-md relative z-10" />
            <div className="absolute inset-0 bg-[#b8d900]/20 blur-md rounded-full group-hover:bg-[#b8d900]/40 transition-all" />
          </div>
          <span className="text-[14px] font-bold tracking-tight text-white group-hover:text-[#b8d900] transition-colors">Logicarium</span>
        </Link>
        <nav className="flex items-center gap-6">
          <Link href="#features" className="text-[14px] font-medium text-white/40 hover:text-white transition-colors">Features</Link>
          <Link href="/docs" className="text-[14px] font-medium text-white/40 hover:text-white transition-colors">Docs</Link>
          <Link href="https://github.com/tomlin7/Logicarium" className="text-[14px] font-medium text-white/40 hover:text-white transition-colors">GitHub ↗</Link>
          {/* <a href="https://github.com/tomlin7/Logicarium" className="text-[14px] font-medium text-white/40 hover:text-white transition-colors">Community ↗</a> */}
        </nav>
        <a
          href="https://github.com/tomlin7/Logicarium/releases"
          className="bg-[#f0a0ff] text-black px-3 py-1 rounded-md text-[14px] font-bold border-b-2 border-[#d080ff] hover:brightness-105 active:translate-y-[1px] active:border-b-0 transition-all flex items-center gap-1.5"
        >
          <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M12 4v12M12 16l-4-4M12 16l4-4M4 20h16" />
          </svg>
          Download
        </a>
      </div>
    </header>
  );
}

function HeroSection() {
  return (
    <div className="pt-24 pb-12 max-w-2xl mx-auto px-6 text-center md:text-left">
      {/* <div className="mb-6 flex justify-center md:justify-start">
        <div className="relative">
          <img src="/logo.gif" alt="Logicarium Logo" className="w-16 h-16 rounded-xl relative z-10 shadow-2xl" />
          <div className="absolute inset-0 bg-[#b8d900]/10 blur-xl rounded-full" />
        </div>
      </div> */}
      <h1 className="text-[24px] font-bold text-white mb-4 font-mono">Logicarium</h1>

      <p className="text-[14px] text-white/90 mb-6 leading-relaxed max-w-xl mx-auto md:mx-0 font-mono">
        Run high-performance logic simulations directly in your browser without breaking your flow.
      </p>

      <ul className="space-y-3 mb-8 text-[14px] text-white/60 text-left max-w-md mx-auto md:mx-0 font-medium font-mono">
        <li className="flex items-start gap-4">
          <div className="mt-1.5 w-1 h-1 bg-white/10 shrink-0" />
          <span><strong className="text-white/80">Local</strong>, because simulations need direct access to your compute resources.</span>
        </li>
        <li className="flex items-start gap-4">
          <div className="mt-1.5 w-1 h-1 bg-white/10 shrink-0" />
          <span><strong className="text-white/80">Native</strong>, because it should actually feel like it belongs on your desktop.</span>
        </li>
        <li className="flex items-start gap-4">
          <div className="mt-1.5 w-1 h-1 bg-white/10 shrink-0" />
          <span><strong className="text-white/80">Flexible</strong>, because some circuits take seconds and others take hours to perfect.</span>
        </li>
      </ul>

      <p className="text-[14px] text-white/40 mb-6 font-mono">
        Latest: <span className="text-white/80">v1.0.4</span> ・ <span className="text-[#b8d900]">5</span> stars ・ MIT License ・ build: <span className="text-[#b8d900]">passing</span>
      </p>

      <div className="flex flex-wrap gap-3 justify-center md:justify-start">
        <a
          href="https://github.com/tomlin7/Logicarium/releases"
          className="group relative overflow-hidden bg-[#f0a0ff] text-black px-3 py-2 rounded-md border-b-3 border-[#c766ff] text-[14px] font-bold font-mono hover:brightness-105 active:translate-y-[1px] active:border-b-0 transition-all flex items-center gap-2"
        >
          <div className="absolute inset-0 -translate-x-full group-hover:translate-x-full transition-transform duration-1000 bg-gradient-to-r from-transparent via-white/10 to-white/10"></div>
          <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M12 4v12M12 16l-4-4M12 16l4-4M4 20h16" />
          </svg>
          Download for Windows
        </a>
        <button className="bg-transaparent text-white/40 px-3 py-2 rounded-md border-[1px] border-b-3 border-[#ffffff0a] text-[14px] font-bold font-mono hover:bg-white/5 hover:text-white transition-all flex items-center gap-2">
          <svg className="w-3.5 h-3.5" fill="currentColor" viewBox="0 0 24 24"><path d="M8 5v14l11-7z" /></svg>
          <a href="https://github.com/tomlin7/Logicarium">View Source Code</a>
        </button>
      </div>
      <button className="mt-4 text-[12px] font-mono text-white/20 hover:text-white/40 transition-colors"><a href="https://github.com/tomlin7/Logicarium/releases">View all versions</a></button>
    </div>
  );
}

function ProductShowcase({
  children,
  currentDemoId,
  onSelectDemo
}: {
  children: React.ReactNode;
  currentDemoId: string;
  onSelectDemo: (id: string) => void;
}) {
  return (
    <div id="showcase" className="max-w-7xl mx-auto px-6 mb-16 font-mono">
      <div className="bg-[#0f0f0f] border border-white/5 rounded-lg overflow-hidden shadow-2xl">
        {/* Creative Segmented Selector */}
        <div className="h-12 border-b border-white/5 bg-[#0a0a0a] flex items-center justify-between px-4">
          <div className="flex gap-1.5 opacity-20">
            <div className="w-2.5 h-2.5 rounded-full bg-white" />
            <div className="w-2.5 h-2.5 rounded-full bg-white" />
            <div className="w-2.5 h-2.5 rounded-full bg-white" />
          </div>

          <nav className="flex items-center gap-1 bg-white/[0.03] p-1 rounded-lg border border-white/5">
            {Object.entries(DEMO_CIRCUITS).map(([id, { name }]) => (
              <button
                key={id}
                onClick={() => onSelectDemo(id)}
                className={`px-4 py-1 rounded-md text-[14px] font-bold font-mono transition-all duration-200 relative ${currentDemoId === id
                  ? "text-[#b8d900] bg-white/[0.07] shadow-[inset_0_0_10px_rgba(184,217,0,0.05)]"
                  : "text-white/20 hover:text-white/50"
                  }`}
              >
                {name}
              </button>
            ))}
          </nav>

          <div className="text-[12px] font-mono font-bold text-white/10 uppercase tracking-[0.2em] hidden md:block">
            Logicarium
          </div>
        </div>

        <div className="h-[800px] flex overflow-hidden">
          {children}
        </div>
      </div>
    </div>
  );
}

function ProductivitySection() {
  return (
    <section id="features" className="max-w-2xl mx-auto px-6 py-16 border-t border-white/5 font-mono">
      <h2 className="text-[18px] font-bold text-white mb-6">Built for your productive work</h2>
      <ul className="space-y-4 text-[14px] text-white/40 leading-relaxed font-medium">
        <li className="flex items-start gap-4">
          <div className="mt-1.5 w-1 h-1 bg-[#b8d900] shrink-0" />
          <span><strong className="text-white/90">Design with code.</strong> Describe your circuit architecture in plain text DSL and watch it render instantly.</span>
        </li>
        <li className="flex items-start gap-4">
          <div className="mt-1.5 w-1 h-1 bg-[#b8d900] shrink-0" />
          <span><strong className="text-white/90">Real-time simulation.</strong> Watch logic signals propagate as you edit, with zero latency.</span>
        </li>
        <li className="flex items-start gap-4">
          <div className="mt-1.5 w-1 h-1 bg-[#b8d900] shrink-0" />
          <span><strong className="text-white/90">Verify with confidence.</strong> Every change is tracked, highlighted, and validated by the engine.</span>
        </li>
      </ul>
    </section>
  );
}

function BenchmarksTable() {
  const models = [
    { name: "6502 Core", gates: "3,510", throughput: "125 MHz", memory: "1.2 MB", parse: "2ms" },
    { name: "16-bit ALU", gates: "12,402", throughput: "42 MHz", memory: "4.5 MB", parse: "8ms" },
    { name: "RISC-V Mini", gates: "45,209", throughput: "12 MHz", memory: "12.1 MB", parse: "24ms" },
    { name: "SRAM 1KB", gates: "124,000", throughput: "4.5 MHz", memory: "32.5 MB", parse: "85ms" },
    { name: "Gate Array", gates: "450,000", throughput: "1.2 MHz", memory: "112.4 MB", parse: "210ms" },
  ];

  return (
    <section id="benchmarks" className="max-w-2xl mx-auto px-6 py-16 border-t border-white/5 font-mono">
      <h2 className="text-[18px] font-bold text-white mb-2">Engine Benchmarks</h2>
      <p className="text-[14px] text-white/30 mb-6">Measured on WASM engine (Browser). C++ Native performance is ~10x higher.</p>

      <div className="overflow-x-auto">
        <table className="w-full text-left border-collapse text-[14px]">
          <thead>
            <tr className="border-b border-white/10 text-white/30">
              <th className="pb-3 font-medium">Circuit Model</th>
              <th className="pb-3 font-medium text-right">Gates</th>
              <th className="pb-3 font-medium text-right">Sim Speed</th>
              <th className="pb-3 font-medium text-right">Memory</th>
              <th className="pb-3 font-medium text-right">Parse</th>
            </tr>
          </thead>
          <tbody className="text-white/60">
            {models.map((m) => (
              <tr key={m.name} className="border-b border-white/5 hover:bg-white/[0.02] transition-colors">
                <td className="py-3 font-medium text-white/80">{m.name}</td>
                <td className="py-3 text-right">{m.gates}</td>
                <td className="py-3 text-right">{m.throughput}</td>
                <td className="py-3 text-right">{m.memory}</td>
                <td className="py-3 text-right">{m.parse}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}

function FAQSection() {
  return (
    <section className="max-w-2xl mx-auto px-6 py-16 border-t border-white/5 font-mono">
      <h2 className="text-[18px] font-bold text-white mb-6">Frequently asked questions</h2>
      <div className="space-y-4">
        {[
          { q: "Which circuit types does Logicarium support?", a: "Supports combinatorial, sequential, and hybrid digital logic circuits up to 10M gates." },
          { q: "How do I use the DSL?", a: "Use the built-in editor to define components. Syntax is similar to Verilog but optimized for rapid drafting." },
          { q: "What is Logicarium built for?", a: "Academic research, rapid prototyping, and digital logic education where performance matters." }
        ].map((item, i) => (
          <details key={i} className="group border-b border-white/5 pb-4">
            <summary className="flex items-center justify-between cursor-pointer list-none text-[14px] text-white/80 hover:text-white transition-colors">
              <span>{item.q}</span>
              <svg className="w-4 h-4 text-white/20 group-open:rotate-180 transition-transform" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
              </svg>
            </summary>
            <p className="mt-3 text-[14px] text-white/40 leading-relaxed">{item.a}</p>
          </details>
        ))}
      </div>

      <div className="mt-12 py-12 text-center border-t border-white/5">
        <p className="text-[18px] text-white/80 mb-5">Ready to design faster?</p>
        <a
          href="https://github.com/tomlin7/Logicarium/releases"
          className="inline-block bg-[#f0a0ff] text-black px-6 py-2 rounded-md text-[14px] font-bold border-b-2 border-[#d080ff] hover:brightness-105 active:translate-y-[1px] active:border-b-0 transition-all shadow-xl shadow-pink-500/5"
        >
          Download
        </a>
      </div>
    </section>
  );
}

// =============================================================================
// Circuit View (with ReactFlow)
// =============================================================================

function CircuitView({
  parsed,
  inputStates,
  nodeOutputs,
  edgeStates,
  onToggle,
}: {
  parsed: ParsedCircuit;
  inputStates: Record<string, boolean>;
  nodeOutputs: Record<string, boolean>;
  edgeStates: Record<string, boolean>;
  onToggle: (id: string) => void;
}) {
  const { fitView } = useReactFlow();

  const flowNodes = useMemo(() => {
    return parsed.nodes.map((node) => {
      if (node.type === "In") {
        return {
          id: node.id,
          type: "inputNode",
          position: { x: node.x, y: node.y },
          data: { label: node.id, value: inputStates[node.id], onToggle },
        };
      }
      if (node.type === "Out") {
        return {
          id: node.id,
          type: "outputNode",
          position: { x: node.x, y: node.y },
          data: { label: node.id, value: nodeOutputs[node.id] },
        };
      }
      if (
        node.type.toUpperCase() === "NOT" ||
        node.type.toUpperCase() === "BUF"
      ) {
        const inConn = parsed.connections.find((c) => c.targetId === node.id);
        return {
          id: node.id,
          type: "notGateNode",
          position: { x: node.x, y: node.y },
          data: {
            label: node.type,
            input: inConn ? nodeOutputs[inConn.sourceId] : false,
            output: nodeOutputs[node.id],
          },
        };
      }
      const connA = parsed.connections.find(
        (c) =>
          c.targetId === node.id &&
          (c.targetSlot === "a" || c.targetSlot === "in0"),
      );
      const connB = parsed.connections.find(
        (c) =>
          c.targetId === node.id &&
          (c.targetSlot === "b" || c.targetSlot === "in1"),
      );
      return {
        id: node.id,
        type: "gateNode",
        position: { x: node.x, y: node.y },
        data: {
          label: node.type,
          inputA: connA ? nodeOutputs[connA.sourceId] : false,
          inputB: connB ? nodeOutputs[connB.sourceId] : false,
          output: nodeOutputs[node.id],
        },
      };
    });
  }, [parsed, inputStates, nodeOutputs, onToggle]);

  const flowEdges = useMemo(() => {
    return parsed.connections.map((conn) => {
      const edgeId = `${conn.sourceId}.${conn.sourceSlot}->${conn.targetId}.${conn.targetSlot}`;
      return {
        id: edgeId,
        source: conn.sourceId,
        sourceHandle: conn.sourceSlot,
        target: conn.targetId,
        targetHandle: conn.targetSlot,
        type: "custom",
        data: { active: edgeStates[edgeId] },
      };
    });
  }, [parsed, edgeStates]);

  const [nodes, setNodes, onNodesChange] = useNodesState(flowNodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(flowEdges);

  useEffect(() => {
    setNodes(flowNodes);
    setEdges(flowEdges);
  }, [flowNodes, flowEdges, setNodes, setEdges]);

  // Only fit view when the number of nodes or their IDs change (i.e. script changed)
  const nodeSignature = useMemo(() =>
    parsed.nodes.map(n => n.id).join(','),
    [parsed.nodes]);

  useEffect(() => {
    setTimeout(() => fitView({ padding: 0.15 }), 50);
  }, [nodeSignature, fitView]);

  return (
    <ReactFlow
      nodes={nodes}
      edges={edges}
      onNodesChange={onNodesChange}
      onEdgesChange={onEdgesChange}
      nodeTypes={nodeTypes}
      edgeTypes={edgeTypes}
      fitViewOptions={{ padding: 0.15 }}
      proOptions={{ hideAttribution: true }}
      minZoom={0.3}
      maxZoom={2}
    >
      <Background
        variant={BackgroundVariant.Cross}
        gap={32}
        color="#1a1a1a"
      />
    </ReactFlow>
  );
}

// =============================================================================
// Main Component
// =============================================================================

function HomePageInner() {
  const [currentDemoId, setCurrentDemoId] = useState("half-adder");
  const [script, setScript] = useState(DEMO_CIRCUITS["half-adder"].script);
  const [description, setDescription] = useState(
    DEMO_CIRCUITS["half-adder"].description,
  );

  const handleSelectDemo = useCallback((id: string) => {
    setCurrentDemoId(id);
    setScript(DEMO_CIRCUITS[id].script);
    setDescription(DEMO_CIRCUITS[id].description);
  }, []);

  const parsed = useMemo(() => parseCircuitDSL(script), [script]);

  const [inputStates, setInputStates] = useState<Record<string, boolean>>({});

  // Reset inputs when parsed nodes change
  useEffect(() => {
    const states: Record<string, boolean> = {};
    parsed.nodes
      .filter((n) => n.type === "In")
      .forEach((n) => {
        states[n.id] = inputStates[n.id] ?? false;
      });
    setInputStates(states);
  }, [parsed.nodes.map((n) => n.id).join(",")]);

  const { nodeOutputs, edgeStates } = useMemo(
    () => evaluateCircuit(parsed, inputStates),
    [parsed, inputStates],
  );

  const handleToggle = useCallback((id: string) => {
    setInputStates((prev) => ({ ...prev, [id]: !prev[id] }));
  }, []);

  return (
    <div className="min-h-screen bg-[#080808] text-white font-sans selection:bg-[#b8d900] selection:text-black">
      <TopBar />

      <main className="pb-24">
        <HeroSection />

        <ProductShowcase
          currentDemoId={currentDemoId}
          onSelectDemo={handleSelectDemo}
        >
          <div className="relative flex-1 bg-[#080808]">
            <CircuitView
              parsed={parsed}
              inputStates={inputStates}
              nodeOutputs={nodeOutputs}
              edgeStates={edgeStates}
              onToggle={handleToggle}
            />
            <Dock />
          </div>
          <PropertiesPanel
            script={script}
            onChange={setScript}
            errors={parsed.errors}
            description={description}
          />
        </ProductShowcase>

        <ProductivitySection />
        <BenchmarksTable />
        <FAQSection />
      </main>

      <footer className="py-12 px-6 border-t border-white/5 font-mono">
        <div className="max-w-5xl mx-auto flex flex-wrap justify-center gap-x-8 gap-y-4 text-[14px] text-white/30 font-medium">
          <Link href="#" className="hover:text-white transition-colors">Terms</Link>
          <Link href="#" className="hover:text-white transition-colors">Privacy</Link>
          <a href="https://github.com/tomlin7/Logicarium" className="hover:text-white transition-colors">Community ↗</a>
          <a href="https://github.com/tomlin7/Logicarium" className="hover:text-white transition-colors">GitHub ↗</a>
          {/* <a href="#" className="hover:text-white transition-colors cursor-help">X (Twitter) ↗</a> */}
        </div>
      </footer>
    </div>
  );
}

export default function HomePage() {
  return (
    <ReactFlowProvider>
      <HomePageInner />
    </ReactFlowProvider>
  );
}
