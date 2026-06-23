import matplotlib.pyplot as plt

data_raw = """
8192
mem alloc and copy, cycles
1712
load to vector reg, cycles
6443
vector computations, cycles
1987
performance, OP/1000 cycles
1943
utilization, %o
121

4096
mem alloc and copy, cycles
1160
load to vector reg, cycles
4600
vector computations, cycles
1435
performance, OP/1000 cycles
1357
utilization, %o
84

2048
mem alloc and copy, cycles
905
load to vector reg, cycles
3678
vector computations, cycles
1180
performance, OP/1000 cycles
843
utilization, %o
52

128
mem alloc and copy, cycles
667
load to vector reg, cycles
2865
vector computations, cycles
919
performance, OP/1000 cycles
67
utilization, %o
4

8
mem alloc and copy, cycles
652
load to vector reg, cycles
2862
vector computations, cycles
868
performance, OP/1000 cycles
4
utilization, %o
0
"""

# -------- parsing --------
blocks = [b.strip() for b in data_raw.strip().split("\n\n")]

sizes = []
mem = []
load = []
comp = []
perf = []
util = []

for b in blocks:
    lines = b.splitlines()
    sizes.append(int(lines[0]))

    mem.append(int(lines[2]))
    load.append(int(lines[4]))
    comp.append(int(lines[6]))
    perf.append(int(lines[8]))
    util.append(int(lines[10]))

# sort (ascending for nicer bars)
sizes, mem, load, comp, perf, util = zip(*sorted(
    zip(sizes, mem, load, comp, perf, util)
))

sizes = list(sizes)
mem = list(mem)
load = list(load)
comp = list(comp)
perf = list(perf)
util = list(util)

# -------- plot 1: stacked bars (fixed width, clearer) --------
plt.figure(figsize=(8, 5))

x = range(len(sizes))
bar_width = 0.6
totals = [m + l + c for m, l, c in zip(mem, load, comp)]

plt.bar(x, mem, width=bar_width, label="mem alloc + copy")
plt.bar(x, load, width=bar_width, bottom=mem, label="load to vector reg")

bottom2 = [m + l for m, l in zip(mem, load)]
plt.bar(x, comp, width=bar_width, bottom=bottom2, label="vector computations")

for xi, total in zip(x, totals):
    plt.text(
        xi,
        total + 50,       
        f"{total}",
        ha='center',
        va='bottom',
        fontsize=10,
        fontweight='bold'
    )

plt.xticks(x, sizes)
plt.xlabel("Problem size")
plt.ylabel("Cycles")
plt.title("Cycle breakdown")
plt.legend()
plt.grid(axis="y", alpha=0.3)

plt.tight_layout()
plt.show()

# -------- plot 2: dual axis --------
fig, ax1 = plt.subplots(figsize=(8, 5))

ax1.plot(sizes, perf, marker='o', label="performance (OP/1000 cycles)")
ax1.set_xlabel("Problem size")
ax1.set_ylabel("Performance")
ax1.grid(alpha=0.3)

ax2 = ax1.twinx()
ax2.plot(sizes, util, marker='o', color='orange', label="utilization (%)")
ax2.set_ylabel("Utilization (%)")

# combined legend
lines1, labels1 = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax1.legend(lines1 + lines2, labels1 + labels2, loc="best")

plt.title("Performance & Utilization")
plt.tight_layout()
plt.show()