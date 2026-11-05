import matplotlib
matplotlib.use("Agg")
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

d = pd.read_csv("reporte_thrashing.csv")
q = pd.read_csv("reporte_quantum.csv")

estilo = {"FIFO": ("tab:red", "o"), "LRU": ("tab:blue", "s"), "RELOJ": ("tab:green", "^")}
UMBRAL = 0.50
WS_SUMA = 16          # 4 + 3 + 5 + 4
PISO = 18             # marcos donde se llega a los 23 fallos obligatorios

rr = d[d.Planificador == "RR"].copy()


def salida_thrashing(alg):
    """Primer numero de marcos donde la tasa de fallos cae por debajo del umbral."""
    s = rr[rr.Algoritmo == alg].sort_values("MarcosRAM")
    fuera = s[s.TasaFallos < UMBRAL]
    return int(fuera.MarcosRAM.iloc[0]) if len(fuera) else None


salidas = {alg: salida_thrashing(alg) for alg in estilo}

# ---------- Figura 1: efecto de thrashing (RR, FIFO/LRU/Reloj) ----------
fig, ax = plt.subplots(2, 2, figsize=(13, 9))
paneles = [
    ("TotalPageFaults", "Total de fallos de pagina", "Fallos de pagina vs RAM"),
    ("UtilCPU", "Utilizacion de CPU util (%)", "Utilizacion de CPU vs RAM"),
    ("AVG_WT", "Tiempo de espera promedio (ms)", "Tiempo de espera (WT) vs RAM"),
    ("AVG_TAT", "Tiempo de retorno promedio (ms)", "Tiempo de retorno (TAT) vs RAM"),
]
for a, (col, yl, tit) in zip(ax.flat, paneles):
    for alg, (c, m) in estilo.items():
        s = rr[rr.Algoritmo == alg].sort_values("MarcosRAM")
        a.plot(s.MarcosRAM, s[col], marker=m, color=c, lw=2, label=alg)
    for alg, x in salidas.items():
        if x is not None:
            a.axvline(x, color=estilo[alg][0], ls="--", lw=1, alpha=0.55)
    a.axvline(WS_SUMA, color="gray", ls=":", lw=1.3)
    a.axvline(PISO, color="black", ls="-.", lw=1.3)
    a.set_title(tit)
    a.set_xlabel("Marcos de RAM")
    a.set_ylabel(yl)
    a.grid(True)

handles = [Line2D([0], [0], color=c, marker=m, lw=2, label=alg)
           for alg, (c, m) in estilo.items()]
handles += [
    Line2D([0], [0], color="gray", ls=":", lw=1.3, label=f"Suma de working sets = {WS_SUMA}"),
    Line2D([0], [0], color="black", ls="-.", lw=1.3, label=f"Piso de fallos obligatorios ~ {PISO}"),
]
fig.legend(handles=handles, loc="lower center", ncol=5, fontsize=8, frameon=False)
fig.suptitle(
    "Efecto del thrashing - Round Robin (q=3), 4 procesos, reemplazo global\n"
    "(lineas punteadas de color = salida del thrashing por algoritmo)",
    fontsize=12,
)
fig.tight_layout(rect=[0, 0.05, 1, 0.95])
fig.savefig("grafico_thrashing.png", dpi=130)

# ---------- Figura 2: RR (apropiativo) vs SJF (no apropiativo) con LRU ----------
fig, ax = plt.subplots(1, 3, figsize=(15, 4.5))
for plan, c, m in [("RR", "tab:blue", "s"), ("SJF", "tab:orange", "D")]:
    s = d[(d.Planificador == plan) & (d.Algoritmo == "LRU")].sort_values("MarcosRAM")
    ax[0].plot(s.MarcosRAM, s.TotalPageFaults, marker=m, color=c, lw=2, label=plan)
    ax[1].plot(s.MarcosRAM, s.AVG_WT, marker=m, color=c, lw=2, label=plan)
    ax[2].plot(s.MarcosRAM, s.UtilCPU, marker=m, color=c, lw=2, label=plan)
for a, t, y in zip(ax,
                   ["Fallos de pagina", "Tiempo de espera promedio", "Utilizacion de CPU"],
                   ["Fallos", "ms", "%"]):
    a.set_title(f"{t}: RR vs SJF (LRU)")
    a.set_xlabel("Marcos de RAM")
    a.set_ylabel(y)
    a.grid(True)
    a.legend()
fig.tight_layout()
fig.savefig("grafico_planificadores.png", dpi=130)

# ---------- Figura 3: efecto del quantum ----------
fig, ax = plt.subplots(1, 3, figsize=(15, 4.5))
for marcos, c in [(6, "tab:red"), (20, "tab:green")]:
    s = q[q.MarcosRAM == marcos].sort_values("Quantum")
    ax[0].plot(s.Quantum, s.AVG_WT, marker="o", color=c, lw=2, label=f"{marcos} marcos")
    ax[1].plot(s.Quantum, s.TotalPageFaults, marker="o", color=c, lw=2, label=f"{marcos} marcos")

# Los cambios de contexto dependen solo del quantum: una sola linea.
s = q[q.MarcosRAM == 6].sort_values("Quantum")
ax[2].plot(s.Quantum, s.CambiosContexto, marker="o", color="tab:purple", lw=2)

ax[0].set_title("Espera promedio (WT) vs quantum (RR, LRU)")
ax[0].set_xlabel("Quantum (ms)")
ax[0].set_ylabel("ms")
ax[0].grid(True)
ax[0].legend()

ax[1].set_title("Fallos de pagina vs quantum (RR, LRU)")
ax[1].set_xlabel("Quantum (ms)")
ax[1].set_ylabel("Fallos")
ax[1].grid(True)
ax[1].legend()

ax[2].set_title("Cambios de contexto vs quantum\n(no dependen de los marcos)")
ax[2].set_xlabel("Quantum (ms)")
ax[2].set_ylabel("CS")
ax[2].grid(True)

fig.tight_layout()
fig.savefig("grafico_quantum.png", dpi=130)

# ---------- Figura 4: trade-off WT vs RT (24 marcos, LRU) ----------
fig, ax = plt.subplots(1, 2, figsize=(13, 5))

# Panel A: barras agrupadas WT vs RT a 24 marcos
base = d[(d.Algoritmo == "LRU") & (d.MarcosRAM == 24)].set_index("Planificador")
planes = ["RR", "SJF"]
pos = list(range(len(planes)))
ancho = 0.36
wt = [float(base.loc[p, "AVG_WT"]) for p in planes]
rt = [float(base.loc[p, "AVG_RT"]) for p in planes]
b1 = ax[0].bar([i - ancho / 2 for i in pos], wt, ancho, label="WT (espera)", color="tab:blue")
b2 = ax[0].bar([i + ancho / 2 for i in pos], rt, ancho, label="RT (respuesta)", color="tab:orange")
for b in list(b1) + list(b2):
    ax[0].text(b.get_x() + b.get_width() / 2, b.get_height() + 4,
            f"{b.get_height():.2f}", ha="center", fontsize=10)
ax[0].set_xticks(pos)
ax[0].set_xticklabels(planes)
ax[0].set_ylabel("ms")
ax[0].set_title("WT vs RT a 24 marcos (LRU)\n23 fallos en ambos planificadores")
ax[0].legend()
ax[0].grid(True, axis="y")

# Panel B: evolucion de WT y RT con los marcos
for plan, c, m in [("RR", "tab:blue", "s"), ("SJF", "tab:orange", "D")]:
    s = d[(d.Planificador == plan) & (d.Algoritmo == "LRU")].sort_values("MarcosRAM")
    ax[1].plot(s.MarcosRAM, s.AVG_WT, marker=m, color=c, lw=2, ls="-", label=f"{plan} WT")
    ax[1].plot(s.MarcosRAM, s.AVG_RT, marker=m, color=c, lw=2, ls="--", label=f"{plan} RT")
    last = s.iloc[-1]
    ax[1].annotate(f"{last.AVG_WT:.2f}", (last.MarcosRAM, last.AVG_WT),
                   textcoords="offset points", xytext=(6, 4), fontsize=9, color=c)
    ax[1].annotate(f"{last.AVG_RT:.2f}", (last.MarcosRAM, last.AVG_RT),
                   textcoords="offset points", xytext=(6, 4), fontsize=9, color=c)
for plan, xoff in [("SJF", (7.2, 260)), ("RR", (11.5, 620))]:
    s = d[(d.Planificador == plan) & (d.Algoritmo == "LRU")].sort_values("MarcosRAM")
    hit = s[s.TotalPageFaults == s.TotalPageFaults.min()].iloc[0]
    ax[1].annotate(
        f"{plan} llega a {int(hit.TotalPageFaults)} fallos\ncon {int(hit.MarcosRAM)} marcos",
        xy=(hit.MarcosRAM, hit.AVG_WT), xytext=xoff,
        fontsize=9, arrowprops=dict(arrowstyle="->", color="gray"))
ax[1].set_xlabel("Marcos de RAM")
ax[1].set_ylabel("ms")
ax[1].set_title("WT y RT vs marcos (LRU)\nsolida = WT, discontinua = RT")
ax[1].legend(fontsize=8)
ax[1].grid(True)

fig.tight_layout()
fig.savefig("grafico_tradeoff.png", dpi=130)

# ---------- Figura 5: thrashing dinamico (solo si el CSV trae la columna) ----------
if "PctThrashing" in rr.columns:
    fig, ax = plt.subplots(figsize=(9, 5))
    for alg, (c, m) in estilo.items():
        s = rr[rr.Algoritmo == alg].sort_values("MarcosRAM")
        ax.plot(s.MarcosRAM, s.PctThrashing * 100, marker=m, color=c, lw=2, label=alg)
    ax.axhline(50, color="black", ls="--", lw=1, label="umbral 50%")
    ax.set_xlabel("Marcos de RAM")
    ax.set_ylabel("% de referencias en thrashing")
    ax.set_title("Thrashing dinamico (ventana deslizante de 20 referencias)")
    ax.grid(True)
    ax.legend()
    fig.tight_layout()
    fig.savefig("grafico_thrashing_dinamico.png", dpi=130)

print("Graficos generados:")
print("  grafico_thrashing.png")
print("  grafico_planificadores.png")
print("  grafico_quantum.png")
print("  grafico_tradeoff.png")
if "PctThrashing" in rr.columns:
    print("  grafico_thrashing_dinamico.png")