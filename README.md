# Bitcoin Fee Visualizer

Visualizador en tiempo real de las tarifas de transacción de Bitcoin con soporte para múltiples fuentes de datos.

## Características
- Múltiples fuentes de datos (mempool.space, blockstream.info, bitcoinfees.earn.com)
- Caché local para funcionamiento sin conexión
- Exportación de datos a CSV
- Interfaz en tiempo real con ncurses

## Requisitos
- gcc
- libcurl
- libcjson
- ncurses

## Instalación
```bash
make
```

## Uso
```bash
./btc_fee_visualizer
```

### Controles
- `q`: Salir
- `r`: Actualizar manualmente
- `h`: Alternar historial
- `s`: Cambiar fuente de datos
- `e`: Exportar datos a CSV
