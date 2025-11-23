"""
NOMAIN Quantum Library
Provides quantum computing primitives and operations for NOMAIN language
"""

import numpy as np
from typing import List, Tuple, Optional


# ============================================================================
# Quantum State Representation
# ============================================================================

class Qubit:
    """
    Represents a single qubit in a quantum system.
    State stored as complex amplitudes [alpha, beta] where |psi> = alpha|0> + beta|1>
    """
    def __init__(self, alpha: complex = 1.0, beta: complex = 0.0):
        # Normalize the state
        norm = np.sqrt(abs(alpha)**2 + abs(beta)**2)
        self.state = np.array([alpha/norm, beta/norm], dtype=complex)
    
    def __repr__(self):
        return f"Qubit(|0>: {self.state[0]:.3f}, |1>: {self.state[1]:.3f})"
    
    def measure(self) -> int:
        """Measure the qubit, collapsing to |0> or |1>"""
        prob_0 = abs(self.state[0])**2
        result = 0 if np.random.random() < prob_0 else 1
        
        # Collapse state
        self.state = np.array([1.0, 0.0] if result == 0 else [0.0, 1.0], dtype=complex)
        return result
    
    def get_bloch_coords(self) -> Tuple[float, float, float]:
        """Get Bloch sphere coordinates (x, y, z) for visualization"""
        alpha, beta = self.state
        
        # Bloch sphere coordinates
        theta = 2 * np.arccos(abs(alpha))
        phi = np.angle(beta) - np.angle(alpha)
        
        x = np.sin(theta) * np.cos(phi)
        y = np.sin(theta) * np.sin(phi)
        z = np.cos(theta)
        
        return (float(x), float(y), float(z))


class QuantumRegister:
    """
    Represents multiple qubits in a quantum register.
    State stored as full state vector (2^n dimensional)
    """
    def __init__(self, num_qubits: int):
        self.num_qubits = num_qubits
        self.dim = 2 ** num_qubits
        # Initialize to |00...0>
        self.state = np.zeros(self.dim, dtype=complex)
        self.state[0] = 1.0
    
    def __repr__(self):
        return f"QuantumRegister({self.num_qubits} qubits, state vector size: {self.dim})"
    
    def measure_all(self) -> List[int]:
        """Measure all qubits, returning list of 0s and 1s"""
        # Calculate probabilities
        probabilities = np.abs(self.state)**2
        
        # Sample from probability distribution
        result_index = np.random.choice(self.dim, p=probabilities)
        
        # Convert to binary representation
        result = [(result_index >> i) & 1 for i in range(self.num_qubits)]
        
        # Collapse state
        self.state = np.zeros(self.dim, dtype=complex)
        self.state[result_index] = 1.0
        
        return result


# ============================================================================
# Quantum Gates
# ============================================================================

class QuantumGate:
    """Base class for quantum gates"""
    def __init__(self, name: str, matrix: np.ndarray):
        self.name = name
        self.matrix = matrix
    
    def __repr__(self):
        return f"Gate({self.name})"


# Common single-qubit gates
GATE_HADAMARD = QuantumGate("H", np.array([
    [1,  1],
    [1, -1]
], dtype=complex) / np.sqrt(2))

GATE_PAULI_X = QuantumGate("X", np.array([
    [0, 1],
    [1, 0]
], dtype=complex))

GATE_PAULI_Y = QuantumGate("Y", np.array([
    [0, -1j],
    [1j, 0]
], dtype=complex))

GATE_PAULI_Z = QuantumGate("Z", np.array([
    [1,  0],
    [0, -1]
], dtype=complex))

GATE_S = QuantumGate("S", np.array([
    [1, 0],
    [0, 1j]
], dtype=complex))

GATE_T = QuantumGate("T", np.array([
    [1, 0],
    [0, np.exp(1j * np.pi / 4)]
], dtype=complex))

# Two-qubit gates
GATE_CNOT = QuantumGate("CNOT", np.array([
    [1, 0, 0, 0],
    [0, 1, 0, 0],
    [0, 0, 0, 1],
    [0, 0, 1, 0]
], dtype=complex))


# ============================================================================
# Quantum Operations
# ============================================================================

def apply_gate_single(qubit: Qubit, gate: QuantumGate) -> Qubit:
    """Apply a single-qubit gate to a qubit"""
    qubit.state = gate.matrix @ qubit.state
    return qubit


def apply_gate_register(register: QuantumRegister, gate: QuantumGate, 
                       target_qubit: int, control_qubit: Optional[int] = None) -> QuantumRegister:
    """
    Apply a gate to a quantum register.
    For single-qubit gates, specify target_qubit.
    For two-qubit gates (like CNOT), specify both control_qubit and target_qubit.
    """
    if control_qubit is None:
        # Single-qubit gate - construct full operator
        n = register.num_qubits
        full_operator = 1
        
        for i in range(n):
            if i == target_qubit:
                full_operator = np.kron(full_operator, gate.matrix)
            else:
                full_operator = np.kron(full_operator, np.eye(2))
        
        register.state = full_operator @ register.state
    else:
        # Two-qubit gate (like CNOT)
        # For simplicity, assume gate is CNOT for now
        # Full implementation would need more sophisticated tensor product construction
        register.state = gate.matrix @ register.state
    
    return register


def create_bell_state() -> QuantumRegister:
    """Create a Bell state (maximally entangled state)"""
    reg = QuantumRegister(2)
    apply_gate_register(reg, GATE_HADAMARD, 0)
    apply_gate_register(reg, GATE_CNOT, 1, 0)
    return reg


# ============================================================================
# NOMAIN Built-in Function Wrappers
# ============================================================================

def builtin_create_qubit(alpha: float = 1.0, beta: float = 0.0) -> Qubit:
    """NOMAIN: CreateQubit() - Create a new qubit"""
    return Qubit(complex(alpha), complex(beta))


def builtin_create_qubits(num: int) -> QuantumRegister:
    """NOMAIN: CreateQubits(n) - Create n qubits in a register"""
    return QuantumRegister(num)


def builtin_apply_gate(gate_name: str, qubit, control=None):
    """NOMAIN: ApplyGate(gate, qubit, [control]) - Apply quantum gate"""
    gate_map = {
        'HADAMARD': GATE_HADAMARD,
        'H': GATE_HADAMARD,
        'X': GATE_PAULI_X,
        'Y': GATE_PAULI_Y,
        'Z': GATE_PAULI_Z,
        'S': GATE_S,
        'T': GATE_T,
        'CNOT': GATE_CNOT,
    }
    
    gate = gate_map.get(gate_name.upper())
    if not gate:
        raise ValueError(f"Unknown gate: {gate_name}")
    
    if isinstance(qubit, Qubit):
        return apply_gate_single(qubit, gate)
    elif isinstance(qubit, QuantumRegister):
        target = 0  # Default target
        control_idx = None if control is None else 0
        return apply_gate_register(qubit, gate, target, control_idx)
    else:
        raise TypeError(f"Cannot apply gate to {type(qubit)}")


def builtin_measure(qubit) -> int:
    """NOMAIN: Measure(qubit) - Measure a qubit"""
    if isinstance(qubit, Qubit):
        return qubit.measure()
    elif isinstance(qubit, QuantumRegister):
        return qubit.measure_all()
    else:
        raise TypeError(f"Cannot measure {type(qubit)}")


def builtin_get_state(qubit) -> str:
    """NOMAIN: GetState(qubit) - Get string representation of quantum state"""
    if isinstance(qubit, Qubit):
        alpha, beta = qubit.state
        return f"{alpha:.3f}|0> + {beta:.3f}|1>"
    elif isinstance(qubit, QuantumRegister):
        # Show first few basis states with significant amplitude
        result = []
        for i, amp in enumerate(qubit.state):
            if abs(amp) > 0.01:  # Threshold for display
                binary = format(i, f'0{qubit.num_qubits}b')
                result.append(f"{amp:.3f}|{binary}>")
        return " + ".join(result[:10])  # Limit to first 10 terms
    else:
        return str(qubit)


# ============================================================================
# Circuit Representation (for future use)
# ============================================================================

class QuantumCircuit:
    """Represents a quantum circuit with gates and measurements"""
    def __init__(self, num_qubits: int):
        self.num_qubits = num_qubits
        self.gates = []  # List of (gate, target, control) tuples
        self.register = QuantumRegister(num_qubits)
    
    def add_gate(self, gate: QuantumGate, target: int, control: Optional[int] = None):
        """Add a gate to the circuit"""
        self.gates.append((gate, target, control))
    
    def run(self) -> QuantumRegister:
        """Execute the circuit"""
        for gate, target, control in self.gates:
            apply_gate_register(self.register, gate, target, control)
        return self.register
    
    def __repr__(self):
        return f"QuantumCircuit({self.num_qubits} qubits, {len(self.gates)} gates)"
