#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <time.h>
#include "chart_utils.h"

// Tema de la aplicación
typedef enum {
    THEME_LIGHT,
    THEME_DARK,
    THEME_SYSTEM
} ThemePreference;

// Monedas soportadas
typedef enum {
    COIN_BTC,
    COIN_ETH,
    COIN_LTC,
    COIN_COUNT
} Cryptocurrency;

// Estructura para almacenar los widgets de la interfaz
typedef struct {
    // Ventana principal
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *header_bar;
    GtkWidget *header_box;     // Contenedor de la barra superior
    GtkWidget *content_box;    // Contenedor principal del contenido
    GtkWidget *drawing_area;
    GtkWidget *status_bar;
    GtkWidget *stack;
    GtkCssProvider *css_provider;
    GtkWidget *charts_box;     // Contenedor de gráficos
    GtkWidget *alerts_view;    // Vista de alertas
    
    // Barra de herramientas
    GtkWidget *menu_button;
    GtkWidget *refresh_button;
    GtkWidget *settings_button;
    GtkWidget *theme_switch;
    GtkWidget *currency_combo;
    GtkWidget *notebook;
    GtkWidget *config_box;
    GtkWidget *footer_box;
    
    // Paneles
    GtkWidget *dashboard_panel;
    GtkWidget *settings_panel;
    GtkWidget *history_panel;
    
    // Etiquetas
    GtkWidget *status_label;
    GtkWidget *price_label;
    GtkWidget *mempool_label;
    GtkWidget *fee_label;
    GtkWidget *fee_labels[5];  // Para las diferentes tarifas
    GtkWidget *alerts_box;     // Contenedor de alertas
    
    // Gráficos
    ChartConfig *fee_chart;
    ChartConfig *price_chart;
    ChartConfig *mempool_chart;
    
    // Almacenamiento de datos
    GtkListStore *fee_history_store;
    GtkTreeModelFilter *fee_history_filter;
    GtkListStore *alerts_store;
    
    // Configuración
    ThemePreference current_theme;
    Cryptocurrency current_currency;
    bool notifications_enabled;
    int refresh_interval; // en segundos
    
} AppUI;

/**
 * Inicializa la interfaz de usuario
 * 
 * @param app Aplicación GTK
 * @return Puntero a la estructura de la interfaz de usuario
 */
AppUI* ui_init(GtkApplication *app);

/**
 * Libera los recursos de la interfaz de usuario
 * 
 * @param ui Puntero a la estructura de la interfaz de usuario
 */
void ui_cleanup(AppUI *ui);

/**
 * Actualiza la información de tarifas en la interfaz
 */
void ui_update_fee_info(
    AppUI *ui, 
    double fastest, 
    double halfHour, 
    double hour, 
    double economy, 
    double minimum
);

/**
 * Actualiza la información de precios en la interfaz
 */
void ui_update_price_info(
    AppUI *ui, 
    double usd, 
    double eur, 
    double change24h
);

/**
 * Actualiza la información del mempool en la interfaz
 */
void ui_update_mempool_info(
    AppUI *ui, 
    int count, 
    int size, 
    double total_fee, 
    double avg_fee
);

/**
 * Muestra una notificación en el sistema
 */
void ui_show_notification(
    const char *title, 
    const char *message, 
    const char *icon
);

/**
 * Añade una alerta a la interfaz
 */
void ui_add_alert(
    AppUI *ui, 
    const char *type, 
    const char *condition, 
    double value, 
    const char *status
);

/**
 * Actualiza el tema de la aplicación
 */
void ui_update_theme(AppUI *ui, ThemePreference theme);

/**
 * Actualiza la moneda seleccionada
 */
void ui_update_currency(AppUI *ui, Cryptocurrency currency);

/**
 * Carga la configuración de la aplicación
 */
void ui_load_settings(AppUI *ui);

/**
 * Guarda la configuración de la aplicación
 */
void ui_save_settings(const AppUI *ui);

#endif // UI_UTILS_H
