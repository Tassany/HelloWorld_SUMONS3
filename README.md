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
| `scenario.cfg` | SUMO configuration — links the two files above and sets simulation time |

---

## Generating a Grid Network

```bash
netgenerate --grid \
            --grid.number=5 \
            --grid.length=200 \
            --output-file=sumo/map.net.xml
```

## Defining Routes

Vehicles require a type (length, acceleration, max speed) and a route. Example `hello.rou.xml`:

```xml
<routes>
    <vType accel="1.0" decel="5.0" id="Car" length="2.0" maxSpeed="100.0" sigma="0.0" />
    <route id="route0" edges="1to2 out"/>
    <vehicle depart="1" id="veh0" route="route0" type="Car" />
</routes>
```

See [Definition of Vehicles, Vehicle Types, and Routes](https://sumo.dlr.de/docs/Definition_of_Vehicles%2C_Vehicle_Types%2C_and_Routes.html) for more details.

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

---

## References

- [SUMO Documentation](https://sumo.dlr.de/docs/Tools/index.html)
- [ns-3 Documentation](https://www.nsnam.org/documentation/)
