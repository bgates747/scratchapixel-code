# invert_winding_order.py

def invert_winding_order(input_file, output_file):
    with open(input_file, 'r') as infile, open(output_file, 'w') as outfile:
        for line in infile:
            stripped_line = line.strip()
            if stripped_line.startswith('f '):
                parts = stripped_line.split()
                face_elements = parts[1:]
                inverted_face = ' '.join(reversed(face_elements))
                outfile.write(f"f {inverted_face}\n")
            else:
                outfile.write(line)

if __name__ == "__main__":
    input_filename = '3d-viewing-pinhole-camera/wolf_map1.obj'
    output_filename = '3d-viewing-pinhole-camera/wolf_map1_inv.obj'
    invert_winding_order(input_filename, output_filename)
    print(f"Inverted winding order saved to {output_filename}")
