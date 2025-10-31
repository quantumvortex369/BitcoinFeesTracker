# Bitcoin Fee Visualizer

Visualizador en tiempo real de las tarifas de transacción de Bitcoin con soporte para múltiples fuentes de datos.

## Gas Fee Tracker

Una aplicación para rastrear y visualizar las tarifas de transacción (gas fees) de múltiples criptomonedas en tiempo real.

## Características

- Monitoreo en tiempo real de tarifas de transacción
- Soporte para múltiples criptomonedas (Bitcoin, Ethereum, etc.)
- Gráficos históricos interactivos
- Notificaciones personalizables
- Interfaz de usuario moderna y personalizable
- Temas claro/oscuro

## Requisitos

- GTK+ 3.0 o superior
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
