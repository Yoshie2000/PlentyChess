# Assuming your input is in 'input.txt'
with open('src/l3weights.txt', 'r') as f:
    lines = f.readlines()

with open('output.txt', 'w') as f:
    for line in lines:
        if not line.strip(): continue
        
        # Split the line into the identifier and the value
        # Example: "l3_0_0" and " 0.43223..."
        identifier, value = line.split(',')
        
        # Split the identifier by underscores to get the indices
        # "l3_0_0" -> ['l3', '0', '0']
        parts = identifier.split('_')
        i = parts[1]
        j = parts[2]
        
        # Write the formatted string
        f.write(f"l3Weights[{i}][{j}] = {value.strip()};\n")

print("Conversion complete! Check output.txt")