#!/usr/bin/env bash
# Integrantes: IVAN ALEXANDER LOPEZ ZEGARRA | JESUS BRUNO CHANCAYAURI SONCCO | JOSE RODRIGO CARI ALMIRON
# Curso: Computación Gráfica - Laboratorio Ball Pivoting
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA="$ROOT/Laboratorio_Ball_Pivoting_Completo"
BUILD="$ROOT/build"
RUNS="$ROOT/ejecuciones_locales"
MODE="${1:-ayuda}"
mkdir -p "$BUILD" "$RUNS"

need() { command -v "$1" >/dev/null 2>&1 || { echo "Falta $1" >&2; exit 1; }; }

get_nvcc() {
  if [[ -n "${NVCC:-}" ]]; then printf '%s\n' "$NVCC"; return; fi
  if command -v nvcc >/dev/null 2>&1; then command -v nvcc; return; fi
  if [[ -x /usr/local/cuda/bin/nvcc ]]; then printf '%s\n' /usr/local/cuda/bin/nvcc; return; fi
  return 1
}

compilar_cpu() {
  need g++
  g++ -std=c++17 -O2 "$ROOT/benchmarks/bench_2503.cpp" -o "$BUILD/bench_2503"
  g++ -std=c++17 -O2 "$ROOT/benchmarks/bench_multi.cpp" -o "$BUILD/bench_multi"
}

compilar_cuda() {
  need g++
  local nvcc_cmd
  nvcc_cmd="$(get_nvcc)" || { echo "No se encontró nvcc de Linux" >&2; exit 1; }
  local arch="${CUDA_ARCH:-native}"
  "$nvcc_cmd" -std=c++17 -O2 --fmad=false -arch="$arch" -I "$ROOT/cuda" \
      -c "$ROOT/cuda/bpa_vecindad.cu" -o "$BUILD/bpa_vecindad.o"
  local tipo
  for tipo in 2503 multi; do
    g++ -std=c++17 -O2 -I "$ROOT/cuda" -c "$ROOT/benchmarks/bench_${tipo}_cuda.cpp" \
        -o "$BUILD/bench_${tipo}_cuda.o"
    "$nvcc_cmd" -std=c++17 -O2 -arch="$arch" "$BUILD/bench_${tipo}_cuda.o" \
        "$BUILD/bpa_vecindad.o" -o "$BUILD/bench_${tipo}_cuda"
  done
}

medir() {
  local modo="$1" tipo="$2" cantidad="$3" radio="$4" entrada="$5" csv="$6"
  local ejecutable="bench_${tipo}"
  [[ "$modo" == cuda ]] && ejecutable="${ejecutable}_cuda"
  local log="$RUNS/${modo}_${cantidad}_${radio//./}.log"
  echo "$modo | $cantidad puntos | radio $radio"
  "$BUILD/$ejecutable" "$DATA/$entrada" "$radio" "$csv" | tee "$log"
}

cpu() {
  compilar_cpu
  local csv="$RUNS/metricas_cpu.csv"
  echo 'puntos,radio,tiempo_ms,triangulos,hash' > "$csv"
  medir cpu 2503 2503 0.005 05_BPA_Bunny_2503/Stanford_Bunny_2503.xyz "$csv"
  medir cpu 2503 2503 0.007 05_BPA_Bunny_2503/Stanford_Bunny_2503.xyz "$csv"
  medir cpu 2503 2503 0.010 05_BPA_Bunny_2503/Stanford_Bunny_2503.xyz "$csv"
  medir cpu multi 20000 0.007 06_CPU_vs_CUDA/Stanford_Bunny_20000.xyz "$csv"
}

cuda() {
  compilar_cuda
  local csv="$RUNS/metricas_cuda.csv"
  echo 'puntos,radio,tiempo_ms,triangulos,hash,vecindad_gpu_ms,kernels_ms' > "$csv"
  medir cuda 2503 2503 0.005 05_BPA_Bunny_2503/Stanford_Bunny_2503.xyz "$csv"
  medir cuda 2503 2503 0.007 05_BPA_Bunny_2503/Stanford_Bunny_2503.xyz "$csv"
  medir cuda 2503 2503 0.010 05_BPA_Bunny_2503/Stanford_Bunny_2503.xyz "$csv"
  medir cuda multi 20000 0.007 06_CPU_vs_CUDA/Stanford_Bunny_20000.xyz "$csv"
}

probar_cpu() {
  compilar_cpu
  local csv="$RUNS/prueba_cpu.csv"
  echo 'puntos,radio,tiempo_ms,triangulos,hash' > "$csv"
  medir cpu 2503 2503 0.007 05_BPA_Bunny_2503/Stanford_Bunny_2503.xyz "$csv"
  python3 "$ROOT/scripts/verificar_resultados.py" --prueba-cpu "$csv"
}

visualizar() {
  need g++
  local -a dirs=(01_BPA_2D 01_BPA_2D 02_BPA_3D_Pedagogico 02_BPA_3D_Pedagogico 04_Stanford_Bunny 05_BPA_Bunny_2503 06_CPU_vs_CUDA)
  local -a fuentes=(BallPivoting2D_OpenGL.cpp BallPivoting2D_Malla_OpenGL.cpp BallPivoting3D_Pedagogico_OpenGL.cpp Nube3D_20Puntos_OpenGL.cpp 01_Visualizador_Stanford_Bunny_CENTRADO.cpp 02_Stanford_Bunny_BPA_2503_CENTRADO.cpp 03_Stanford_Bunny_BPA_MULTISEMILLA_CENTRADO.cpp)
  local -a exes=(bpa_2d bpa_malla_2d bpa_3d nube20 bunny_ref bunny_bpa bunny_multi)
  local i
  for i in "${!exes[@]}"; do
    g++ -std=c++17 -O2 "$DATA/${dirs[$i]}/${fuentes[$i]}" \
        -o "$BUILD/${exes[$i]}" -lglut -lGL -lGLU
  done
  for i in "${!exes[@]}"; do
    echo "Abriendo ${exes[$i]} (ESC para pasar al siguiente)"
    (cd "$DATA/${dirs[$i]}" && "$BUILD/${exes[$i]}")
  done
}

diagnostico() {
  echo 'Sistema:'; uname -sr
  echo 'g++:'; if command -v g++ >/dev/null; then g++ --version | head -1; else echo 'No instalado'; fi
  echo 'nvcc:'; if nvcc_cmd="$(get_nvcc)"; then "$nvcc_cmd" --version | tail -4; else echo 'No instalado'; fi
  echo 'GPU:'; if command -v nvidia-smi >/dev/null; then nvidia-smi -L || true; else echo 'Sin nvidia-smi'; fi
  echo "DISPLAY=${DISPLAY:-no configurado}"
}

case "$MODE" in
  diagnostico) diagnostico ;;
  compilar-cpu) compilar_cpu ;;
  compilar-cuda) compilar_cuda ;;
  cpu) cpu ;;
  cuda) cuda ;;
  comparar) cpu; cuda; python3 "$ROOT/scripts/verificar_resultados.py" --comparar "$RUNS/metricas_cpu.csv" "$RUNS/metricas_cuda.csv" ;;
  prueba-cpu) need python3; probar_cpu ;;
  visuales) visualizar ;;
  verificar) need python3; python3 "$ROOT/scripts/verificar_resultados.py" ;;
  ayuda|help|-h|--help)
    cat <<'AYUDA'
Uso: bash ejecutar_pruebas.sh <modo>
  diagnostico     Herramientas disponibles y GPU
  compilar-cpu    Compilar los benchmarks CPU
  compilar-cuda   Compilar los benchmarks CUDA (requiere nvcc)
  prueba-cpu      Ejecutar una prueba corta con 2503 puntos y validar la malla
  cpu             Cuatro mediciones CPU (2503/20000 puntos)
  cuda            Cuatro mediciones CPU+CUDA (2503/20000 puntos)
  comparar        Ejecutar CPU y CUDA y comparar sus mallas
  visuales        Compilar y abrir los siete programas OpenGL
  verificar       Comprobar las evidencias guardadas en el repositorio

Los resultados nuevos quedan en ejecuciones_locales/ y no sustituyen las evidencias.
AYUDA
    ;;
  *) echo "Modo desconocido: $MODE" >&2; exit 2 ;;
esac
