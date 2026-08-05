# Etapa 5 — Recibir y parsear la respuesta

**Requisito previo:** [04-construir-enviar.md](04-construir-enviar.md) · **Siguiente:** [06-tiempos-rtt.md](06-tiempos-rtt.md)

---

**Versión llana:** como estás sobre un raw socket, `recvfrom()` no te entrega solo el mensaje ICMP:
te entrega el *paquete IP completo*, cabecera IP incluida, envolviendo esa respuesta ICMP.

**Parsear la cabecera IP:** necesitas un struct para la cabecera IP (`struct ip` en `netinet/ip.h`,
o el tuyo propio), y en concreto su campo **IHL** — **I**nternet **H**eader **L**ength. Te dice
cuántas palabras de 32 bits mide la cabecera IP *de ese paquete concreto* (varía cuando hay
opciones IP presentes), para que sepas exactamente dónde empieza el payload ICMP.

**Reconocer tu propia respuesta:** después de saltarte la cabecera IP, parsea los bytes restantes
como tu struct de cabecera ICMP, comprueba type/code, y compara el identifier y el sequence number
con lo que enviaste — confirmando que es realmente una respuesta a *tu* petición, y no el ping de
otro proceso ni un paquete perdido.

> **Nombres, desglosados** — `recvfrom` = **rec**ei**v**e **from** (recibir de; también te devuelve
> la dirección del emisor) · **IHL** = **I**nternet **H**eader **L**ength · **TTL** = **T**ime
> **T**o **L**ive (cuenta *saltos*, no segundos — cada router lo decrementa) · `ICMP_ECHOREPLY` =
> la constante del type **ECHO REPLY** de ICMP, valor 0 · `MSG_TRUNC` = **M**e**S**sa**G**e
> **TRUNC**ated, el flag que significa "el paquete era mayor que tu buffer y el resto se ha
> perdido" · **RTT** = **R**ound-**T**rip **T**ime. Lista completa en [GLOSARIO.md](GLOSARIO.md).
>
> **"Datagrama"** aquí significa simplemente un paquete autocontenido, por oposición a un flujo
> continuo. **"Granularidad"** significa el tamaño de la unidad con la que trabajas — un raw socket
> trabaja con paquetes enteros.

---

## Preguntas de control

<details><summary>P1: ¿Por qué recvfrom() te entrega más de lo que esperabas del mensaje ICMP?</summary>

Un raw socket ICMP recibe con granularidad de capa IP: obtienes el datagrama IP completo (cabecera
+ payload), no un flujo ya filtrado por la capa de transporte. El mensaje ICMP está después de la
cabecera IP, no ocupa el buffer entero.
</details>

<details><summary>P2: ¿Por qué no puedes suponer que la cabecera IP mide siempre exactamente 20 bytes?</summary>

La longitud de la cabecera IP es variable por culpa de los campos opcionales. El campo IHL da la
longitud real en palabras de 32 bits de ese paquete concreto — codificar 20 bytes a fuego rompe
con cualquier paquete que lleve opciones.
</details>

---

## Ejercicio

Cierra el ciclo: un envío, una recepción, una línea impresa.

1. Haz `recvfrom()` sobre un buffer holgadamente mayor que cualquier paquete esperado. **Comprueba
   la longitud devuelta antes de tocar nada** — parsear una lectura corta como si fuera una cabecera
   IP completa es una lectura fuera de límites directa.
2. Extrae el IHL: `ip_hl * 4` te da el desplazamiento en bytes. Imprímelo. En una respuesta normal
   será 20; fíjate en que lo has *calculado*, no supuesto.
3. **Valida antes de indexar.** En este orden: ¿es `n >= sizeof(struct ip)`? ¿es `ihl >= 20`? ¿es
   `n >= ihl + sizeof(struct icmphdr)`? Solo entonces leas la cabecera ICMP. Saltarte cualquiera de
   estas es un crash que la evaluación encontrará.
4. Filtra: `type == ICMP_ECHOREPLY (0)`, `id == tu id`, `seq == el esperado`. **Rompe el filtro del
   id a propósito** (pon un id equivocado a fuego) y confirma que rechazas correctamente la
   respuesta — eso demuestra que el filtro se ejecuta de verdad.
5. Imprime una sola línea parecida a
   `64 bytes from 127.0.0.1: icmp_seq=1 ttl=64` (el TTL sale del campo `ip_ttl` de la cabecera IP).
   El RTT llega en la Etapa 6.
6. Ejecútalo con ASan contra localhost y contra un host real.

**Terminado cuando:** un ciclo de ping completo se imprima correctamente, y ASan esté limpio tanto
con una respuesta buena como con una truncada o con id equivocado.

> **Trampa:** en Linux el raw socket entrega *todos* los paquetes ICMP que recibe el host, no solo
> los tuyos. Tu filtro no es una cortesía opcional: sin él imprimirás las respuestas de otros
> procesos.

---

## Lecturas

- `man 2 recvfrom` — semántica del valor de retorno, en especial los casos `0` y `-1`, y `MSG_TRUNC`
- `man 7 raw` — relee el párrafo de "la cabecera IP siempre viene incluida" ahora que te importa
- `/usr/include/netinet/ip.h` — el `struct ip` real; fíjate en que `ip_hl` es un campo de bits y su
  posición depende del endianness (lee el bloque `#if __BYTE_ORDER`)
- **RFC 791** §3.1 — el diagrama de la cabecera IP, en concreto IHL y Options
