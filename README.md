# HelloWorld SUMO + ns-3

A hello-world project integrating [SUMO](https://sumo.dlr.de/) (Simulation of Urban MObility) with [ns-3](https://www.nsnam.org/) for vehicular network simulations.

## Requirements

- Ubuntu/Debian system
- `git`, `cmake`, `python3`

---

## Installation

### 1. System dependencies

```bash
sudo apt update
sudo apt install -y build-essential python3 python3-pip git cmake \
    libxml2-dev libsqlite3-dev libssl-dev
```

### 2. Install SUMO

```bash
sudo apt install -y sumo sumo-tools sumo-doc

# Verify installation
sumo --version

# Set environment variable
echo 'export SUMO_HOME=/usr/share/sumo' >> ~/.bashrc
source ~/.bashrc
```

### 3. Install ns-3

```bash
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev

# Configure and build
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

### 4. ns-3 + SUMO coupling (TraCI)

```bash
git clone https://github.com/vodafone-chair/ns3-sumo-coupling.git
```

---

## SUMO Simulation Files

| File | Description |
|------|-------------|
| `map.net.xml` | Road network map |
| `routes.rou.xml` | Vehicles and their routes in the simulation |
| `scenario.cfg` | SUMO configuration links the two files above and sets simulation time |


---

## SUMO Tools Reference

| Tool | Description |
|------|-------------|
| [sumo](https://sumo.dlr.de/docs/sumo.html) | Microscopic simulation (no GUI) |
| [sumo-gui](https://sumo.dlr.de/docs/sumo-gui.html) | Microscopic simulation with GUI |
| [netconvert](https://sumo.dlr.de/docs/netconvert.html) | Network importer and converter |
| [netedit](https://sumo.dlr.de/docs/Netedit/index.html) | Graphical network editor |
| [netgenerate](https://sumo.dlr.de/docs/netgenerate.html) | Abstract network generator |
| [duarouter](https://sumo.dlr.de/docs/duarouter.html) | Fastest route computation (DUA) |
| [jtrrouter](https://sumo.dlr.de/docs/jtrrouter.html) | Routes via junction turning percentages |
| [dfrouter](https://sumo.dlr.de/docs/dfrouter.html) | Routes from induction loop data |
| [marouter](https://sumo.dlr.de/docs/marouter.html) | Macroscopic assignment |
| [od2trips](https://sumo.dlr.de/docs/od2trips.html) | O/D matrices to vehicle trips |
| [polyconvert](https://sumo.dlr.de/docs/polyconvert.html) | Import POIs and polygons |
| [activitygen](https://sumo.dlr.de/docs/activitygen.html) | Demand generation from population model |
| [Additional Tools](https://sumo.dlr.de/docs/Tools/index.html) | Various utility scripts |

## Generating a road network map
Using the `netgenerate` tool, is possible to create automatically a map. For example, to create a 5x5 grid map with 200m long edges:
```bash
netgenerate --grid \
            --grid.number=5 \
            --grid.length=200 \
            --output-file=sumo/map.net.xml
```


## Defining Routes
Now that we have a map, we can define vehicle routes. Vehicles require a type (length, acceleration, max speed) and a route. Furthermore it needs a so called sigma parameter which introduces some random behavior owing to the car following model used.Example `veic.rou.xml`:

```xml
<routes>
    <vType id="car" maxSpeed="30" accel="2" decel="2" sigma="0.0" />
    
    <flow id="flow_0" type="car" 
          from="edge_0" to="edge_10" 
          begin="0" end="100" 
          number="20"/>
</routes>
```

See [Definition of Vehicles, Vehicle Types, and Routes](https://sumo.dlr.de/docs/Definition_of_Vehicles%2C_Vehicle_Types%2C_and_Routes.html) for more details.


## Configuring the Simulation
The `scenario.cfg` file links the network and route files and sets simulation parameters:

```xml<configuration>
    <input>
        <net-file value="map.net.xml"/>
        <route-files value="veic.rou.xml"/>
    </input>
    <time>
        <begin value="0"/>
        <end value="1000"/>
        <step-length value="0.1"/>
    </time>
</configuration>
``` 

## Running the SUMO Simulation
      python3 generate.py

## SUMO output

 SUMO will generate the output files. The mobility file `mobility_30v.tcl` will be used by ns-3 to set the positions of the vehicles.
 
## Runnning the ns-3 Simulation
    ./ns3 run scratch/vanet_simulation
---
## References

- [SUMO Documentation](https://sumo.dlr.de/docs/Tools/index.html)
- [ns-3 Documentation](https://www.nsnam.org/documentation/)
