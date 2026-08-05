# Glosario — todos los acrónimos, desglosados

Un acrónimo sin desglosar es una palabra mágica que memorizas. Uno desglosado casi siempre se
explica solo. Aquí está desglosada letra por letra cada abreviatura usada en estos apuntes.

Los acrónimos son ingleses, así que el desglose es en inglés y la explicación en castellano — que
es exactamente como te los vas a encontrar en las man pages y en el subject.

Ir a: [Protocolos](#protocolos) · [Constantes de socket](#constantes-de-socket) ·
[Opciones de socket](#opciones-de-socket) · [Campos de cabecera](#campos-de-cabecera) ·
[Errores](#códigos-de-error) · [Señales y tiempo](#señales-y-tiempo) ·
[Estadísticas](#estadísticas-y-salida) · [Herramientas](#herramientas-y-estándares) ·
[Expresiones en lenguaje llano](#expresiones-frecuentes-en-lenguaje-llano)

---

## Protocolos

| Escrito | Significa | Qué es |
|---|---|---|
| **IP** | **I**nternet **P**rotocol | Lleva un paquete de una máquina a otra a través de redes. Se encarga del direccionamiento y el enrutado; no promete nada sobre la entrega. |
| **ICMP** | **I**nternet **C**ontrol **M**essage **P**rotocol | El sistema de mensajería propio de la red: informes de error y diagnóstico *sobre* IP, transportados dentro de IP. Es lo que usa ping. |
| **TCP** | **T**ransmission **C**ontrol **P**rotocol | Flujo de bytes fiable y ordenado entre dos programas. Retransmite lo que se pierde. |
| **UDP** | **U**ser **D**atagram **P**rotocol | Envías un mensaje y esperas que llegue. Sin retransmisión ni ordenación. |
| **ARP** | **A**ddress **R**esolution **P**rotocol | Traduce una dirección IP a una dirección hardware dentro de la red local. |
| **DNS** | **D**omain **N**ame **S**ystem | Convierte `google.com` en una dirección IP. |
| **NTP** | **N**etwork **T**ime **P**rotocol | Sincroniza tu reloj con internet — y puede hacerlo saltar, que es justo por lo que la Etapa 6 evita el reloj de pared. |

---

## Constantes de socket

| Escrito | Significa | Qué es |
|---|---|---|
| **`AF_INET`** | **A**ddress **F**amily: **INET**ernet | "Usa direcciones IPv4". (`AF_INET6` para IPv6.) |
| **`SOCK_STREAM`** | **SOCK**et, **STREAM** | Flujo continuo de bytes. Estilo TCP. |
| **`SOCK_DGRAM`** | **SOCK**et, **D**ata**GRAM** | Un mensaje cada vez, conservando sus límites. Estilo UDP. |
| **`SOCK_RAW`** | **SOCK**et, **RAW** | Acceso sin procesar: construyes y lees los paquetes tú mismo. |
| **`IPPROTO_ICMP`** | **IP** **PROTO**col: **ICMP** | Qué protocolo quieres dentro de esa familia y ese tipo. |
| **`INADDR_ANY`** | **IN**ternet **ADDR**ess: **ANY** | "Cualquier dirección local" — escuchar en todas las interfaces. |
| **`CAP_NET_RAW`** | **CAP**ability, **NET**work, **RAW** | El permiso concreto que habilita los raw sockets. Lo tiene root, o se concede por separado. |

**"Capability"** aquí significa *un poder concreto*, en lugar de permisos de administrador en
bloque. Linux dividió "root puede hacer cualquier cosa" en una lista de permisos independientes,
para poder dar a un programa exactamente el que necesita. `CAP_NET_RAW` es el de los raw sockets.

---

## Opciones de socket

| Escrito | Significa | Qué es |
|---|---|---|
| **`IP_HDRINCL`** | **IP** **H**ea**D**e**R** **INCL**uded | "La cabecera IP la construyo yo, no me la montes tú". |
| **`IP_TTL`** | **IP** **T**ime **T**o **L**ive | Fija el contador de saltos de los paquetes que envías. El flag `-T`. |
| **`IP_OPTIONS`** | **IP** **OPTIONS** | Los campos opcionales al final de una cabecera IP. |
| **`SO_DONTROUTE`** | **S**ocket **O**ption: **DON'T ROUTE** | Sáltate la tabla de rutas; envía solo a una red directamente conectada. El flag `-r`. |
| **`SO_RCVBUF`** | **S**ocket **O**ption: **R**e**C**ei**V**e **BUF**fer | Cuántos datos entrantes te guarda el kernel. |
| **`MSG_TRUNC`** | **M**e**S**sa**G**e **TRUNC**ated | Flag que significa "el paquete era mayor que tu buffer; el resto se ha perdido". |

---

## Campos de cabecera

| Escrito | Significa | Qué es |
|---|---|---|
| **IHL** | **I**nternet **H**eader **L**ength | Cuánto mide la cabecera IP — **contado en grupos de 4 bytes**, así que hay que multiplicar por 4. Normalmente 5, es decir 20 bytes. |
| **TTL** | **T**ime **T**o **L**ive | Pese al nombre cuenta *saltos*, no segundos. Cada router lo decrementa; al llegar a cero el paquete muere y recibes un Time Exceeded. |
| **TOS** | **T**ype **O**f **S**ervice | Pistas de prioridad y trato. Hoy reutilizado en gran medida como DSCP. |
| **DSCP** | **D**ifferentiated **S**ervices **C**ode **P**oint | Sustituto moderno de TOS — marcado de clase de tráfico. |
| **MTU** | **M**aximum **T**ransmission **U**nit | El paquete más grande que transporta un enlace, típicamente 1500 bytes en Ethernet. Si lo superas, el paquete se parte (fragmentación). Relevante para el flag `-s`. |
| **FQDN** | **F**ully **Q**ualified **D**omain **N**ame | Un nombre de host completo, con todos sus dominios: `www.example.com`, no solo `www`. |

---

## Códigos de error

Las constantes de error empiezan todas por **`E`** de **E**rror.

| Escrito | Significa | Cuándo aparece |
|---|---|---|
| **`EPERM`** | **E**rror: **PERM**ission denied | Al abrir un raw socket sin root ni `CAP_NET_RAW`. |
| **`EINTR`** | **E**rror: **INTR**errupted | Llegó una señal en mitad de una syscall. **No es un error real** — reintenta o sal a propósito. Etapa 7. |
| **`EAGAIN`** | **E**rror: try **AGAIN** | Ahora mismo no hay nada que leer en un socket no bloqueante. |
| **`EACCES`** | **E**rror: **ACCES**s denied | Problema de permisos, de origen distinto a `EPERM`. |
| **`EINVAL`** | **E**rror: **INVAL**id argument | Le has pasado algo sin sentido. |
| **`EMSGSIZE`** | **E**rror: **M**e**S**sa**G**e **SIZE** | Paquete demasiado grande para enviarlo; suele ser cosa de la MTU. |
| **`errno`** | **err**or **n**umber | La global con el último error. Solo tiene sentido *inmediatamente* después de la llamada fallida. |
| **`perror`** | **p**rint **error** | Imprime tu mensaje seguido del texto del `errno` actual. |
| **`strerror`** | **str**ing for **error** | Devuelve el texto del error como cadena, para que lo formatees tú. |

---

## Señales y tiempo

| Escrito | Significa | Qué es |
|---|---|---|
| **`SIGINT`** | **SIG**nal: **INT**errupt | Lo que envía Ctrl+C. |
| **`SIGALRM`** | **SIG**nal: **AL**a**RM** | Señal de temporizador vencido. |
| **`SA_RESTART`** | **S**igaction **A**ction: **RESTART** | Flag que decide si una syscall interrumpida se reanuda sola o devuelve `EINTR`. Es una decisión real, no un valor por defecto. |
| **`sig_atomic_t`** | **sig**nal **atomic** **t**ype | Un tipo que se lee y escribe en un solo paso indivisible, para que una señal no lo pille a medias. Lo único que puedes tocar dentro de un handler. |
| **`CLOCK_MONOTONIC`** | **CLOCK**, **MONOTONIC** (solo crece) | Un reloj que nunca salta hacia atrás. El correcto para medir duraciones. |
| **`CLOCK_REALTIME`** | **CLOCK**, **REAL** wall-clock **TIME** | Fecha y hora reales — pueden saltar cuando NTP corrige. Incorrecto para medir duraciones. |
| **`timespec`** | **time** **spec**ification | Struct con segundos + nanosegundos. |
| **Async-signal-safe** | **Asynchronous**-signal-safe | Seguro de llamar dentro de un signal handler. `printf` **no** lo es. |

---

## Estadísticas y salida

| Escrito | Significa | Qué es |
|---|---|---|
| **RTT** | **R**ound-**T**rip **T**ime | Tiempo desde que envías una petición hasta que recibes su respuesta. |
| **mdev** | **m**ean **dev**iation | Cuánto se desvían los RTT individuales de la media. Bajo = estable; alto = con jitter. |
| **stddev** | **st**andard **dev**iation | Idea parecida; ping reporta mdev en concreto. |
| **DUP!** | **DUP**licate | El mismo sequence number ha vuelto más de una vez. |

---

## Herramientas y estándares

| Escrito | Significa | Qué es |
|---|---|---|
| **RFC** | **R**equest **F**or **C**omments | Los documentos que definen los protocolos de internet. Pese al nombre modesto, **son** los estándares. El RFC 792 define ICMP. |
| **IANA** | **I**nternet **A**ssigned **N**umbers **A**uthority | Mantiene el registro oficial de números de protocolo, tipos ICMP y puertos. |
| **ASan** | **A**ddress **San**itizer | Función del compilador (`-fsanitize=address`) que detecta desbordamientos de buffer y use-after-free en tiempo de ejecución. |
| **GCC** | **G**NU **C**ompiler **C**ollection | El compilador. |
| **GNU** | **G**NU's **N**ot **U**nix | Chiste recursivo. El proyecto detrás de `make`, `gcc` y el paquete inetutils contra el que comparas tu salida. |
| **LCOV** | **L**inux **C**overage tool | Formato de informe de cobertura de tests. |
| **`-MMD -MP`** | **M**ake **M**ake **D**ependencies / **M**ake **P**hony | Flags del compilador que generan automáticamente los archivos de dependencias de cabeceras, para que `make` recompile bien cuando cambia un `.h`. |
| **setuid** | **set** **u**ser **id**entity | Un binario que se ejecuta como su propietario y no como quien lo lanzó. Una forma de obtener permiso para raw sockets. |
| **sysctl** | **sys**tem **c**on**t**ro**l** | Ajustes del kernel en caliente, p. ej. `net.ipv4.ping_group_range`. |
| **PID** | **P**rocess **ID**entifier | El número que el sistema asigna a tu programa en ejecución. |
| **CLI** | **C**ommand-**L**ine **I**nterface | Los argumentos y flags que el usuario escribe después del nombre del programa. |

---

## Expresiones frecuentes en lenguaje llano

**"Inject traffic"** (inyectar tráfico) — poner en la red paquetes que has compuesto tú mismo, byte
a byte, en lugar de entregar datos al sistema y dejar que él monte el paquete alrededor.

**"Arbitrary"** (arbitrario) — cualquier cosa, sin restricciones. No "de un conjunto fijo de
opciones".

**"IP-level"** / **"network-layer"** (nivel IP, capa de red) — operar en la capa que mueve paquetes
entre máquinas, por debajo de la capa que normalmente mantiene separados a los programas.

**"Traffic"** (tráfico) — paquetes circulando por la red. Mismo uso que el tráfico de coches.

**"Gated behind"** (restringido tras) — para conseguirlo tienes que pasar una comprobación de
permisos.

**"Demultiplexing"** (demultiplexado) — repartir un único flujo entrante hacia su destino correcto.
Los puertos hacen esto para TCP y UDP. ICMP no tiene puertos, así que **lo haces tú a mano** con el
identifier y el sequence number.

**"Encapsulation"** (encapsulado) — envolver el mensaje de un protocolo dentro del de otro, como
una carta dentro de un sobre. Tu mensaje ICMP va encapsulado en un paquete IP.

**"Byte order" / "endianness"** (orden de bytes) — por qué extremo empieza un número de varios
bytes. Las redes acordaron un orden ("network byte order"); tu máquina puede usar el otro.
`htons` = **h**ost **to** **n**etwork **s**hort; `ntohs` = **n**etwork **to** **h**ost **s**hort.
"Short" significa un número de 2 bytes.

**"Reentrant"** (reentrante) — seguro de volver a llamar mientras una llamada anterior sigue en
curso. Los signal handlers lo necesitan porque interrumpen el código en puntos arbitrarios.

**"One's complement"** (complemento a uno) — invertir todos los bits (los 0 pasan a 1 y al revés).
El checksum se guarda invertido, y eso es lo que hace que en recepción funcione el truco de
"súmalo todo y comprueba que da cero".

**"End-around carry"** (acarreo circular) — cuando la suma se desborda más allá de 16 bits, sumas
ese desbordamiento por abajo en lugar de descartarlo.

**"Pseudo-header"** (pseudocabecera) — campos adicionales (IP origen, IP destino, protocolo,
longitud) que TCP y UDP mezclan en sus checksums. **ICMP no usa ninguna** — fuente habitual de
confusión.

**"Spoofing"** (suplantación) — poner la dirección de otro en el campo "de", para que las
respuestas le lleguen a él.

**"Payload"** (carga útil) — los datos que transporta el paquete, por detrás de la cabecera.

**"Raw"** (crudo, sin procesar) — sin la capa de comodidad que el sistema pone normalmente por
encima.
