#include "ui_utils.h"
#include <string.h>
#include <math.h>
#include <libnotify/notify.h>
#include <stdlib.h>
#include <time.h>

// Estilo CSS para la interfaz
static const char *CSS_STYLE =
"@define-color bg_color #1E1E2E;\n"
"@define-color fg_color #CDD6F4;\n"
"@define-color accent_color #89B4FA;\n"
"@define-color warning_color #F9E2AF;\n"
"@define-color error_color #F38BA8;\n"
"@define-color success_color #A6E3A1;\n"
"\n"
".window {\n"
"  background-color: @bg_color;\n"
"  color: @fg_color;\n"
"  font-family: 'Inter', 'Roboto', Arial, sans-serif;\n"
"}\n"
"\n"
".header {\n"
"  background-color: rgba(30, 30, 46, 0.8);\n"
"  border-bottom: 1px solid rgba(108, 112, 134, 0.3);\n"
"  padding: 12px 16px;\n"
"}\n"
"\n"
".header-title {\n"
"  font-size: 18px;\n"
"  font-weight: 600;\n"
"  color: @accent_color;\n"
"}\n"
"\n"
".status-bar {\n"
"  background-color: rgba(30, 30, 46, 0.8);\n"
"  border-top: 1px solid rgba(108, 112, 134, 0.3);\n"
"  padding: 6px 12px;\n"
"  font-size: 11px;\n"
"  color: rgba(205, 214, 244, 0.7);\n"
"}\n"
"\n"
".card {\n"
"  background-color: rgba(49, 50, 68, 0.6);\n"
"  border-radius: 8px;\n"
"  padding: 16px;\n"
"  margin: 8px;\n"
"  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);\n"
"}\n"
"\n"
".card-title {\n"
"  font-size: 14px;\n"
"  font-weight: 600;\n"
"  margin-bottom: 12px;\n"
"  color: @accent_color;\n"
"}\n"
"\n"
".fee-value {\n"
"  font-size: 24px;\n"
"  font-weight: 700;\n"
"  margin: 4px 0;\n"
"}\n"
"\n"
".fee-label {\n"
"  font-size: 12px;\n"
"  color: rgba(205, 214, 244, 0.7);\n"
"  margin-bottom: 8px;\n"
"}\n"
"\n"
".price-up {\n"
"  color: @success_color;\n"
"}\n"
"\n"
".price-down {\n"
"  color: @error_color;\n"
"}\n"
"\n"
".notebook {\n"
"  background-color: transparent;\n"
"  border: none;\n"
"}\n"
"\n"
".notebook tab {\n"
"  padding: 8px 16px;\n"
"  background-color: rgba(49, 50, 68, 0.6);\n"
"  border: 1px solid rgba(108, 112, 134, 0.3);\n"
"  border-bottom: none;\n"
"  border-radius: 6px 6px 0 0;\n"
"  margin-right: 4px;\n"
"  color: @fg_color;\n"
"}\n"
"\n"
".notebook tab:checked {\n"
"  background-color: rgba(69, 71, 90, 0.8);\n"
"  border-bottom: 2px solid @accent_color;\n"
"}\n"
"\n"
".notebook tab:hover {\n"
"  background-color: rgba(88, 91, 112, 0.6);\n"
"}\n"
"\n"
".alert-row {\n"
"  padding: 8px;\n"
"  border-bottom: 1px solid rgba(108, 112, 134, 0.3);\n"
"}\n"
"\n"
".alert-active {\n"
"  background-color: rgba(166, 227, 161, 0.1);\n"
"}\n"
"\n"
".alert-triggered {\n"
"  background-color: rgba(243, 139, 168, 0.1);\n"
"  font-weight: bold;\n"
"}";

// Inicializa la interfaz de usuario
AppUI* ui_init(GtkApplication *app) {
    AppUI *ui = g_malloc0(sizeof(AppUI));
    if (!ui) return NULL;
    
    // Crear ventana principal
    ui->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(ui->window), "Bitcoin Fee Tracker Pro");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 900, 650);
    gtk_window_set_position(GTK_WINDOW(ui->window), GTK_WIN_POS_CENTER);
    
    // Configurar el tema oscuro
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-application-prefer-dark-theme", TRUE, NULL);
    
    // Aplicar estilos CSS personalizados
    ui->css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(ui->css_provider, CSS_STYLE, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(ui->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    // Crear el layout principal
    ui->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ui->window), ui->main_box);
    
    // Crear el encabezado
    ui->header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(ui->main_box), ui->header_box, FALSE, FALSE, 0);
    gtk_widget_set_name(ui->header_box, "header");
    
    // Título de la aplicación
    GtkWidget *title_label = gtk_label_new("BITCOIN FEE TRACKER PRO");
    gtk_widget_set_name(title_label, "header-title");
    gtk_box_pack_start(GTK_BOX(ui->header_box), title_label, FALSE, FALSE, 10);
    
    // Espaciador
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(ui->header_box), spacer, TRUE, TRUE, 0);
    
    // Botón de actualización manual
    GtkWidget *refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(refresh_btn, "Actualizar datos");
    gtk_box_pack_end(GTK_BOX(ui->header_box), refresh_btn, FALSE, FALSE, 5);
    
    // Contenedor principal
    ui->content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(ui->main_box), ui->content_box, TRUE, TRUE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ui->content_box), 10);
    
    // Crear el notebook (pestañas)
    ui->notebook = gtk_notebook_new();
    gtk_widget_set_name(ui->notebook, "notebook");
    gtk_box_pack_start(GTK_BOX(ui->content_box), ui->notebook, TRUE, TRUE, 0);
    
    // Pestaña de Resumen
    GtkWidget *summary_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(summary_box), 10);
    
    // Fila de tarjetas de tarifas
    GtkWidget *fee_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(summary_box), fee_box, FALSE, FALSE, 0);
    
    // Tarjeta de tarifa más rápida
    GtkWidget *fastest_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_name(fastest_card, "card");
    gtk_widget_set_size_request(fastest_card, 160, 120);
    gtk_box_pack_start(GTK_BOX(fee_box), fastest_card, TRUE, TRUE, 0);
    
    GtkWidget *fastest_label = gtk_label_new("MÁS RÁPIDO");
    gtk_widget_set_name(fastest_label, "card-title");
    gtk_box_pack_start(GTK_BOX(fastest_card), fastest_label, FALSE, FALSE, 0);
    
    ui->fee_label = gtk_label_new("--");
    gtk_widget_set_name(ui->fee_label, "fee-value");
    gtk_box_pack_start(GTK_BOX(fastest_card), ui->fee_label, TRUE, TRUE, 0);
    
    GtkWidget *fastest_desc = gtk_label_new("sat/vB");
    gtk_widget_set_name(fastest_desc, "fee-label");
    gtk_box_pack_start(GTK_BOX(fastest_card), fastest_desc, FALSE, FALSE, 0);
    
    // (Añadir más tarjetas de tarifas aquí...)
    
    // Fila de precios y mempool
    GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(summary_box), info_box, TRUE, TRUE, 0);
    
    // Tarjeta de precio
    GtkWidget *price_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_name(price_card, "card");
    gtk_box_pack_start(GTK_BOX(info_box), price_card, TRUE, TRUE, 0);
    
    GtkWidget *price_title = gtk_label_new("PRECIO BTC");
    gtk_widget_set_name(price_title, "card-title");
    gtk_box_pack_start(GTK_BOX(price_card), price_title, FALSE, FALSE, 0);
    
    ui->price_label = gtk_label_new("Cargando...");
    gtk_widget_set_name(ui->price_label, "fee-value");
    gtk_box_pack_start(GTK_BOX(price_card), ui->price_label, TRUE, TRUE, 0);
    
    // Tarjeta de mempool
    GtkWidget *mempool_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_name(mempool_card, "card");
    gtk_box_pack_start(GTK_BOX(info_box), mempool_card, TRUE, TRUE, 0);
    
    GtkWidget *mempool_title = gtk_label_new("MEMPOOL");
    gtk_widget_set_name(mempool_title, "card-title");
    gtk_box_pack_start(GTK_BOX(mempool_card), mempool_title, FALSE, FALSE, 0);
    
    ui->mempool_label = gtk_label_new("Transacciones: --\nTamaño: -- MB\nTarifa media: -- sat/vB");
    gtk_label_set_line_wrap(GTK_LABEL(ui->mempool_label), TRUE);
    gtk_widget_set_halign(ui->mempool_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(mempool_card), ui->mempool_label, TRUE, TRUE, 0);
    
    // Añadir pestaña de resumen
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), summary_box, gtk_label_new("Resumen"));
    
    // Pestaña de gráficos
    ui->charts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ui->charts_box), 10);
    
    // Inicializar gráficos
    ui->fee_chart = chart_config_new(ui->charts_box, "Historial de Tarifas (sat/vB)");
    chart_add_series(ui->fee_chart, "Tarifa más rápida", NULL, TRUE);
    
    ui->price_chart = chart_config_new(ui->charts_box, "Precio de Bitcoin (USD)");
    chart_add_series(ui->price_chart, "Precio USD", NULL, TRUE);
    
    ui->mempool_chart = chart_config_new(ui->charts_box, "Tamaño de la Mempool (MB)");
    chart_add_series(ui->mempool_chart, "Tamaño MB", NULL, FALSE);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), ui->charts_box, gtk_label_new("Gráficos"));
    
    // Pestaña de alertas
    ui->alerts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ui->alerts_box), 10);
    
    // Crear modelo para la lista de alertas
    ui->alerts_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_STRING);
    ui->alerts_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(ui->alerts_store)));
    
    // Configurar columnas
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // Columna de tipo
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Tipo", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(ui->alerts_view, column);
    
    // Columna de condición
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Condición", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(ui->alerts_view, column);
    
    // Columna de valor
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Valor", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(ui->alerts_view, column);
    
    // Columna de estado
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Estado", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(ui->alerts_view, column);
    
    // Añadir scroll a la vista de árbol
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                  GTK_POLICY_AUTOMATIC, 
                                  GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(ui->alerts_view));
    gtk_box_pack_start(GTK_BOX(ui->alerts_box), scrolled, TRUE, TRUE, 0);
    
    // Botón para añadir alerta
    GtkWidget *add_alert_btn = gtk_button_new_with_label("Añadir Alerta");
    gtk_box_pack_start(GTK_BOX(ui->alerts_box), add_alert_btn, FALSE, FALSE, 5);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), ui->alerts_box, gtk_label_new("Alertas"));
    
    // Pestaña de configuración
    ui->config_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ui->config_box), 10);
    
    // Añadir controles de configuración aquí...
    
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), ui->config_box, gtk_label_new("Configuración"));
    
    // Barra de estado
    ui->footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(ui->footer_box, "status-bar");
    gtk_box_pack_end(GTK_BOX(ui->main_box), ui->footer_box, FALSE, FALSE, 0);
    
    ui->status_label = gtk_label_new("Conectando a la red Bitcoin...");
    gtk_box_pack_start(GTK_BOX(ui->footer_box), ui->status_label, TRUE, TRUE, 5);
    
    // Mostrar todo
    gtk_widget_show_all(ui->window);
    
    return ui;
}

// Limpia los recursos de la interfaz de usuario
void ui_cleanup(AppUI *ui) {
    if (!ui) return;
    
    // Liberar gráficos
    if (ui->fee_chart) chart_config_free(ui->fee_chart);
    if (ui->price_chart) chart_config_free(ui->price_chart);
    if (ui->mempool_chart) chart_config_free(ui->mempool_chart);
    
    // Liberar proveedor CSS
    if (ui->css_provider) {
        gtk_style_context_remove_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(ui->css_provider)
        );
        g_object_unref(ui->css_provider);
    }
    
    // Destruir ventana
    if (ui->window) {
        gtk_widget_destroy(ui->window);
    }
    
    // Liberar memoria
    g_free(ui);
}
void ui_update_fee_info(AppUI *ui, double fastest, double halfHour, double hour, double economy, double minimum) {
    if (!ui || !ui->fee_label) return;
    
    (void)halfHour; // Unused parameter
    (void)hour;     // Unused parameter
    (void)economy;  // Unused parameter
    (void)minimum;  // Unused parameter
    
    char text[512];
    snprintf(text, sizeof(text),
             "<b>Tarifas de transacción (sat/vB):</b>\n"
             "• Rápido (10 min): <span foreground='#F38BA8'>%.1f</span>\n"
             "• Media hora: <span foreground='#F9E2AF'>%.1f</span>\n"
             "• 1 hora: <span foreground='#A6E3A1'>%.1f</span>\n"
             "• Económico: <span foreground='#89B4FA'>%.1f</span>\n"
             "• Mínimo: <span>%.1f</span>",
             fastest, halfHour, hour, economy, minimum);
    
    gtk_label_set_markup(GTK_LABEL(ui->fee_label), text);
    
    // Update fee chart
    if (ui->fee_chart) {
        // Add data to chart series
        time_t now = time(NULL);
        chart_add_point(ui->fee_chart, "Rápido", now, fastest);
        chart_add_point(ui->fee_chart, "Media Hora", now, halfHour);
        chart_add_point(ui->fee_chart, "1 Hora", now, hour);
        chart_add_point(ui->fee_chart, "Económico", now, economy);
        
        // Redraw the chart
        if (ui->fee_chart->drawing_area) {
            gtk_widget_queue_draw(ui->fee_chart->drawing_area);
        }
    }
    
    // Actualizar estado
    char status[256];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(status, sizeof(status), "Actualizado: %H:%M:%S", tm_info);
    gtk_label_set_text(GTK_LABEL(ui->status_label), status);
}

// Actualiza la información de precios en la interfaz
void ui_update_price_info(AppUI *ui, double usd, double eur, double change24h) {
    if (!ui || !ui->price_label) return;
    
    char text[256];
    const char *change_class = (change24h >= 0) ? "price-up" : "price-down";
    const char *arrow = (change24h >= 0) ? "⬆" : "⬇";
    
    snprintf(text, sizeof(text),
             "<span font_desc='24' weight='bold'>$%.2f</span>\n"
             "<span font_desc='12'>€%.2f • <span class='%s'>%s %.2f%%</span></span>",
             usd, eur, change_class, arrow, fabs(change24h));
    
    gtk_label_set_markup(GTK_LABEL(ui->price_label), text);
    
    // Actualizar gráfico de precios
    chart_add_data(ui->price_chart, 0, usd);
}

// Actualiza la información de la mempool en la interfaz
void ui_update_mempool_info(AppUI *ui, int count, int size, double total_fee, double avg_fee) {
    if (!ui || !ui->mempool_label) return;
    
    (void)total_fee; // Unused parameter
    
    double size_mb = size / (1024.0 * 1024.0);
    
    char text[512];
    snprintf(text, sizeof(text),
             "<b>Mempool Info:</b>\n"
             "• Transacciones: %d\n"
             "• Tamaño: %.2f MB\n"
             "• Tarifa media: %.1f sat/vB",
             count, size_mb, avg_fee);
    
    gtk_label_set_markup(GTK_LABEL(ui->mempool_label), text);
    
    // Update mempool chart
    if (ui->mempool_chart) {
        time_t now = time(NULL);
        chart_add_point(ui->mempool_chart, "Transacciones", now, count);
        chart_add_point(ui->mempool_chart, "Tamaño (MB)", now, size_mb);
        chart_add_point(ui->mempool_chart, "Tarifa Media", now, avg_fee);
        
        if (ui->mempool_chart->drawing_area) {
            gtk_widget_queue_draw(ui->mempool_chart->drawing_area);
        }
    }
    // Chart updates complete
}

// Añade una alerta a la lista
void ui_add_alert(AppUI *ui, const char *type, const char *condition, double value, const char *status) {
    if (!ui || !ui->alerts_store) return;
    
    GtkTreeIter iter;
    gtk_list_store_append(ui->alerts_store, &iter);
    gtk_list_store_set(ui->alerts_store, &iter,
                      0, type,
                      1, condition,
                      2, value,
                      3, status,
                      -1);
}
