import matplotlib
matplotlib.use("Agg")
import pandas as pd
import matplotlib.pyplot as plt

d = pd.read_csv("reporte_thrashing.csv")
q = pd.read_csv("reporte_quantum.csv")
estilo = {"FIFO": ("tab:red", "o"), "LRU": ("tab:blue", "s"), "RELOJ": ("tab:green", "^")}
UMBRAL = 0.50  # tasa de fallos >= 50% => thrashing (igual que sim_kernel.c)

rr = d[d.Planificador == "RR"]

# ---------- Figura 1: efecto de thrashing (RR, FIFO/LRU/Reloj) ----------
fig, ax = plt.subplots(2, 2, figsize=(13, 9))
paneles = [("TotalPageFaults", "Total de fallos de página", "Fallos de página vs RAM"),
        ("UtilCPU", "Utilización de CPU útil (%)", "Utilización de CPU vs RAM"),
        ("AVG_WT", "Tiempo de espera promedio (ms)", "Tiempo de espera (WT) vs RAM"),
        ("AVG_TAT", "Tiempo de retorno promedio (ms)", "Tiempo de retorno (TAT) vs RAM")]
lru = rr[rr.Algoritmo == "LRU"]
zona = lru[lru.TasaFallos >= UMBRAL].MarcosRAM
for a, (col, yl, tit) in zip(ax.flat, paneles):
    for alg, (c, m) in estilo.items():
        s = rr[rr.Algoritmo == alg]
        a.plot(s.MarcosRAM, s[col], marker=m, color=c, lw=2, label=alg)
    if len(zona):
        a.axvspan(zona.min() - 1, zona.max() + 1, color="red", alpha=0.10,
                label="Zona de thrashing (LRU)")
    a.set_title(tit); a.set_xlabel("Marcos de RAM"); a.set_ylabel(yl); a.grid(True)
    a.legend(fontsize=8)
fig.suptitle("Efecto del thrashing - Round Robin (q=3), 4 procesos, reemplazo global", fontsize=13)
fig.tight_layout()
fig.savefig("grafico_thrashing.png", dpi=130)

# ---------- Figura 2: RR (apropiativo) vs SJF (no apropiativo) con LRU ----------
fig, ax = plt.subplots(1, 3, figsize=(15, 4.5))
for plan, c, m in [("RR", "tab:blue", "s"), ("SJF", "tab:orange", "D")]:
    s = d[(d.Planificador == plan) & (d.Algoritmo == "LRU")]
    ax[0].plot(s.MarcosRAM, s.TotalPageFaults, marker=m, color=c, lw=2, label=plan)
    ax[1].plot(s.MarcosRAM, s.AVG_WT, marker=m, color=c, lw=2, label=plan)
    ax[2].plot(s.MarcosRAM, s.UtilCPU, marker=m, color=c, lw=2, label=plan)
for a, t, y in zip(ax, ["Fallos de página", "Tiempo de espera promedio", "Utilización de CPU"],
                ["Fallos", "ms", "%"]):
    a.set_title(f"{t}: RR vs SJF (LRU)"); a.set_xlabel("Marcos de RAM"); a.set_ylabel(y)
    a.grid(True); a.legend()
fig.tight_layout(); fig.savefig("grafico_planificadores.png", dpi=130)

# ---------- Figura 3: efecto del quantum ----------
fig, ax = plt.subplots(1, 3, figsize=(15, 4.5))
for marcos, c in [(6, "tab:red"), (20, "tab:green")]:
    s = q[q.MarcosRAM == marcos]
    lab = f"{marcos} marcos"
    ax[0].plot(s.Quantum, s.AVG_WT, marker="o", color=c, lw=2, label=lab)
    ax[1].plot(s.Quantum, s.TotalPageFaults, marker="o", color=c, lw=2, label=lab)
    ax[2].plot(s.Quantum, s.CambiosContexto, marker="o", color=c, lw=2, label=lab)
for a, t in zip(ax, ["Espera promedio (WT)", "Fallos de página", "Cambios de contexto"]):
    a.set_title(f"{t} vs quantum (RR, LRU)"); a.set_xlabel("Quantum (ms)"); a.grid(True); a.legend()
fig.tight_layout(); fig.savefig("grafico_quantum.png", dpi=130)
print("Gráficos generados: grafico_thrashing.png, grafico_planificadores.png, grafico_quantum.png")