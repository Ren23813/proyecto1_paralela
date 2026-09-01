#!/bin/bash
# benchmark.sh
#
# Corre la version secuencial y la paralela con distintos valores de N y de
# cantidad de hilos, guarda los tiempos crudos en results.csv, y genera
# ademas un resumen con speedup y eficiencia por combinacion (N, hilos).
#
# Requiere que ya hayas compilado:
#   make -f Makefile.secuencial
#   make -f Makefile.paralelo
#
# Uso:
#   chmod +x benchmark.sh
#   ./benchmark.sh

set -e

SEQ_BIN="./screensaver_secuencial"
PAR_BIN="./screensaver_paralelo"

# --- Valores a probar: ajusta segun lo que quieras demostrar en tu bitacora ---
N_VALUES=(50 100 200 400 800 1600 3200 6400 12800 25600)
THREAD_VALUES=(1 2 4 8 16 24)
REPETITIONS=3   # minimo 10 mediciones pide el enunciado; aca 3 repeticiones x 6 N x 4 hilos = 72 corridas del paralelo

RAW_CSV="results_raw.csv"
SUMMARY_CSV="results_summary.csv"

echo "version,N,hilos,frames,tiempo_total_seg,tiempo_promedio_frame_ms,repeticion" > "$RAW_CSV"

if [ ! -x "$SEQ_BIN" ]; then
    echo "Error: no se encontro $SEQ_BIN. Corre 'make -f Makefile.secuencial' primero."
    exit 1
fi
if [ ! -x "$PAR_BIN" ]; then
    echo "Error: no se encontro $PAR_BIN. Corre 'make -f Makefile.paralelo' primero."
    exit 1
fi

echo "=== Corriendo version SECUENCIAL ==="
for N in "${N_VALUES[@]}"; do
    for rep in $(seq 1 $REPETITIONS); do
        echo "  N=$N  rep=$rep"
        line=$("$SEQ_BIN" "$N" --benchmark)
        echo "${line},${rep}" >> "$RAW_CSV"
    done
done

echo "=== Corriendo version PARALELA ==="
for N in "${N_VALUES[@]}"; do
    for T in "${THREAD_VALUES[@]}"; do
        for rep in $(seq 1 $REPETITIONS); do
            echo "  N=$N  hilos=$T  rep=$rep"
            line=$("$PAR_BIN" "$N" "$T" --benchmark)
            echo "${line},${rep}" >> "$RAW_CSV"
        done
    done
done

echo ""
echo "=== Resultados crudos guardados en $RAW_CSV ==="

# --- Resumen: promedio por (version, N, hilos) + speedup + eficiencia ---
python3 - "$RAW_CSV" "$SUMMARY_CSV" <<'PYEOF'
import csv, sys
from collections import defaultdict

raw_path, summary_path = sys.argv[1], sys.argv[2]

rows = defaultdict(list)  # (version, N, hilos) -> [tiempo_total_seg, ...]

bad_rows = []
with open(raw_path) as f:
    reader = csv.DictReader(f)
    for line_num, row in enumerate(reader, start=2):  # +2: fila 1 es el header
        try:
            key = (row['version'], int(row['N']), int(row['hilos']))
            rows[key].append(float(row['tiempo_total_seg']))
        except (TypeError, ValueError, KeyError) as e:
            bad_rows.append((line_num, row))

if bad_rows:
    print(f"AVISO: se ignoraron {len(bad_rows)} fila(s) mal formada(s) en {raw_path}:")
    for line_num, row in bad_rows:
        print(f"  linea {line_num}: {row}")
    print("  Revisa que el programa no imprima nada extra (printf de depuracion, etc.)")
    print("  antes de la linea CSV cuando corre con --benchmark.")

# tiempo secuencial promedio por N (hilos siempre 1 en secuencial)
seq_avg = {}
for (version, N, hilos), times in rows.items():
    if version == 'secuencial':
        seq_avg[N] = sum(times) / len(times)

with open(summary_path, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['version', 'N', 'hilos', 'tiempo_promedio_seg', 'speedup', 'eficiencia'])

    for (version, N, hilos), times in sorted(rows.items(), key=lambda x: (x[0][1], x[0][0], x[0][2])):
        avg = sum(times) / len(times)
        if version == 'secuencial':
            speedup = 1.0
            eficiencia = 1.0
        else:
            base = seq_avg.get(N)
            if base is None:
                speedup = eficiencia = float('nan')
            else:
                speedup = base / avg
                eficiencia = speedup / hilos
        writer.writerow([version, N, hilos, f"{avg:.6f}", f"{speedup:.4f}", f"{eficiencia:.4f}"])

print(f"Resumen (con speedup y eficiencia) guardado en {summary_path}")
PYEOF

echo ""
echo "=== Listo ==="
echo "  Datos crudos:  $RAW_CSV"
echo "  Resumen:       $SUMMARY_CSV  (columnas: version,N,hilos,tiempo_promedio_seg,speedup,eficiencia)"