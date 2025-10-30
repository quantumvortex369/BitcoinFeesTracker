#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <gtk/gtk.h>
#include "chart_utils.h"

// Estructura para mantener referencias a los widgets de la UI
typedef struct {
    // Ventana principal
    GtkWidget *window;
    
    // Contenedores
    GtkWidget *main_box;
    GtkWidget *header_box;
    GtkWidget *content_box;
    GtkWidget *footer_box;
    
    // Widgets de información
    GtkWidget *fee_label;
    GtkWidget *price_label;
    GtkWidget *mempool_label;
    GtkWidget *status_label;
    
    // Pestañas
    GtkWidget *notebook;
    
    // Pestaña de gráficos
    GtkWidget *charts_box;
    ChartConfig *fee_chart;
    ChartConfig *price_chart;
    ChartConfig *mempool_chart;
    
    // Pestaña de alertas
    GtkWidget *alerts_box;
    GtkListStore *alerts_store;
    GtkTreeView *alerts_view;
    
    // Pestaña de configuración
    GtkWidget *config_box;
    
    // Datos de la aplicación
    GtkCssProvider *css_provider;
    
} AppUI;

// Funciones públicas
AppUI* ui_init(GtkApplication *app);
void ui_cleanup(AppUI *ui);
void ui_update_fee_info(AppUI *ui, double fastest, double halfHour, double hour, double economy, double minimum);
void ui_update_price_info(AppUI *ui, double usd, double eur, double change24h);
void ui_update_mempool_info(AppUI *ui, int count, int size, double total_fee, double avg_fee);
void ui_show_notification(const char *title, const char *message, const char *icon);
void ui_add_alert(AppUI *ui, const char *type, const char *condition, double value, const char *status);

#endif // UI_UTILS_H
