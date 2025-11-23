"""
NOMAIN Visualization Library
Provides real-time quantum visualization capabilities
"""

import numpy as np
from typing import List, Tuple, Optional

# Will use matplotlib for now, can upgrade to pygame/OpenGL later
try:
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d import Axes3D
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not installed. Visualization disabled.")


# ============================================================================
# Bloch Sphere Visualization
# ============================================================================

def plot_bloch_sphere(qubit, show=True):
    """
    NOMAIN: PLOT_BLOCH_SPHERE(qubit)
    Visualize a qubit on the Bloch sphere
    """
    if not HAS_MATPLOTLIB:
        print("Visualization not available (matplotlib not installed)")
        return
    
    from quantum_lib import Qubit
    
    if not isinstance(qubit, Qubit):
        print(f"Cannot plot {type(qubit)} on Bloch sphere")
        return
    
    # Get Bloch coordinates
    x, y, z = qubit.get_bloch_coords()
    
    # Create 3D plot
    fig = plt.figure(figsize=(8, 8))
    ax = fig.add_subplot(111, projection='3d')
    
    # Draw sphere
    u = np.linspace(0, 2 * np.pi, 100)
    v = np.linspace(0, np.pi, 100)
    x_sphere = np.outer(np.cos(u), np.sin(v))
    y_sphere = np.outer(np.sin(u), np.sin(v))
    z_sphere = np.outer(np.ones(np.size(u)), np.cos(v))
    ax.plot_surface(x_sphere, y_sphere, z_sphere, alpha=0.1, color='cyan')
    
    # Draw axes
    ax.plot([0, 0], [0, 0], [-1, 1], 'k-', linewidth=1)
    ax.plot([0, 0], [-1, 1], [0, 0], 'k-', linewidth=1)
    ax.plot([-1, 1], [0, 0], [0, 0], 'k-', linewidth=1)
    
    # Label axes
    ax.text(0, 0, 1.2, '|0⟩', fontsize=14)
    ax.text(0, 0, -1.2, '|1⟩', fontsize=14)
    ax.text(1.2, 0, 0, '|+⟩', fontsize=14)
    ax.text(0, 1.2, 0, '|+i⟩', fontsize=14)
    
    # Draw state vector
    ax.quiver(0, 0, 0, x, y, z, color='red', arrow_length_ratio=0.1, linewidth=3)
    
    # Set limits and labels
    ax.set_xlim([-1, 1])
    ax.set_ylim([-1, 1])
    ax.set_zlim([-1, 1])
    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_zlabel('Z')
    ax.set_title('Qubit State on Bloch Sphere')
    
    if show:
        plt.show()
    
    return fig


# ============================================================================
# State Probability Visualization
# ============================================================================

def plot_state_probabilities(quantum_obj, show=True):
    """
    NOMAIN: PLOT_PROBABILITIES(qubit/register)
    Show probability distribution of measurement outcomes
    """
    if not HAS_MATPLOTLIB:
        print("Visualization not available (matplotlib not installed)")
        return
    
    from quantum_lib import Qubit, QuantumRegister
    
    if isinstance(quantum_obj, Qubit):
        # Single qubit - show |0> and |1> probabilities
        probs = [abs(quantum_obj.state[0])**2, abs(quantum_obj.state[1])**2]
        labels = ['|0⟩', '|1⟩']
    elif isinstance(quantum_obj, QuantumRegister):
        # Multiple qubits - show all basis state probabilities
        probs = [abs(amp)**2 for amp in quantum_obj.state]
        n = quantum_obj.num_qubits
        labels = [f'|{format(i, f"0{n}b")}⟩' for i in range(len(probs))]
    else:
        print(f"Cannot plot probabilities for {type(quantum_obj)}")
        return
    
    # Create bar chart
    fig, ax = plt.subplots(figsize=(12, 6))
    colors = ['blue' if p > 0.01 else 'lightgray' for p in probs]
    ax.bar(labels, probs, color=colors)
    ax.set_ylabel('Probability')
    ax.set_xlabel('Basis State')
    ax.set_title('Measurement Probability Distribution')
    ax.set_ylim([0, 1])
    plt.xticks(rotation=45)
    plt.tight_layout()
    
    if show:
        plt.show()
    
    return fig


# ============================================================================
# Circuit Visualization (stub for future)
# ============================================================================

def visualize_circuit(circuit):
    """
    NOMAIN: VISUALIZE_CIRCUIT(circuit)
    Draw quantum circuit diagram
    """
    print(f"Circuit visualization: {circuit}")
    print("(Full circuit drawing coming soon)")


def animate_circuit_evolution(circuit):
    """
    NOMAIN: ANIMATE_CIRCUIT(circuit)
    Show animated execution of circuit
    """
    print(f"Animating circuit: {circuit}")
    print("(Animation coming soon)")


# ============================================================================
# Real-time Plotting (stub for future)
# ============================================================================

def plot_realtime_init():
    """Initialize real-time plotting window"""
    if not HAS_MATPLOTLIB:
        return None
    
    plt.ion()  # Interactive mode
    fig, ax = plt.subplots()
    return (fig, ax)


def plot_realtime_update(fig_ax, x_data, y_data, title="Real-time Plot"):
    """Update real-time plot with new data"""
    if not HAS_MATPLOTLIB or fig_ax is None:
        return
    
    fig, ax = fig_ax
    ax.clear()
    ax.plot(x_data, y_data)
    ax.set_title(title)
    plt.draw()
    plt.pause(0.01)


# ============================================================================
# NOMAIN Built-in Wrappers
# ============================================================================

def builtin_plot_bloch_sphere(qubit):
    """NOMAIN: PLOT_BLOCH_SPHERE(qubit)"""
    return plot_bloch_sphere(qubit, show=True)


def builtin_plot_probabilities(quantum_obj):
    """NOMAIN: PLOT_PROBABILITIES(qubit)"""
    return plot_state_probabilities(quantum_obj, show=True)


def builtin_show_entanglement(register):
    """NOMAIN: SHOW_ENTANGLEMENT(register)"""
    print(f"Entanglement visualization for: {register}")
    # TODO: Implement entanglement measure (e.g., concurrence, entropy)
    print("(Detailed entanglement analysis coming soon)")
