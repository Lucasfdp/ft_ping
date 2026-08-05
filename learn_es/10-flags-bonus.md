# Etapa 10 (Bonus) — Flags adicionales

**Requisito previo:** [09-salida-makefile.md](09-salida-makefile.md) — **y una parte obligatoria que
pase por completo.** El subject condiciona la evaluación del bonus a una parte obligatoria perfecta;
lo que hagas aquí no cuenta para nada si algo de lo anterior está incompleto.

---

| Flag | Qué hace | Enlaza con |
|---|---|---|
| `-f` | Modo flood — dispara el siguiente paquete inmediatamente (al recibir respuesta o sin esperar), salida de un punto por paquete | Estructura del bucle, Etapas 4/7 |
| `-l` | Preload — envía N paquetes de golpe antes de esperar ninguna respuesta | Desacople envío/recepción, Etapas 4/5 |
| `-n` | Solo numérico — omite el DNS inverso en la salida, imprime IPs en crudo | Resolución, Etapa 4 |
| `-w` | Deadline — tiempo total de ejecución antes de salir a la fuerza, sin importar el número de paquetes | Lógica de bucle/salida, Etapa 7 |
| `-W` | Timeout de respuesta por paquete | Lógica de bucle/salida, Etapa 7 |
| `-p` | Patrón — rellena los bytes del payload con un patrón hexadecimal dado en lugar de los datos por defecto | Payload, Etapa 2 |
| `-r` | Saltarse el enrutado — `SO_DONTROUTE`, enviar directamente a un host de una red conectada | Nueva sockopt |
| `-s` | Tamaño de paquete — payloads grandes pueden provocar fragmentación IP | Concepto nuevo: MTU/fragmentación |
| `-T`/`--ttl` | Fija el TTL de IP vía `setsockopt(IPPROTO_IP, IP_TTL, ...)` — permite inducir a propósito una respuesta Time Exceeded | Tipos de error ICMP, Etapa 2 |
| `--ip-timestamp` | Añade una *opción* de timestamp a nivel IP en los paquetes salientes | `IP_HDRINCL`, Etapa 1 |

> **Nombres, desglosados** — `IPPROTO_IP` = nivel de **IP** **PROTO**col: **IP** (el argumento de
> `setsockopt` que dice "a qué capa pertenece esta opción") · `setsockopt` = **set** **sock**et
> **opt**ion · `SO_DONTROUTE` = **S**ocket **O**ption: **DON'T ROUTE** · `IP_TTL` = **IP** **T**ime
> **T**o **L**ive (cuenta *saltos*, no segundos) · `IP_HDRINCL` = **IP** **H**ea**D**e**R**
> **INCL**uded · **MTU** = **M**aximum **T**ransmission **U**nit, el paquete más grande que
> transporta un enlace (~1500 bytes en Ethernet). Si lo superas, el paquete se parte — eso es la
> *fragmentación*. Lista completa en [GLOSARIO.md](GLOSARIO.md).

`--ip-timestamp` es el que de verdad te devuelve al terreno de `IP_HDRINCL`: el IP timestamp es una
*opción* de la cabecera IP, no algo expuesto vía un simple `setsockopt`, así que producirlo implica
construir la cabecera IP tú mismo.

---

## Preguntas de control

<details><summary>P1: ¿Qué flag bonus obliga más directamente a volver a IP_HDRINCL, y por qué?</summary>

`--ip-timestamp`. El IP timestamp es una opción de la cabecera IP, no algo que puedas fijar con una
simple llamada a setsockopt — producirlo implica construir la cabecera IP tú mismo con IP_HDRINCL
activado, en lugar de dejar que la monte el kernel.
</details>

<details><summary>P2: ¿Por qué -s puede tocar un concepto que la parte obligatoria nunca te obliga a ver?</summary>

Enviar un payload lo bastante grande como para que el paquete IP resultante supere la MTU de la red
puede provocar fragmentación IP — un comportamiento de capa inferior que la parte obligatoria nunca
te exige pensar. Merece la pena conocerlo conceptualmente aunque tu implementación no gestione el
reensamblado a mano.
</details>

---

## Ejercicio

Impleméntalos en este orden — cada grupo reutiliza la estructura del anterior.

**Grupo 1 — sockopts puras (lo más fácil, sin tocar el bucle):** `-T/--ttl`, `-r`, `-n`
- `-T 1` contra un host lejano debería producir una respuesta **Time Exceeded**. Esta es tu ruta de
  robustez de la Etapa 2 disparándose por fin de verdad — confirma que la informas y sigues
  ejecutando.
- `-n` debe omitir por completo la consulta inversa (verifícalo: tiene que ser *más rápido*, no solo
  tener otro formato).

**Grupo 2 — cambios en el payload:** `-p`, `-s`
- `-p` parsea **hexadecimal proporcionado por el usuario desde argv**. Valida con severidad: rechaza
  caracteres no hexadecimales, rechaza cadenas de longitud impar, y acota la longitud contra tu
  buffer de payload. Parsear sin límite directamente a un buffer fijo es el desbordamiento clásico
  aquí — escribe primero la comprobación de longitud.
- `-s 65500` y `-s 0` son ambos casos límite. Ninguno puede provocar un crash. Ojo con la
  interacción de `-s` con el timestamp de la Etapa 6: si el payload es menor que
  `sizeof(struct timespec)` no tienes dónde meterlo — decide qué haces y gestiónalo.

**Grupo 3 — reestructurar el bucle:** `-w`, `-W`, `-l`, `-f`
- Estos obligan a separar el envío de la recepción. Convierte el `recvfrom` bloqueante en
  `select`/`poll` con timeout, en lugar de ir apilando `alarm()`.
- `-f` requiere root o debe negarse limpiamente — y aun así debe respetar Ctrl+C. Pruébalo solo
  contra `127.0.0.1`; inundar un host que no es tuyo es algo con lo que conviene tener cuidado.
- Extrae las constantes de tiempo: `#define FLOOD_MIN_INTERVAL_US 10000`, etc.

**Grupo 4 — `--ip-timestamp`:** el último, y solo. Activa `IP_HDRINCL`, construye la cabecera IP tú
mismo, calcula el checksum de la cabecera IP (tu función de la Etapa 3 sirve sin cambios) y coloca
la opción con el alineamiento a 4 bytes y el padding correctos.

**Terminado cuando:** cada grupo pase con valgrind limpio y el diff de salida de la parte
obligatoria de la Etapa 9 siga siendo idéntico sin pasar ningún flag. **Repite el diff de la Etapa 9
después de cada grupo** — que el trabajo del bonus haga regresión en la salida obligatoria es la
forma habitual de perder el bonus entero.

---

## Lecturas

- `man 7 ip` — `IP_TTL`, `IP_HDRINCL`, `IP_OPTIONS`
- `man 7 socket` — `SO_DONTROUTE`
- **RFC 791** §3.1 "Options" — formato de la opción Internet Timestamp, alineamiento y reglas de
  desbordamiento
- **RFC 791** §3.2 "Fragmentation and Reassembly" — para `-s`
- `man 2 select` / `man 2 poll` — para el Grupo 3
- `man 8 ping` — la semántica de referencia de cada uno de estos flags; reproduce su comportamiento
- inetutils-2.0 `ping/ping.c` — su tratamiento de opciones, para la paridad de formato de salida
