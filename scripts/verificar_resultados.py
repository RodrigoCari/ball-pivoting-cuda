# Integrantes: IVAN ALEXANDER LOPEZ ZEGARRA | JESUS BRUNO CHANCAYAURI SONCCO | JOSE RODRIGO CARI ALMIRON
# Curso: Computación Gráfica - Laboratorio Ball Pivoting
import argparse
import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EVIDENCIAS = ROOT / 'evidencias'


def leer_csv(path):
    with path.open(newline='', encoding='utf-8-sig') as archivo:
        return list(csv.DictReader(archivo))


def clave(row):
    return (int(row['puntos']), round(float(row['radio']), 3))


def comparar(cpu, gpu):
    filas_cpu = {clave(r): r for r in leer_csv(cpu)}
    filas_gpu = {clave(r): r for r in leer_csv(gpu)}
    if not filas_cpu or filas_cpu.keys() != filas_gpu.keys():
        raise AssertionError('Los casos CPU y CUDA no coinciden')
    for caso in sorted(filas_cpu):
        a, b = filas_cpu[caso], filas_gpu[caso]
        if (a['triangulos'], a['hash']) != (b['triangulos'], b['hash']):
            raise AssertionError(f'La malla no coincide: {caso}')
        print(f'{caso[0]:5d} puntos, radio {caso[1]:.3f}: '
              f'{a["triangulos"]} triangulos; hash {a["hash"]}; '
              f'CPU {a["tiempo_ms"]} ms / CUDA {b["tiempo_ms"]} ms')
    return filas_cpu, filas_gpu


def leer_log(path):
    text = path.read_text(encoding='utf-8')
    def dato(etiqueta):
        match = re.search(rf'^{re.escape(etiqueta)}:\s*(\S+)', text, re.MULTILINE)
        if not match:
            raise AssertionError(f'{path.name}: falta {etiqueta}')
        return match.group(1)
    return {
        'puntos': int(dato('Puntos cargados')),
        'triangulos': dato('TRIANGULOS'),
        'hash': dato('MALLA_HASH'),
        'tiempo_ms': dato('TIEMPO_CPU_MS' if path.name.startswith('CPU') else 'TIEMPO_HIBRIDO_MS'),
    }


def verificar_evidencias():
    result_dir = EVIDENCIAS / 'resultados'
    capturas = sorted((EVIDENCIAS / 'capturas').glob('*.png'))
    assert len(capturas) == 11, f'Se esperaban 11 capturas; hay {len(capturas)}'
    cpu_csv = result_dir / 'metricas.csv'
    cuda_csv = result_dir / 'metricas_cuda.csv'
    cpu, cuda = comparar(cpu_csv, cuda_csv)
    validacion = {clave(r): r for r in leer_csv(result_dir / 'validacion.csv')}
    assert set(cpu) == set(validacion), 'Los casos en validacion.csv no coinciden'
    for caso in sorted(cpu):
        a,b,v = cpu[caso],cuda[caso],validacion[caso]
        n, radio = caso
        radio_tag = f'{radio:.3f}'.replace('.', '')
        for tipo,row in [('CPU',a),('CUDA',b)]:
            log = result_dir / f'{tipo}_{n}_{radio_tag}.log'
            assert log.is_file(), f'Falta el registro {log.name}'
            datos = leer_log(log)
            for campo in ('puntos', 'triangulos','hash','tiempo_ms'):
                assert str(datos[campo]) == str(row[campo]), f'{log.name}: difiere {campo}'
        assert v['estado'] == 'COINCIDE', f'Validacion fallida: {caso}'
        assert a['triangulos'] == v['triangulos_cpu'] == v['triangulos_cuda'], 'Diferencia de caras'
        assert a['hash'] == v['hash_cpu'] == v['hash_cuda'], 'Diferencia de hashes'
    print(f'Validacion correcta: {len(cpu)} casos, {len(capturas)} capturas, 8 registros')


def prueba_cpu(path):
    filas = leer_csv(path)
    assert len(filas) == 1, 'La prueba rapida debe contener un caso'
    fila = filas[0]
    referencia = {clave(r): r for r in leer_csv(EVIDENCIAS/'resultados'/'metricas.csv')}
    key = clave(fila)
    assert key == (2503, 0.007), f'Caso inesperado: {key}'
    assert fila['triangulos'] == referencia[key]['triangulos'], 'Cambio en numero de triangulos'
    assert fila['hash'] == referencia[key]['hash'], 'Cambio en la malla'
    print('Prueba CPU correcta: 2503 puntos, 6564 triangulos, hash coincidente')


def main():
    parser = argparse.ArgumentParser(description='Comprueba los resultados de Ball Pivoting')
    grupo = parser.add_mutually_exclusive_group()
    grupo.add_argument('--comparar', nargs=2, type=Path, metavar=('CPU_CSV', 'CUDA_CSV'))
    grupo.add_argument('--prueba-cpu', type=Path, metavar='CSV')
    args = parser.parse_args()
    if args.comparar:
        comparar(*args.comparar)
    elif args.prueba_cpu:
        prueba_cpu(args.prueba_cpu)
    else:
        verificar_evidencias()


if __name__ == '__main__':
    main()
