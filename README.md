# LOGICARIUM

**HIGH-PERFORMANCE | NODE-BASED | LOGIC SIMULATOR**

---

### **Overview**

`Logicarium` is a minimalist, performant, visual logic design environment built for speed and clarity. Design complex digital systems using a high-density, performant interface.

**Custom gates** for modular logic, and a **visual DSL** for script with real-time synchronization and code generation, all **hardware-accelerated**.

Logicarium can also be used as a gamified logic simulator for educational purposes. The user/player is expected to explore and figure out how to build more complex logic circuits using the existing primitive gates (**AND, NOT**) and custom gate creation. Find the tutorials in the [documentation](https://logicarium.tomlin7.com/).

### **Quick Start**
```bash
# Clone with submodules
> git clone --recursive https://github.com/tomlin7/logicarium.git

# Generate Visual Studio solution
> premake5 vs2022

# Build
> msbuild Logicarium.sln /p:Configuration=Release /p:Platform=x64
```

---

*Inspired by Logisim. Reimagined for performance.*
