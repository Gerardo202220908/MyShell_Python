import pandas as pd

d = pd.read_csv("reporte_thrashing.csv")

cortes = [10, 18, 24]
cols = [
    "Planificador", "Algoritmo", "MarcosRAM", "TotalPageFaults",
    "PF_Obligatorios", "PF_Capacidad", "TasaFallos",
    "AVG_WT", "AVG_TAT", "AVG_RT", "UtilCPU", "CambiosContexto",
]

sel = (
    d[d.MarcosRAM.isin(cortes)][cols]
    .sort_values(["MarcosRAM", "Planificador", "Algoritmo"])
)

sel.to_csv("reporte_resumen.csv", index=False)
print(sel.to_string(index=False))
print("\nGenerado: reporte_resumen.csv")