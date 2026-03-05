import os
import sys
import subprocess
import argparse
import xml.etree.ElementTree as ET

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SIM_TIME = 100 #seconds
AREA_SIZE = 2000 #meters
GRID_CELLS = 10 #number of edges in one direction
CELL_SIZE  = AREA_SIZE // GRID_CELLS 

def get_sumo_home():
    sumo_home = os.environ.get("SUMO_HOME", "")
    if not sumo_home or not os.path.isdir(sumo_home):
        # Tentativa automática
        for candidate in ["/usr/share/sumo", "/opt/sumo"]:
            if os.path.isdir(candidate):
                return candidate
        print("ERRO: SUMO_HOME not found. Execute 'source ~/.bashrc'.")
        sys.exit(1)
    return sumo_home


def generate_network(output_dir: str):
    """Use netgenerate to create a grid network map."""
    net_file = os.path.join(output_dir, "grid.net.xml")
    if os.path.exists(net_file):
        print(f"  Rede já existe: {net_file}")
        return net_file

    cmd = [
        "netgenerate",
        "--grid",
        f"--grid.number={GRID_CELLS + 1}",   # nós de junção
        f"--grid.x-length={CELL_SIZE}",
        f"--grid.y-length={CELL_SIZE}",
        "--grid.attach-length=0",
        "--default.speed=30",                 # maxSpeed = 30 m/s
        f"--offset.x={AREA_SIZE // 2}",
        f"--offset.y={AREA_SIZE // 2}",
        "--output-file", net_file,
    ]
    subprocess.run(cmd, check=True)
    return net_file


def generate_vehicle_type(output_dir: str):
    vtypes_file = os.path.join(output_dir, "vtypes.add.xml")
    root = ET.Element("additional")
    vtype = ET.SubElement(root, "vType")
    vtype.set("id",       "car")
    vtype.set("accel",    "2.0")     # m/s² - paper Tabela I
    vtype.set("decel",    "2.0")     # m/s² - padrão SUMO
    vtype.set("maxSpeed", "30.0")    # m/s  - paper Tabela I
    vtype.set("length",   "4.5")     # m    - paper Tabela I
    vtype.set("width",    "2.0")     # m    - paper Tabela I
    vtype.set("sigma",    "0.0")     # fator de imperfeição do motorista
    vtype.set("color",    "1,0,0")

    tree = ET.ElementTree(root)
    ET.indent(tree, space="  ")
    tree.write(vtypes_file, xml_declaration=True, encoding="unicode")
    
    return vtypes_file

def generate_routes(output_dir: str, net_file: str, num_vehicles: int):
    sumo_home  = get_sumo_home()
    rtrips_py  = os.path.join(sumo_home, "tools", "randomTrips.py")

    routes_file  = os.path.join(output_dir, f"routes_{num_vehicles}v.rou.xml")
    trips_file   = os.path.join(output_dir, f"trips_{num_vehicles}v.trips.xml")

    if os.path.exists(routes_file):
        print(f"  Routes already exist: {routes_file}")
        return routes_file
    

    # The randomTrips.py script generates trips at regular intervals. 
    # To insert a total of `num_vehicles` vehicles, we can calculate the insertion end time based on a fixed period. 
    # For example, if we choose a period of 0.1 seconds, the insertion end time would be `num_vehicles * period`.
    # IN 3.0s we would have 30 vehicles, which is a reasonable rate for our scenario.
    period = 0.1
    insertion_end = num_vehicles * period

    cmd = [
        "python3", rtrips_py,
        "-n", net_file,
        "-b", "0",
        "-e", f"{insertion_end:.1f}",
        "-p", f"{period:.1f}",
        "--trip-attributes", 'type="car"',  # referencia o vType definido em vtypes.add.xml
        "--fringe-factor", "1",          # viagens internas ao grid
        "--min-distance", "500",         # distância mínima de viagem
        "--allow-roundabouts",
        "--route-file", routes_file,
        "-o",  trips_file,
    ]
    subprocess.run(cmd, check=True)
    return routes_file

def generate_sumo_config(output_dir: str, net_file: str,
                         routes_file: str, vtypes_file: str,
                         num_vehicles: int):
    
    #Floating car data??? 
    fcd_file = os.path.join(output_dir, f"fcd_{num_vehicles}v.xml")
    config_file = os.path.join(output_dir, f"scenario1_{num_vehicles}v.sumocfg")

    if os.path.exists(config_file) and os.path.exists(fcd_file):
        print(f"  Config already exists: {config_file}")
        return config_file, fcd_file
    
    root = ET.Element("configuration")
    inp = ET.SubElement(root, "input")
    ET.SubElement(inp, "net-file",        attrib={"value": os.path.basename(net_file)})
    ET.SubElement(inp, "route-files",     attrib={"value": os.path.basename(routes_file)})
    ET.SubElement(inp, "additional-files",attrib={"value": os.path.basename(vtypes_file)})

    time = ET.SubElement(root, "time")
    ET.SubElement(time, "begin", attrib={"value": "0"})
    ET.SubElement(time, "end",   attrib={"value": str(SIM_TIME)})
    ET.SubElement(time, "step-length", attrib={"value": "0.1"})

    out = ET.SubElement(root, "output")
    # Save the Floating Car Data (FCD) for post-simulation analysis
    ET.SubElement(out, "fcd-output", attrib={"value": os.path.basename(fcd_file)})

    tree = ET.ElementTree(root)
    ET.indent(tree, space="  ")
    tree.write(config_file, xml_declaration=True, encoding="unicode")
    return config_file, fcd_file
    
def run_sumo(output_dir: str, cfg_file: str, num_vehicles: int):
    fcd_file = os.path.join(output_dir, f"fcd_{num_vehicles}v.xml")
    if os.path.exists(fcd_file):
        print(f"  FCD já existe: {fcd_file}")
        return fcd_file

    cmd = ["sumo", "-c", cfg_file, "--no-warnings", "--no-step-log"]
    subprocess.run(cmd, cwd=output_dir, check=True)
    return fcd_file

def convert_to_ns3(output_dir: str, fcd_file: str, num_vehicles: int):
    """Convert SUMO FCD to NS-3 trace format using traceExporter.py."""
    sumo_home   = get_sumo_home()

    exporter_py = os.path.join(sumo_home, "tools", "traceExporter.py")
    ns3_file = os.path.join(output_dir, f"mobility_{num_vehicles}v.tcl")
    if os.path.exists(ns3_file):
        print(f"  NS-3 trace already exists: {ns3_file}")
        return ns3_file

    cmd = [
        "python3", exporter_py,
        "--fcd-input",          fcd_file,
        "--ns2mobility-output", ns3_file,
        "--penetration",        "1.0",
    ]
    subprocess.run(cmd, check=True)
    return ns3_file

def main():
    parser = argparse.ArgumentParser(description="Generate SUMO scenarios and convert to NS-3 format.")
    parser.add_argument("--output-dir", default=SCRIPT_DIR, help="Directory to save generated files")
    parser.add_argument("--vehicles", type=int, default=30, help="Number of vehicles in the scenario")
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    print("Generating network...")
    net_file = generate_network(args.output_dir)

    print("Generating vehicle types...")
    vtypes_file = generate_vehicle_type(args.output_dir)

    print("Generating routes...")
    routes_file = generate_routes(args.output_dir, net_file, args.vehicles)

    print("Generating SUMO configuration...")
    cfg_file, fcd_file = generate_sumo_config(args.output_dir, net_file, routes_file, vtypes_file, args.vehicles)

    print("Executing SUMO simulation...")
    run_sumo(args.output_dir, cfg_file, args.vehicles)

    print("Converting to NS-3 format...")
    ns3_file = convert_to_ns3(args.output_dir, fcd_file, args.vehicles)
    print(f"NS-3 mobility trace generated: {ns3_file}")
    



if __name__ == "__main__":
    main()