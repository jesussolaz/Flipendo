# Compara dos tandas de capturas (lineas "clave WxH huella") y lista las diferencias.
#
# No es codigo del motor: es la herramienta con la que se juzga si un dibujo nativo sale
# igual que el de Python. Si se le pasan los directorios con las imagenes .npy de las
# claves que difieren, genera ademas una imagen por diferencia con tres paneles: antes,
# despues y los pixeles distintos en rojo, para poder ver QUE cambia.
#
# Uso:  python3 compare_captures.py <antes.txt> <despues.txt> [<dir_antes> <dir_despues> <dir_salida>]
import os
import sys
from collections import defaultdict


def leer(path):
    datos = {}
    for linea in open(path):
        linea = linea.rstrip("\n")
        partes = linea.rsplit(" ", 2)
        if len(partes) == 3 and len(partes[2]) == 40 and "x" in partes[1]:
            datos[partes[0]] = (partes[1], partes[2])
        else:
            datos[linea] = None
    return datos


antes, despues = leer(sys.argv[1]), leer(sys.argv[2])
iguales, distintas, solo_antes, solo_despues = [], [], [], []
for clave, valor in antes.items():
    if clave not in despues:
        solo_antes.append(clave)
    elif valor != despues[clave]:
        distintas.append(clave)
    else:
        iguales.append(clave)
solo_despues = [c for c in despues if c not in antes]

print("iguales:   %d" % len(iguales))
print("distintas: %d" % len(distintas))
print("solo antes / solo despues: %d / %d" % (len(solo_antes), len(solo_despues)))
por_herramienta = defaultdict(list)
for clave in distintas:
    partes = clave.split(" ")
    por_herramienta[" ".join(partes[:-1])].append(partes[-1])
for herramienta in sorted(por_herramienta):
    print("  DIFIERE %-60s %s" % (herramienta, ",".join(por_herramienta[herramienta])))
for clave in solo_antes + solo_despues:
    print("  SOLO EN UNA %s" % clave)

if len(sys.argv) > 5:
    import numpy as np
    dir_a, dir_d, dir_out = sys.argv[3], sys.argv[4], sys.argv[5]
    os.makedirs(dir_out, exist_ok=True)
    try:
        from PIL import Image
    except ImportError:
        Image = None

    def png_escribir(path, rgba):
        # PNG minimo con la biblioteca estandar (zlib), para no depender de PIL.
        import struct
        import zlib
        h, w = rgba.shape[:2]
        crudo = b"".join(b"\x00" + rgba[y].tobytes() for y in range(h))

        def trozo(tipo, datos):
            c = struct.pack(">I", len(datos)) + tipo + datos
            return c + struct.pack(">I", zlib.crc32(tipo + datos) & 0xFFFFFFFF)
        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n")
            f.write(trozo(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)))
            f.write(trozo(b"IDAT", zlib.compress(crudo, 6)))
            f.write(trozo(b"IEND", b""))
    for clave in distintas:
        nombre = clave.replace(" ", "__") + ".npy"
        fa, fd = os.path.join(dir_a, nombre), os.path.join(dir_d, nombre)
        if not (os.path.exists(fa) and os.path.exists(fd)):
            continue
        a, d = np.load(fa), np.load(fd)
        h, w = max(a.shape[0], d.shape[0]), max(a.shape[1], d.shape[1])
        pa = np.zeros((h, w, 4), np.uint8); pa[:a.shape[0], :a.shape[1]] = a
        pd = np.zeros((h, w, 4), np.uint8); pd[:d.shape[0], :d.shape[1]] = d
        mask = np.any(pa != pd, axis=2)
        diff = pd.copy(); diff[mask] = (255, 0, 0, 255)
        panel = np.concatenate([pa, pd, diff], axis=1)[::-1]  # Blender guarda de abajo arriba.
        salida = os.path.join(dir_out, clave.replace(" ", "__") + ".png")
        if Image is not None:
            Image.fromarray(panel, "RGBA").save(salida)
        else:
            png_escribir(salida, np.ascontiguousarray(panel))
        print("  imagen: %s  (%d pixeles distintos)" % (salida, int(mask.sum())))
