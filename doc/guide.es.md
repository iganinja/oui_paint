# OUI Paint

Un editor de imágenes de mapa de bits al estilo de Paint.NET: capas, selecciones que se pueden mover, girar y escalar,
las herramientas y las teclas de Paint.NET, 8 y 16 bits por canal, imágenes indexadas con paletas, y efectos escritos
como plugins.

## La ventana

- **Las herramientas** bajan por la izquierda, con los colores primario y secundario y una paleta de colores debajo. La
  letra de una herramienta la elige; pulsarla otra vez pasa a la siguiente que la comparte, como la S recorre las
  herramientas de selección.
- **Las opciones de la herramienta**, como el ancho del pincel, están en la barra bajo los menús.
- **Cada imagen abierta** tiene una pestaña sobre el lienzo. Ctrl+Tab pasa a la siguiente.
- **El historial** está arriba a la derecha: la imagen como era al principio, y luego cada paso. Un clic en uno lleva
  la imagen hacia atrás o hacia delante hasta él, y las flechas de debajo deshacen y rehacen. F6, o Ver > Panel de
  historial, lo oculta y lo muestra.
- **Las capas** están debajo y, en una imagen indexada, sus paletas debajo de ellas. F7, o Ver > Panel de capas, oculta
  y muestra todo el lado derecho. Los dos se quedan como se dejaron para la próxima vez.
- **La barra de estado** dice dónde está el puntero en la imagen, su tamaño y su tipo, y el zoom.

El botón izquierdo pinta con el color primario y el derecho con el secundario. Ver > Interfaz más grande e Interfaz más
pequeña cambian el tamaño de todo menos de la imagen; el tamaño se guarda para la próxima vez.

## Moverse

| Teclas o ratón | Qué hace |
|---|---|
| Ctrl+rueda | Zoom alrededor del puntero |
| Ctrl++, Ctrl+- | Acercar, alejar |
| Ctrl+B | Ajustar a la ventana |
| Ctrl+Mayús+B | Ajustar a lo seleccionado |
| Ctrl+0 | Tamaño real |
| Rueda, Mayús+rueda | Desplazar arriba y abajo, a los lados |
| Espacio+arrastrar, arrastrar con el botón central, o la herramienta Desplazar (H) | Desplazar |

## Selecciones

Selección rectangular, Selección con lazo, Selección elíptica y la Varita mágica eligen sobre qué trabajan las demás
herramientas y los efectos. Mientras se arrastra:

| Ratón | Qué hace |
|---|---|
| Arrastrar | Una selección nueva |
| Ctrl+arrastrar | Añadir a la selección |
| Alt+arrastrar | Quitarle |
| Alt+arrastrar con el botón derecho | Quedarse solo con lo que cubren las dos |
| Ctrl+arrastrar con el botón derecho | Invertir lo que cubre |
| Mayús | Mantener un rectángulo o una elipse cuadrado o redonda |

Ctrl+A lo selecciona todo, Ctrl+D anula la selección, Ctrl+I la invierte. Supr borra lo seleccionado y Retroceso lo
rellena con el color primario.

**Mover píxeles seleccionados** (M) levanta lo seleccionado para poder arrastrarlo, escalarlo con sus asas y girarlo
arrastrando con el botón derecho; Mayús mantiene sus proporciones al escalar y lo gira de 15 en 15 grados. **Mover
selección** (M otra vez) mueve solo el contorno. Edición > Espejar selección y Girar selección espejan y giran lo
seleccionado en su sitio.

## Pintar

| Herramienta | Tecla | Qué hace |
|---|---|---|
| Pincel | B | Suave o duro según su dureza, bordes lisos con el suavizado |
| Lápiz | P | De un píxel de ancho, nunca suavizado |
| Goma de borrar | E | Pinta transparencia |
| Bote de pintura | F | Rellena la zona de color parecido |
| Degradado | G | Desde donde empieza el arrastre hasta donde acaba |
| Tampón de clonar | L | Ctrl+clic elige de dónde copia, y luego al pintar copia de allí |
| Recolorear | R | Cambia el color secundario por el primario donde pinta; con el botón derecho, al revés |
| Cuentagotas | K | Toma el color bajo el puntero: el izquierdo para el primario, el derecho para el secundario |
| Texto | T | Escribe texto donde se hace clic |
| Línea / curva | O | Arrastrar de un extremo al otro, Mayús de 15 en 15 grados; luego arrastrar sus cuatro asas para curvarla, y Intro, o un clic lejos de ellas, la deja como está |
| Formas | O otra vez | Arrastrar de una esquina a la otra; Mayús las mantiene cuadradas o redondas |
| Arco | O una tercera vez | Como con un compás: un clic pone el centro, otro el radio y dónde empieza, y el puntero lo gira, hacia cualquier lado, hasta la circunferencia entera, hasta que un tercer clic lo deja; Mayús de 15 en 15 grados, Esc lo descarta |

[ y ] hacen el pincel más estrecho y más ancho; con Ctrl, de 5 en 5. X intercambia los colores primario y secundario.
C elige cuál de los dos pone un clic en una paleta, el que está enmarcado delante; un clic derecho pone el otro.

## Capas

Cada capa tiene un nombre, se puede mostrar u ocultar, y tiene una opacidad y un modo de fusión, como en Paint.NET (F4
abre sus propiedades, que la imagen muestra mientras cambian). En la lista, el ojo a la derecha de una capa la muestra
u oculta y la papelera la elimina; las casillas a la izquierda marcan capas para que el botón de eliminar bajo la lista
las elimine juntas. Los botones bajo la lista añaden, eliminan, duplican, combinan hacia abajo y suben y bajan la
capa. Guardar en un formato de una sola capa, como PNG, guarda la imagen acoplada; las capas se quedan en la imagen
abierta, y OpenRaster (.ora) las guarda.

## Profundidad

Una imagen nueva es de 8 o 16 bits por canal, en color o en escala de grises. 16 bits guarda pasos finos que 8 bits
pierde, como en un mapa de alturas o un degradado suave; PNG guarda 16 bits, JPEG y BMP solo 8, y al guardar se
pregunta antes de perder la diferencia.

## Imágenes indexadas y paletas

En una imagen indexada cada píxel no guarda un color: guarda un número, el lugar de su color en una paleta.

- **Una imagen puede tener varias paletas del mismo tamaño.** Cada capa usa una de ellas. Una imagen indexada nueva
  pregunta cuántas paletas y cuántos colores en cada una.
- **El panel de paletas**, bajo las capas, muestra cada paleta como una fila de colores. La paleta de la capa en la que
  se trabaja está marcada con ">"; un clic en el número de otra paleta se la da a la capa.
- **Un clic en un color** lo elige para pintar, el botón izquierdo para el primario y el derecho para el secundario. Un
  doble clic cambia el color, y la imagen lo muestra mientras se elige: todos los píxeles que lo usan cambian con él.
- **El color transparente** se muestra con "/". Pintar con él borra.
- **Pintar con cualquier otro color** usa el color más próximo de la paleta: una imagen indexada nunca tiene colores
  fuera de sus paletas, y nunca se suaviza.
- **Reordenar las paletas** (Imagen > Paletas > Subir o Bajar la paleta de la capa) las mueve con lo que las usa, lo que
  cambia cómo se ve la imagen cuando se guarda como PNG indexado con todas las paletas en orden.
- **Los colores permitidos** (Imagen > Paletas > Cargar colores permitidos) limitan los colores que puede tomar una
  paleta a una lista, como los que puede mostrar una máquina; un color elegido fuera de ella pasa a ser el permitido
  más próximo.

Las paletas se guardan aparte, como una configuración de paletas en JSON (Imagen > Paletas > Guardar configuración de
paletas); una paleta sola se puede cargar o guardar como .pal, .gpl o .act, o tomar de la paleta de una imagen.

Al abrir un PNG que tiene paleta se pregunta si mantenerlo indexado o abrirlo en colores. Al guardar una imagen
indexada como PNG se pregunta si guardarla indexada, con todas las paletas una tras otra, o en colores; guardada
indexada, se vuelve a abrir con sus paletas como estaban, los píxeles de cada paleta en una capa propia. Imagen > Modo
cambia una imagen abierta a color o escala de grises, de 8 o 16 bits por canal, o a indexada, con el modo en que está
marcado; la imagen muestra el cambio mientras cambian las opciones del diálogo.

- **A indexada** pregunta por una paleta, hecha para la imagen o cargada de un fichero, cuántos colores tiene una
  paleta hecha, y el método: Corte mediano elige tantos colores como se pidan y puede tramar, con Floyd-Steinberg o un
  patrón ordenado, para que los degradados no hagan bandas; Colores exactos mantiene los colores propios de la imagen,
  si no hay más de los pedidos. Todas las capas van a la misma paleta, con el índice 0 transparente cuando la imagen
  tiene píxeles transparentes.
- **A escala de grises** pregunta cuánto cuenta cada uno del rojo, el verde y el azul; **de 16 bits a 8** pregunta si
  redondear cada píxel o tramar para que los degradados suaves no hagan bandas.

Los métodos son plugins, y se pueden escribir más: mira Plugins.

## Ficheros

| Formato | Abre | Guarda |
|---|---|---|
| OpenRaster (.ora) | con sus capas | con sus capas |
| PNG | 8 y 16 bits, y con paleta | 8 y 16 bits, e indexado |
| JPEG, BMP, TGA | sí | sí, a 8 bits |
| GIF | sí | no |
| Mapa de alturas en bruto de 16 bits (.r16, .raw) | sí, preguntando su tamaño | sí |

Copiar y pegar funcionan con otros programas en los dos sentidos. Archivo > Abrir recientes lista los últimos ficheros
abiertos o guardados.

## Efectos

Los ajustes y los efectos actúan sobre la selección, o sobre toda la capa cuando no hay nada seleccionado; sus
diálogos muestran el cambio mientras se mueven los valores, y nunca esperan a que acabe: la imagen se pone al día en
cuanto el efecto está hecho.

- **Curvas** (Ctrl+Mayús+M) dibuja en qué se convierte cada nivel sobre el histograma de la imagen: un clic añade un
  punto, arrastrarlo lo mueve, y un clic derecho o arrastrarlo lejos lo quita. El mapa de transferencia es la
  luminosidad, que mantiene el tono de los colores, o el rojo, el verde y el azul, los marcados editados a la vez.
- **Niveles** (Ctrl+L) tiene el histograma de lo que entra, con marcas para el negro, el gris y el blanco, y lo que sale
  debajo; Automático estira lo que tiene la imagen del negro al blanco. El rojo, el verde y el azul marcados se editan a
  la vez, cada uno con sus propios niveles, igual que con los tres marcados los comparten.

Ctrl+F repite el último efecto. Cada efecto es un plugin, y se pueden añadir más: Efectos > Gestionar plugins los
escribe y los prueba junto a una imagen; mira [Plugins](plugins.md), que está en inglés.

## Ajustes

Edición > Preferencias (Ctrl+,) elige el idioma, el del sistema u otro, que cambia cuando OUI Paint vuelve a empezar;
el tema, Clásico, Océano o Noche, que se ve en cuanto se elige; y si al empezar sin ninguna imagen dada se vuelven a
abrir las que estaban abiertas al cerrarlo.

OUI Paint guarda sus ajustes en `oui_paint.ini`: junto al programa cuando se puede escribir en su carpeta, y en la
carpeta de configuración del usuario cuando no. La carpeta `plugins` de su lado guarda los plugins añadidos.
