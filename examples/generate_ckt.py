import random

# You can change this number! 
NUM_GATES = 1000
FILENAME = "massive_circuit.txt"

def generate_massive_netlist():
    gate_types = ["AND", "OR", "XOR", "NAND", "NOR"]
    inputs = ["A", "B", "C", "D", "E"]
    
    with open(FILENAME, "w") as f:
        f.write("# Massive Circuit for STA Load Testing\n")
        f.write("CLOCK_PERIOD 100.0\n\n") # Large clock to handle the massive delay
        
        # Primary Inputs and Outputs
        f.write("INPUT A B C D E\n")
        f.write(f"OUTPUT OUT_FINAL\n\n")

        # Create the first gate
        first_gate = random.choice(gate_types)
        f.write(f"GATE {first_gate} G_0 temp_0 A B\n")
        
        # Create a massive chain of intermediate gates
        for i in range(1, NUM_GATES - 1):
            g_type = random.choice(gate_types)
            # Mix the previous output with a random primary input to keep parsing valid
            rand_input = random.choice(inputs) 
            f.write(f"GATE {g_type} G_{i} temp_{i} temp_{i-1} {rand_input}\n")
            
        # Create the final gate connecting to the output
        last_gate = random.choice(gate_types)
        f.write(f"GATE {last_gate} G_{NUM_GATES-1} OUT_FINAL temp_{NUM_GATES-2} C\n")
        
    print(f"Successfully generated '{FILENAME}' with {NUM_GATES} gates.")

if __name__ == "__main__":
    generate_massive_netlist()
